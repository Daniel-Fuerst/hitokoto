// SPDX-License-Identifier: GPL-3.0-or-later
// S039: settings menu configuration persistence and CLI builtin scheme matching.
#include "hitokoto/core/settings.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace hitokoto::core;

namespace {

class SettingsTempDir {
public:
    SettingsTempDir() {
        std::random_device rd;
        path_ = std::filesystem::temp_directory_path() /
                ("hitokoto-settings-test-" + std::to_string(rd()) + std::to_string(rd()));
        std::filesystem::create_directories(path_);
    }
    ~SettingsTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    SettingsTempDir(const SettingsTempDir&) = delete;
    SettingsTempDir& operator=(const SettingsTempDir&) = delete;
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/// What picking a theme leaves in a config.toml that held `original`.
struct ThemeUpdateOutcome {
    std::optional<IoError> error;
    std::string bytes;
};

ThemeUpdateOutcome updateThemeIn(const std::string& original, std::string_view theme) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";
    {
        std::ofstream out(configPath, std::ios::binary);
        out << original;
    }
    ThemeUpdateOutcome outcome;
    outcome.error = updateThemeSettingInFile(configPath, theme);
    const ReadResult read = readFile(configPath);
    outcome.bytes = read.bytes ? *read.bytes : std::string("<unreadable>");
    return outcome;
}

struct ThemeUpdateCase {
    const char* name;
    std::string original;
    std::string expected;
};

void expectThemeUpdates(const std::vector<ThemeUpdateCase>& cases) {
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        const ThemeUpdateOutcome outcome = updateThemeIn(c.original, "ember.toml");
        EXPECT_FALSE(outcome.error.has_value()) << (outcome.error ? outcome.error->message : "");
        EXPECT_EQ(outcome.bytes, c.expected);
        EXPECT_EQ(parseSettings(outcome.bytes).settings.theme, "ember.toml");
    }
}

}  // namespace

TEST(S039_SettingsMenu, SettingsParseAndSerializeRoundTrip) {
    const Settings original{
        .theme = "themes/berry-crush.toml",
        .smoothScrolling = false,
        .frontMatterAsProperties = false,
        .reportBlankLinesAdded = true,
    };
    const std::string serialized = serializeSettings(original);
    const SettingsLoadResult parsed = parseSettings(serialized);
    EXPECT_TRUE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.settings, original);

    // Paths containing backslashes and quotes (e.g. Windows paths) round-trip cleanly.
    const Settings windowsPath{
        .theme = R"(C:\Users\alice\"themes"\ember.toml)",
        .smoothScrolling = true,
        .frontMatterAsProperties = true,
        .reportBlankLinesAdded = false,
    };
    const SettingsLoadResult roundTripped = parseSettings(serializeSettings(windowsPath));
    EXPECT_TRUE(roundTripped.diagnostics.empty());
    EXPECT_EQ(roundTripped.settings, windowsPath);

    // Top-level keys, comments, and CRLF line endings also parse cleanly.
    const SettingsLoadResult crlf = parseSettings(
        "theme = \"dark\" # comment\r\n"
        "smooth-scrolling = false\r\n"
        "front-matter-as-properties = true\r\n"
        "report-blank-lines-added = true\r\n");
    EXPECT_TRUE(crlf.diagnostics.empty());
    EXPECT_EQ(crlf.settings.theme, "dark");
    EXPECT_FALSE(crlf.settings.smoothScrolling);
    EXPECT_TRUE(crlf.settings.frontMatterAsProperties);
    EXPECT_TRUE(crlf.settings.reportBlankLinesAdded);
}

TEST(S039_SettingsMenu, SettingsParseDiagnosticsPreserveValidEntries) {
    const SettingsLoadResult res = parseSettings(
        "[appearance]\n"
        "theme = \"light\"\n"
        "unknown-key = \"ignored\"\n"
        "[unknown]\n"
        "foo = true\n"
        "[editor]\n"
        "smooth-scrolling = \"yes\"\n"
        "front-matter-as-properties = false\n"
        "broken line without equals\n");
    EXPECT_EQ(res.diagnostics.size(), 4u);
    EXPECT_EQ(res.settings.theme, "light");
    EXPECT_TRUE(res.settings.smoothScrolling);
    EXPECT_FALSE(res.settings.frontMatterAsProperties);
    EXPECT_FALSE(res.settings.reportBlankLinesAdded);
}

