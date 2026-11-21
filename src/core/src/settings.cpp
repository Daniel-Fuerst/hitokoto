// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/settings.hpp"

#include <set>
#include <system_error>
#include <variant>

namespace hitokoto::core {
namespace {

using Value = std::variant<std::string, bool>;

bool isKeyChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

bool isBlank(char c) {
    return c == ' ' || c == '\t';
}

char toLowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

std::string_view trimBlanks(std::string_view sv) {
    while (!sv.empty() && isBlank(sv.front())) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && isBlank(sv.back())) {
        sv.remove_suffix(1);
    }
    return sv;
}

std::string toLowerAsciiString(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        out.push_back(toLowerAscii(c));
    }
    return out;
}

bool isKnownSection(std::string_view name) {
    return name == "appearance" || name == "editor";
}

bool hasControlCharacters(std::string_view s) {
    for (char c : s) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7f) {
            return true;
        }
    }
    return false;
}

std::string escapeTomlString(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

struct SettingKeyDef {
    std::string_view section;
    std::string_view key;
    std::string_view canonical;
    bool isBool;
};

constexpr SettingKeyDef kKeys[] = {
    {"appearance", "theme", "appearance.theme", false},
    {"editor", "smooth-scrolling", "editor.smooth-scrolling", true},
    {"editor", "front-matter-as-properties", "editor.front-matter-as-properties", true},
    {"editor", "report-blank-lines-added", "editor.report-blank-lines-added", true},
};

const SettingKeyDef* findSettingKey(std::string_view section, std::string_view key) {
    for (const auto& def : kKeys) {
        if (def.key == key && (section.empty() || section == def.section)) {
            return &def;
        }
    }
    return nullptr;
}

/// One line of config.toml as positions in the whole text (S039, changing the theme).
struct SourceLine {
    std::size_t start = 0;       ///< first byte of the line, after a byte order mark on line 1
    std::size_t contentEnd = 0;  ///< end of the line without "\n" or "\r\n"
    std::size_t end = 0;         ///< start of the next line
    std::size_t number = 0;      ///< 1-based
    std::size_t valueStart = 0;  ///< for the theme key: first byte of its value
};

enum class SourceScope { Root, Appearance, Other };

SourceLine sourceLineAt(std::string_view source, std::size_t pos, std::size_t number) {
    SourceLine line;
    line.number = number;
    line.start = (pos == 0 && source.starts_with("\xEF\xBB\xBF")) ? 3 : pos;
    const std::size_t nl = source.find('\n', pos);
    line.end = nl == std::string_view::npos ? source.size() : nl + 1;
    line.contentEnd = nl == std::string_view::npos ? source.size() : nl;
    if (line.contentEnd > line.start && source[line.contentEnd - 1] == '\r') {
        --line.contentEnd;
    }
    return line;
}

std::size_t skipBlanksFrom(std::string_view s, std::size_t i) {
    while (i < s.size() && isBlank(s[i])) {
        ++i;
    }
    return i;
}

bool isTomlBareKeyChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/// The first part of a TOML key (TOML 1.0 "Keys": bare, "basic" or 'literal') at `i` after blanks, without its
/// quotes; escapes in a quoted part are not decoded. Empty when there is none.
std::string_view firstTomlKeyPart(std::string_view s, std::size_t i) {
    i = skipBlanksFrom(s, i);
    if (i >= s.size()) {
        return {};
    }
    if (s[i] == '"' || s[i] == '\'') {
        const char quote = s[i];
        std::size_t j = i + 1;
        while (j < s.size() && s[j] != quote) {
            j += (quote == '"' && s[j] == '\\') ? std::size_t{2} : std::size_t{1};
        }
        return j < s.size() ? s.substr(i + 1, j - i - 1) : std::string_view{};
    }
    std::size_t j = i;
    while (j < s.size() && isTomlBareKeyChar(s[j])) {
        ++j;
    }
    return s.substr(i, j - i);
}

/// One past the closing delimiter of the multi-line string opened at `open` (TOML 1.0 "String": `"""` with
/// escapes or `'''` without; one or two quotes may stand just inside the closing delimiter).
std::optional<std::size_t> multiLineStringEnd(std::string_view source, std::size_t open) {
    const char quote = source[open];
    std::size_t i = open + 3;
    while (i < source.size()) {
        if (quote == '"' && source[i] == '\\') {
            i += 2;
            continue;
        }
        if (source[i] != quote) {
            ++i;
            continue;
        }
        std::size_t run = 0;
        while (i + run < source.size() && source[i + run] == quote) {
            ++run;
        }
        if (run >= 3) {
            return i + run;
        }
        i += run;
    }
    return std::nullopt;
}

/// One past the closing quote of the single-line string opened at `open`; the end of the line when it is not
/// closed there.
std::size_t singleLineStringEnd(std::string_view source, std::size_t open, std::size_t contentEnd) {
    const char quote = source[open];
    std::size_t i = open + 1;
    while (i < contentEnd) {
        if (source[i] == quote) {
            return i + 1;
        }
        i += (quote == '"' && source[i] == '\\') ? std::size_t{2} : std::size_t{1};
    }
    return contentEnd;
}

class Parser {
public:
    explicit Parser(std::string_view src) : src_(src) {
        if (src_.size() >= 3 && src_[0] == '\xEF' && src_[1] == '\xBB' && src_[2] == '\xBF') {
            src_.remove_prefix(3);
        }
    }

    SettingsLoadResult run() {
        std::size_t lineNo = 0;
        std::size_t pos = 0;
        while (pos <= src_.size()) {
            ++lineNo;
            const std::size_t nl = src_.find('\n', pos);
            std::string_view line = src_.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            parseLine(line, lineNo);
            if (nl == std::string_view::npos) {
                break;
            }
            pos = nl + 1;
        }
        return result_;
    }

private:
    void error(std::size_t line, std::size_t col0, std::string msg) {
        result_.diagnostics.push_back({line, col0 + 1, std::move(msg)});
    }

    static std::size_t skipBlanks(std::string_view l, std::size_t i) {
        while (i < l.size() && isBlank(l[i])) {
            ++i;
        }
        return i;
    }

    bool expectLineEnd(std::string_view l, std::size_t i, std::size_t lineNo) {
        i = skipBlanks(l, i);
        if (i < l.size() && l[i] != '#') {
            error(lineNo, i, "unexpected text; expected end of line or a comment");
            return false;
        }
        return true;
    }

    void applyKeyValue(const SettingKeyDef& def, const Value& value, std::size_t lineNo, std::size_t valueStart) {
        if (def.isBool) {
            const auto* b = std::get_if<bool>(&value);
            if (b == nullptr) {
                error(lineNo, valueStart, std::string(def.canonical) + ": expected true or false");
                return;
            }
            if (def.key == "smooth-scrolling") {
                result_.settings.smoothScrolling = *b;
            } else if (def.key == "front-matter-as-properties") {
                result_.settings.frontMatterAsProperties = *b;
            } else if (def.key == "report-blank-lines-added") {
                result_.settings.reportBlankLinesAdded = *b;
            }
            return;
        }

        const auto* s = std::get_if<std::string>(&value);
        if (s == nullptr) {
            error(lineNo, valueStart, std::string(def.canonical) + ": expected a string in double quotes");
            return;
        }
        if (def.key == "theme") {
            result_.settings.theme = *s;
        }
    }

    void parseLine(std::string_view l, std::size_t lineNo) {
        std::size_t i = skipBlanks(l, 0);
        if (i >= l.size() || l[i] == '#') {
            return;
        }
        if (l[i] == '[') {
            const std::size_t close = l.find(']', i);
            if (close == std::string_view::npos) {
                error(lineNo, i, "missing ']' after section name");
                return;
            }
            const std::string_view name = l.substr(i + 1, close - i - 1);
            if (!isKnownSection(name)) {
                error(lineNo, i + 1, "unknown section [" + std::string(name) + "]");
                section_ = "\x01";  // keys below are covered by the unknown-section diagnostic
                return;
            }
            section_ = std::string(name);
            expectLineEnd(l, close + 1, lineNo);
            return;
        }

        const std::size_t keyStart = i;
        while (i < l.size() && isKeyChar(l[i])) {
            ++i;
        }
        if (i == keyStart) {
            error(lineNo, i, "expected a key, a [section] or a comment");
            return;
        }
        const std::string_view key = l.substr(keyStart, i - keyStart);
        i = skipBlanks(l, i);
        if (i >= l.size() || l[i] != '=') {
            error(lineNo, i, "expected '=' after key '" + std::string(key) + "'");
            return;
        }
        i = skipBlanks(l, i + 1);
        const std::size_t valueStart = i;
        Value value;
        if (i < l.size() && l[i] == '"') {
            std::string s;
            ++i;
            bool closed = false;
            while (i < l.size()) {
                const char c = l[i++];
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\\' && i < l.size() && (l[i] == '"' || l[i] == '\\')) {
                    s.push_back(l[i++]);
                } else {
                    s.push_back(c);
                }
            }
            if (!closed) {
                error(lineNo, valueStart, "missing closing '\"'");
                return;
            }
            value = std::move(s);
        } else {
            std::size_t end = i;
            while (end < l.size() && !isBlank(l[end]) && l[end] != '#') {
                ++end;
            }
            const std::string_view text = l.substr(i, end - i);
            if (text == "true") {
                value = true;
            } else if (text == "false") {
                value = false;
            } else {
                error(lineNo, valueStart,
                      text.empty() ? "expected a value" : "expected a boolean or a string in double quotes");
                return;
            }
            i = end;
        }

        if (!expectLineEnd(l, i, lineNo)) {
            return;
        }
        if (section_ == "\x01") {
            return;
        }

        const SettingKeyDef* def = findSettingKey(section_, key);
        const std::string full = section_.empty() ? std::string(key) : section_ + "." + std::string(key);
        if (def == nullptr) {
            error(lineNo, keyStart, "unknown key '" + full + "'");
            return;
        }
        if (!seen_.emplace(def->canonical).second) {
            error(lineNo, keyStart, "duplicate key '" + full + "'");
            return;
        }
        applyKeyValue(*def, value, lineNo, valueStart);
    }

    std::string_view src_;
    SettingsLoadResult result_;
    std::string section_;
    std::set<std::string> seen_;
};

}  // namespace

