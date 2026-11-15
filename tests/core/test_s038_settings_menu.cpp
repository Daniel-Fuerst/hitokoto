// SPDX-License-Identifier: GPL-3.0-or-later
// S038: settings menu configuration persistence and CLI builtin scheme matching.
#include "hitokoto/core/settings.hpp"
#include "hitokoto/core/theme.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>

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

}  // namespace

TEST(S038_SettingsMenu, SettingsParseAndSerializeRoundTrip) {
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

TEST(S038_SettingsMenu, SettingsParseDiagnosticsPreserveValidEntries) {
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

TEST(S038_SettingsMenu, SettingsSaveCreatesDirectoriesAndLoadsAtomically) {
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

TEST(S038_SettingsMenu, BuiltinSchemeFromSettingMatchesLightAndDarkCaseInsensitively) {
    EXPECT_EQ(builtinSchemeFromSetting("light"), ColorScheme::Light);
    EXPECT_EQ(builtinSchemeFromSetting("Light"), ColorScheme::Light);
    EXPECT_EQ(builtinSchemeFromSetting("hitokoto Light"), ColorScheme::Light);
    EXPECT_EQ(builtinSchemeFromSetting("DARK"), ColorScheme::Dark);
    EXPECT_EQ(builtinSchemeFromSetting("hitokoto dark"), ColorScheme::Dark);
    EXPECT_EQ(builtinSchemeFromSetting("berry-crush.toml"), std::nullopt);
    EXPECT_EQ(builtinSchemeFromSetting(""), std::nullopt);
}

TEST(S038_SettingsMenu, LoadSettingsFileRejectsNonRegularOrOversizedFiles) {
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

TEST(S038_SettingsMenu, UpdateThemeSettingPreservesExistingContentByteForByte) {
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

TEST(S038_SettingsMenu, UpdateThemeSettingPreservesCrlfLineEndings) {
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

TEST(S038_SettingsMenu, UpdateThemeSettingAddsAppearanceSectionIfMissing) {
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
TEST(S038_SettingsMenu, UpdateThemeSettingLeavesUnreadableFileUntouched) {
    SettingsTempDir dir;
    const std::filesystem::path configPath = dir.path() / "config.toml";

    const std::string original = "# unreadable secret content\n";
    {
        std::ofstream out(configPath, std::ios::binary);
        out << original;
    }
    // Write permissions only (no read permissions)
    std::filesystem::permissions(configPath, std::filesystem::perms::owner_write);

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

TEST(S038_SettingsMenu, RefusesThemeSettingWithControlCharactersOrLineBreaks) {
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