TEST(S039_SettingsMenu, SettingsSaveCreatesDirectoriesAndLoadsAtomically) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "nested" / "hitokoto" / std::string(kSettingsFileName);

    // Non-existent config file returns defaults with no diagnostics.
    const SettingsLoadResult missing = loadSettingsFile(configPath);
    EXPECT_TRUE(missing.diagnostics.empty());
    EXPECT_EQ(missing.settings, Settings{});

    const Settings saved{
        .theme = "gruvbox.toml",
        .smoothScrolling = false,
        .frontMatterAsProperties = true,
        .reportBlankLinesAdded = true,
    };
    const auto err = saveSettingsFile(configPath, saved);
    EXPECT_FALSE(err.has_value()) << (err ? err->message : "");
    EXPECT_TRUE(std::filesystem::exists(configPath));

    const SettingsLoadResult loaded = loadSettingsFile(configPath);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.settings, saved);
}

TEST(S039_SettingsMenu, BuiltinSchemeFromSettingMatchesLightAndDarkCaseInsensitively) {
    EXPECT_EQ(builtinSchemeFromSetting("light"), ColorScheme::Light);
    EXPECT_EQ(builtinSchemeFromSetting("Light"), ColorScheme::Light);
    EXPECT_EQ(builtinSchemeFromSetting("hitokoto Light"), ColorScheme::Light);
    EXPECT_EQ(builtinSchemeFromSetting("DARK"), ColorScheme::Dark);
    EXPECT_EQ(builtinSchemeFromSetting("hitokoto dark"), ColorScheme::Dark);
    EXPECT_EQ(builtinSchemeFromSetting("berry-crush.toml"), std::nullopt);
    EXPECT_EQ(builtinSchemeFromSetting(""), std::nullopt);
}

TEST(S039_SettingsMenu, LoadSettingsFileRejectsNonRegularOrOversizedFiles) {
    SettingsTempDir dir;
    const std::filesystem::path configDir = dir.path() / "dir_config";
    std::filesystem::create_directories(configDir);

    const SettingsLoadResult dirResult = loadSettingsFile(configDir);
    EXPECT_EQ(dirResult.settings, Settings{});
    EXPECT_FALSE(dirResult.diagnostics.empty());

    const std::filesystem::path oversizedPath = dir.path() / "oversized.toml";
    {
        std::ofstream out(oversizedPath, std::ios::binary);
        out << "theme = \"light\"\n";
    }
    std::filesystem::resize_file(oversizedPath, 64 * 1024 + 1);
    const SettingsLoadResult oversizedResult = loadSettingsFile(oversizedPath);
    EXPECT_EQ(oversizedResult.settings, Settings{});
    EXPECT_FALSE(oversizedResult.diagnostics.empty());

#if !defined(_WIN32)
    const std::filesystem::path fifoPath = dir.path() / "fifo.toml";
    ASSERT_EQ(::mkfifo(fifoPath.c_str(), 0600), 0);
    ::alarm(2);
    const SettingsLoadResult fifoResult = loadSettingsFile(fifoPath);
    ::alarm(0);
    EXPECT_EQ(fifoResult.settings, Settings{});
    EXPECT_FALSE(fifoResult.diagnostics.empty());
#endif
}

TEST(S039_SettingsMenu, UpdateThemeSettingPreservesExistingContentByteForByte) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";

    const std::string original =
        "# my notes on this file\n"
        "[appearance]\n"
        "theme = \"light\"\n"
        "\n"
        "[editor]\n"
        "smooth-scrolling = false\n"
        "front-matter-as-properties = false # as I like it\n"
        "spell-check = true\n"
        "report-blank-lines-added = ture\n";

    {
        std::ofstream out(configPath, std::ios::binary);
        out << original;
    }

    const auto err = updateThemeSettingInFile(configPath, "dark");
    EXPECT_FALSE(err.has_value());

    const ReadResult read = readFile(configPath);
    if (!read.bytes) {
        FAIL() << "read failed";
        return;
    }

    const std::string expected =
        "# my notes on this file\n"
        "[appearance]\n"
        "theme = \"dark\"\n"
        "\n"
        "[editor]\n"
        "smooth-scrolling = false\n"
        "front-matter-as-properties = false # as I like it\n"
        "spell-check = true\n"
        "report-blank-lines-added = ture\n";

    EXPECT_EQ(*read.bytes, expected);
}