std::string SettingsDiagnostic::toString() const {
    if (line == 0) {
        return message;
    }
    return "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message;
}

SettingsLoadResult parseSettings(std::string_view source) {
    return Parser(source).run();
}

std::string serializeSettings(const Settings& settings) {
    std::string out;
    out += "# hitokoto configuration\n";
    if (!settings.theme.empty() && !hasControlCharacters(settings.theme)) {
        out += "\n[appearance]\n";
        out += "theme = \"";
        out += escapeTomlString(settings.theme);
        out += "\"\n";
    }
    out += "\n[editor]\n";
    out += std::string("smooth-scrolling = ") + (settings.smoothScrolling ? "true\n" : "false\n");
    out += std::string("front-matter-as-properties = ") + (settings.frontMatterAsProperties ? "true\n" : "false\n");
    out += std::string("report-blank-lines-added = ") + (settings.reportBlankLinesAdded ? "true\n" : "false\n");
    return out;
}

SettingsLoadResult loadSettingsFile(const std::filesystem::path& path) noexcept {
    SettingsLoadResult result;
    if (path.empty()) {
        return result;
    }
    try {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) {
            result.diagnostics.push_back({0, 0, ec.message()});
            return result;
        }
        if (!exists) {
            return result;
        }
        const bool isRegular = std::filesystem::is_regular_file(path, ec);
        if (ec) {
            result.diagnostics.push_back({0, 0, ec.message()});
            return result;
        }
        if (!isRegular) {
            result.diagnostics.push_back({0, 0, "not a regular file"});
            return result;
        }
        constexpr std::uintmax_t kMaxSettingsFileSize = 64ULL * 1024ULL;
        const std::uintmax_t size = std::filesystem::file_size(path, ec);
        if (ec) {
            result.diagnostics.push_back({0, 0, ec.message()});
            return result;
        }
        if (size > kMaxSettingsFileSize) {
            result.diagnostics.push_back({0, 0, "file exceeds maximum allowed size"});
            return result;
        }
        const ReadResult read = readFile(path);
        if (!read.bytes) {
            result.diagnostics.push_back({0, 0, read.error ? read.error->message : "cannot read file"});
            return result;
        }
        return parseSettings(*read.bytes);
    } catch (const std::exception& e) {
        result.settings = Settings{};
        result.diagnostics = {{0, 0, e.what()}};
        return result;
    } catch (...) {
        result.settings = Settings{};
        result.diagnostics = {{0, 0, "unknown error loading settings file"}};
        return result;
    }
}

