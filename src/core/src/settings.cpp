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
    if (!settings.theme.empty()) {
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