TEST(S039_SettingsMenu, UpdateThemeSettingPreservesCrlfLineEndings) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";

    const std::string original =
        "[appearance]\r\n"
        "theme = \"light\"\r\n"
        "\r\n"
        "[editor]\r\n"
        "smooth-scrolling = false\r\n";

    {
        std::ofstream out(configPath, std::ios::binary);
        out << original;
    }

    const auto err = updateThemeSettingInFile(configPath, "nord.toml");
    EXPECT_FALSE(err.has_value());

    const ReadResult read = readFile(configPath);
    if (!read.bytes) {
        FAIL() << "read failed";
        return;
    }

    const std::string expected =
        "[appearance]\r\n"
        "theme = \"nord.toml\"\r\n"
        "\r\n"
        "[editor]\r\n"
        "smooth-scrolling = false\r\n";

    EXPECT_EQ(*read.bytes, expected);
}

TEST(S039_SettingsMenu, UpdateThemeSettingAddsAppearanceSectionIfMissing) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";

    const std::string original =
        "[editor]\n"
        "smooth-scrolling = false\n";

    {
        std::ofstream out(configPath, std::ios::binary);
        out << original;
    }

    const auto err = updateThemeSettingInFile(configPath, "dracula.toml");
    EXPECT_FALSE(err.has_value());

    const ReadResult read = readFile(configPath);
    if (!read.bytes) {
        FAIL() << "read failed";
        return;
    }

    const SettingsLoadResult loaded = parseSettings(*read.bytes);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.settings.theme, "dracula.toml");
    EXPECT_FALSE(loaded.settings.smoothScrolling);
    EXPECT_NE(read.bytes->find("[editor]\nsmooth-scrolling = false"), std::string::npos);
}

#if !defined(_WIN32)
TEST(S039_SettingsMenu, UpdateThemeSettingLeavesUnreadableFileUntouched) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";

    const std::string original = "# unreadable secret content\n";
    {
        std::ofstream out(configPath, std::ios::binary);
        out << original;
    }
    // Write permissions only (no read permissions)
    std::filesystem::permissions(configPath, std::filesystem::perms::owner_write);
    // root ignores file permissions (the Debian package runs its tests as root in a container),
    // and so may a file system: then the file is not unreadable and there is nothing to test.
    if (std::ifstream(configPath, std::ios::binary).is_open()) {
        GTEST_SKIP() << "the file stays readable without read permission (running as root?)";
    }

    const auto err = updateThemeSettingInFile(configPath, "dark");
    EXPECT_TRUE(err.has_value());

    // Restore read permissions to verify file was untouched
    std::filesystem::permissions(configPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    const ReadResult read = readFile(configPath);
    if (!read.bytes) {
        FAIL() << "read failed";
        return;
    }
    EXPECT_EQ(*read.bytes, original);
}
#endif

TEST(S039_SettingsMenu, RefusesThemeSettingWithControlCharactersOrLineBreaks) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";

    const Settings malicious{
        .theme = "a\n[editor]\nsmooth-scrolling = false\n#.toml",
        .smoothScrolling = true,
        .frontMatterAsProperties = true,
        .reportBlankLinesAdded = false,
    };

    const auto err = saveSettingsFile(configPath, malicious);
    EXPECT_TRUE(err.has_value());

    const auto updateErr = updateThemeSettingInFile(configPath, malicious.theme);
    EXPECT_TRUE(updateErr.has_value());

    EXPECT_FALSE(std::filesystem::exists(configPath));
}

// A `theme` key under another section is not the theme: only the key in [appearance] (or a
// top-level key before the first section, which hitokoto reads as the theme) changes.
TEST(S039_SettingsMenu, UpdateThemeChangesOnlyTheThemeKeyOfAppearance) {
    expectThemeUpdates({
        {"theme under [editor] before [appearance]", "[editor]\ntheme = \"x\"\n\n[appearance]\ntheme = \"light\"\n",
         "[editor]\ntheme = \"x\"\n\n[appearance]\ntheme = \"ember.toml\"\n"},
        {"theme under [editor], no [appearance]", "[editor]\nsmooth-scrolling = false\ntheme = \"x\"\n",
         "[editor]\nsmooth-scrolling = false\ntheme = \"x\"\n\n[appearance]\ntheme = \"ember.toml\"\n"},
        {"theme under an unknown section", "[appearance]\ntheme = \"light\"\n[other]\ntheme = \"x\"\n",
         "[appearance]\ntheme = \"ember.toml\"\n[other]\ntheme = \"x\"\n"},
    });
}