std::optional<IoError> saveSettingsFile(const std::filesystem::path& path, const Settings& settings) {
    if (path.empty()) {
        return IoError{"settings path is empty"};
    }
    if (hasControlCharacters(settings.theme)) {
        return IoError{"theme name contains control characters"};
    }
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return IoError{"cannot create settings directory: " + ec.message()};
        }
    }
    return writeFileAtomically(path, serializeSettings(settings));
}

SettingsSourceUpdate updateThemeInSettingsSource(std::string_view source, std::string_view newTheme) {
    const auto refuse = [](std::string message) { return SettingsSourceUpdate{{}, IoError{std::move(message)}}; };
    if (hasControlCharacters(newTheme)) {
        return refuse("theme name contains control characters");
    }
    const std::string_view defaultLe = source.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
    const std::string quoted = std::string("\"") + escapeTomlString(newTheme) + "\"";

    // Find the key the parser reads as the theme, line by line as the parser does.
    std::optional<SourceLine> themeLine;
    std::optional<SourceLine> appearanceHeader;
    std::optional<std::size_t> foreignLine;
    SourceScope scope = SourceScope::Root;
    std::size_t lineNo = 0;
    for (std::size_t pos = 0; pos < source.size() && !themeLine;) {
        const SourceLine line = sourceLineAt(source, pos, ++lineNo);
        pos = line.end;
        const std::string_view content = source.substr(line.start, line.contentEnd - line.start);
        const std::size_t i = skipBlanksFrom(content, 0);
        if (i >= content.size() || content[i] == '#') {
            continue;
        }
        if (content[i] == '[') {
            const std::size_t close = content.find(']', i);
            const bool canonical =
                close != std::string_view::npos && content.substr(i + 1, close - i - 1) == "appearance";
            if (close != std::string_view::npos) {
                scope = canonical ? SourceScope::Appearance : SourceScope::Other;
            }
            if (canonical && !appearanceHeader) {
                appearanceHeader = line;
            }
            const std::size_t open = content.size() > i + 1 && content[i + 1] == '[' ? i + 2 : i + 1;
            if (!canonical && firstTomlKeyPart(content, open) == "appearance" && !foreignLine) {
                foreignLine = lineNo;
            }
            continue;
        }
        if (scope == SourceScope::Other) {
            continue;
        }
        std::size_t k = i;
        while (k < content.size() && isKeyChar(content[k])) {
            ++k;
        }
        const std::size_t eq = skipBlanksFrom(content, k);
        if (content.substr(i, k - i) == "theme" && eq < content.size() && content[eq] == '=') {
            themeLine = line;
            themeLine->valueStart = line.start + skipBlanksFrom(content, eq + 1);
            continue;
        }
        const std::string_view part = firstTomlKeyPart(content, i);
        if (((part == "appearance" && scope == SourceScope::Root) || part == "theme") && !foreignLine) {
            foreignLine = lineNo;
        }
    }

    std::string changed;
    if (themeLine) {
        const std::size_t valueStart = themeLine->valueStart;
        std::size_t valueEnd = valueStart;
        const std::string_view rest = source.substr(valueStart, themeLine->contentEnd - valueStart);
        const std::string where = "line " + std::to_string(themeLine->number) + ": ";
        if (rest.starts_with(R"(""")") || rest.starts_with("'''")) {
            const std::optional<std::size_t> close = multiLineStringEnd(source, valueStart);
            if (!close) {
                return refuse(where + "the multi-line string of theme is not closed");
            }
            valueEnd = *close;
        } else if (!rest.empty() && (rest.front() == '"' || rest.front() == '\'')) {
            valueEnd = singleLineStringEnd(source, valueStart, themeLine->contentEnd);
        } else if (!rest.empty() && (rest.front() == '[' || rest.front() == '{')) {
            return refuse(where + "the value of theme is an array or a table");
        } else {
            while (valueEnd < themeLine->contentEnd && !isBlank(source[valueEnd]) && source[valueEnd] != '#') {
                ++valueEnd;
            }
        }
        changed.reserve(source.size() + quoted.size() + 1);
        changed.append(source.substr(0, valueStart));
        if (source[valueStart - 1] == '=') {
            changed.push_back(' ');
        }
        changed.append(quoted);
        changed.append(source.substr(valueEnd));
    } else if (foreignLine) {
        return refuse("line " + std::to_string(*foreignLine) +
                      " sets the theme or [appearance] in a form hitokoto cannot change (a dotted or quoted key, or a "
                      "table header with spaces or quotes)");
    } else if (appearanceHeader) {
        const bool hasLineEnd = appearanceHeader->end > appearanceHeader->contentEnd;
        const std::string_view headerLe =
            hasLineEnd
                ? source.substr(appearanceHeader->contentEnd, appearanceHeader->end - appearanceHeader->contentEnd)
                : defaultLe;
        changed.reserve(source.size() + quoted.size() + 16);
        changed.append(source.substr(0, appearanceHeader->end));
        if (!hasLineEnd) {
            changed.append(defaultLe);
        }
        changed.append("theme = ");
        changed.append(quoted);
        changed.append(headerLe);
        changed.append(source.substr(appearanceHeader->end));
    } else {
        changed.reserve(source.size() + quoted.size() + 32);
        changed.append(source);
        if (!changed.empty() && changed.back() != '\n') {
            changed.append(defaultLe);
        }
        if (!changed.empty()) {
            changed.append(defaultLe);
        }
        changed.append("[appearance]");
        changed.append(defaultLe);
        changed.append("theme = ");
        changed.append(quoted);
        changed.append(defaultLe);
    }

    // The text is used only if the parser reads exactly this change from it.
    const SettingsLoadResult before = parseSettings(source);
    const SettingsLoadResult after = parseSettings(changed);
    Settings expected = before.settings;
    expected.theme = std::string(newTheme);
    if (!(after.settings == expected) || after.diagnostics.size() > before.diagnostics.size()) {
        return refuse("changing the theme alone would not give the new theme with all other settings unchanged");
    }
    return SettingsSourceUpdate{std::move(changed), std::nullopt};
}

std::optional<IoError> updateThemeSettingInFile(const std::filesystem::path& path, std::string_view theme) {
    if (path.empty()) {
        return IoError{"settings path is empty"};
    }
    if (hasControlCharacters(theme)) {
        return IoError{"theme name contains control characters"};
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        return IoError{ec.message()};
    }
    if (!exists) {
        Settings def;
        def.theme = std::string(theme);
        return saveSettingsFile(path, def);
    }

    const bool isRegular = std::filesystem::is_regular_file(path, ec);
    if (ec || !isRegular) {
        return IoError{"not a regular file"};
    }
    constexpr std::uintmax_t kMaxSettingsFileSize = 64ULL * 1024ULL;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxSettingsFileSize) {
        return IoError{"file exceeds maximum allowed size"};
    }

    const ReadResult read = readFile(path);
    if (!read.bytes) {
        return read.error ? *read.error : IoError{"cannot read file"};
    }

    const SettingsSourceUpdate updated = updateThemeInSettingsSource(*read.bytes, theme);
    if (updated.error) {
        return updated.error;
    }
    return writeFileAtomically(path, updated.source);
}

std::optional<IoError> updateEditorBoolSettingInFile(const std::filesystem::path& path, std::string_view key,
                                                     bool value) {
    if (path.empty()) {
        return IoError{"settings path is empty"};
    }
    if (key != "smooth-scrolling" && key != "front-matter-as-properties" && key != "report-blank-lines-added") {
        return IoError{"unknown editor setting"};
    }

    const ReadResult read = readFile(path);
    std::string source;
    if (read.bytes) {
        source = *read.bytes;
    } else if (std::filesystem::exists(path) && read.error) {
        return *read.error;
    }

    const std::string_view lineEnd = source.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
    const std::string valueText = value ? "true" : "false";
    bool inEditor = false;
    bool sawEditor = false;
    bool found = false;
    std::size_t editorEnd = source.size();
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const std::size_t nl = source.find('\n', pos);
        const std::size_t lineFinish = nl == std::string_view::npos ? source.size() : nl;
        std::string line = source.substr(pos, lineFinish - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] == '[') {
            const std::size_t close = line.find(']', first);
            if (close != std::string::npos) {
                const std::string name = line.substr(first + 1, close - first - 1);
                inEditor = (name == "editor");
                if (inEditor) {
                    sawEditor = true;
                    editorEnd = source.size();
                } else if (sawEditor && editorEnd == source.size()) {
                    editorEnd = pos;
                }
            }
        } else if (inEditor && first != std::string::npos) {
            const std::size_t equals = line.find('=');
            if (equals != std::string::npos) {
                std::string lineKey = line.substr(first, equals - first);
                while (!lineKey.empty() && (lineKey.back() == ' ' || lineKey.back() == '\t')) {
                    lineKey.pop_back();
                }
                if (lineKey == key) {
                    const std::size_t afterEquals = equals + 1;
                    std::size_t comment = line.find('#', afterEquals);
                    std::size_t valueEnd = comment == std::string::npos ? line.size() : comment;
                    while (valueEnd > afterEquals && (line[valueEnd - 1] == ' ' || line[valueEnd - 1] == '\t')) {
                        --valueEnd;
                    }
                    std::size_t valueStart = afterEquals;
                    while (valueStart < valueEnd && (line[valueStart] == ' ' || line[valueStart] == '\t')) {
                        ++valueStart;
                    }
                    const std::string leading = line.substr(0, afterEquals);
                    const std::string separator =
                        valueStart > afterEquals ? std::string(valueStart - afterEquals, ' ') : std::string(" ");
                    const std::string suffix = comment == std::string::npos ? std::string() : line.substr(comment);
                    const std::string changedLine =
                        leading + separator + valueText + (comment == std::string::npos ? std::string() : " " + suffix);
                    const std::size_t replaceLength = (nl == std::string::npos ? lineFinish - pos : nl - pos + 1);
                    const std::string replacement =
                        changedLine + (nl == std::string::npos ? std::string() : std::string(lineEnd));
                    source.replace(pos, replaceLength, replacement);
                    found = true;
                    break;
                }
            }
        }
        if (nl == std::string::npos) {
            break;
        }
        pos = nl + 1;
    }

    if (!found) {
        const std::string entry = std::string(key) + " = " + valueText + std::string(lineEnd);
        if (sawEditor) {
            if (editorEnd == source.size()) {
                if (!source.empty() && source.back() != '\n' && source.back() != '\r') {
                    source += lineEnd;
                }
                source += entry;
            } else {
                source.insert(editorEnd, entry);
            }
        } else {
            if (!source.empty() && source.back() != '\n' && source.back() != '\r') {
                source += lineEnd;
            }
            if (!source.empty()) {
                source += lineEnd;
            }
            source += "[editor]" + std::string(lineEnd);
            source += entry;
        }
    }

    const SettingsLoadResult changed = parseSettings(source);
    bool parsedValue = false;
    if (key == "smooth-scrolling") {
        parsedValue = changed.settings.smoothScrolling;
    } else if (key == "front-matter-as-properties") {
        parsedValue = changed.settings.frontMatterAsProperties;
    } else {
        parsedValue = changed.settings.reportBlankLinesAdded;
    }
    if (parsedValue != value) {
        return IoError{"changing the setting alone would not preserve the other settings"};
    }
    return writeFileAtomically(path, source);
}

std::optional<ColorScheme> builtinSchemeFromSetting(std::string_view themeSetting) {
    const std::string lower = toLowerAsciiString(trimBlanks(themeSetting));
    if (lower == "light" || lower == "hitokoto light") {
        return ColorScheme::Light;
    }
    if (lower == "dark" || lower == "hitokoto dark") {
        return ColorScheme::Dark;
    }
    return std::nullopt;
}

}  // namespace hitokoto::core