TEST(S039_SettingsMenu, UpdateThemeChangesATopLevelKeyBeforeAnySection) {
    expectThemeUpdates({
        {"top-level key, then [editor]", "theme = \"light\" # mine\n\n[editor]\nsmooth-scrolling = false\n",
         "theme = \"ember.toml\" # mine\n\n[editor]\nsmooth-scrolling = false\n"},
        {"top-level key, then a second one in [appearance]", "theme = \"x\"\n[appearance]\ntheme = \"light\"\n",
         "theme = \"ember.toml\"\n[appearance]\ntheme = \"light\"\n"},
    });
}

// TOML 1.0 knows four kinds of strings; the whole value is replaced, whatever its kind.
TEST(S039_SettingsMenu, UpdateThemeReplacesTheWholeValue) {
    const std::string before = "[appearance]\n";
    const std::string after = "\n[editor]\nsmooth-scrolling = false\n";
    const std::string expected = before + "theme = \"ember.toml\"" + after;
    expectThemeUpdates({
        {"multi-line basic string", before + R"(theme = """light""")" + after, expected},
        {"multi-line basic string over two lines", before + "theme = \"\"\"\nlight\"\"\"" + after, expected},
        {"multi-line basic string with quotes inside the delimiters",
         before + R"(theme = """"light" theme"""")" + after, expected},
        {"literal string", before + "theme = 'light'" + after, expected},
        {"multi-line literal string", before + "theme = '''light'''" + after, expected},
        {"basic string with an escaped quote", before + R"(theme = "a\"b")" + after, expected},
        {"boolean", before + "theme = true" + after, expected},
        {"no value", before + "theme =" + after, expected},
        {"comment after the value", before + "theme = \"light\" # mine" + after,
         before + "theme = \"ember.toml\" # mine" + after},
    });
}

TEST(S039_SettingsMenu, UpdateThemeKeepsLineEndingsAndAByteOrderMark) {
    expectThemeUpdates({
        {"CRLF, theme under [editor] before [appearance]",
         "[editor]\r\ntheme = \"x\"\r\n\r\n[appearance]\r\ntheme = \"light\"\r\n",
         "[editor]\r\ntheme = \"x\"\r\n\r\n[appearance]\r\ntheme = \"ember.toml\"\r\n"},
        {"CRLF, [appearance] without theme", "[appearance]\r\n[editor]\r\nsmooth-scrolling = false\r\n",
         "[appearance]\r\ntheme = \"ember.toml\"\r\n[editor]\r\nsmooth-scrolling = false\r\n"},
        {"byte order mark before [appearance]", "\xEF\xBB\xBF[appearance]\ntheme = \"light\"\n",
         "\xEF\xBB\xBF[appearance]\ntheme = \"ember.toml\"\n"},
        {"byte order mark before a top-level key", "\xEF\xBB\xBFtheme = \"light\"\n",
         "\xEF\xBB\xBFtheme = \"ember.toml\"\n"},
        {"[appearance] as the last line without a line break", "[editor]\nsmooth-scrolling = false\n[appearance]",
         "[editor]\nsmooth-scrolling = false\n[appearance]\ntheme = \"ember.toml\"\n"},
    });
}

// When the theme cannot be changed alone, the file stays as it is and the caller gets a reason.
TEST(S039_SettingsMenu, UpdateThemeLeavesTheFileWhenOnlyTheThemeCannotBeChanged) {
    const std::vector<std::pair<const char*, std::string>> cases = {
        // TOML 1.0: a dotted key defines the table appearance; a header [appearance] may not define it again.
        {"dotted key", "appearance.theme = \"light\"\n"},
        // TOML 1.0: whitespace around the key of a table header is ignored, so this is [appearance].
        {"header with spaces", "[ appearance ]\ntheme = \"light\"\n"},
        // TOML 1.0: quoted and bare keys are equivalent, so a second theme key would be a duplicate.
        {"quoted key", "[appearance]\n\"theme\" = \"light\"\n"},
        {"array value over two lines", "[appearance]\ntheme = [\"a\",\n  \"b\"]\n"},
        {"multi-line string without its end", "[appearance]\ntheme = \"\"\"light\n"},
        // hitokoto reads the lines inside the string as settings; replacing the string would drop them.
        {"multi-line string holding lines that hitokoto reads",
         "[appearance]\ntheme = \"\"\"\n[editor]\nsmooth-scrolling = false\n\"\"\"\n"},
    };
    for (const auto& [name, original] : cases) {
        SCOPED_TRACE(name);
        const ThemeUpdateOutcome outcome = updateThemeIn(original, "ember.toml");
        EXPECT_TRUE(outcome.error.has_value());
        EXPECT_EQ(outcome.bytes, original);
    }
}
