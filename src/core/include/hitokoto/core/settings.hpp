// SPDX-License-Identifier: GPL-3.0-or-later
// User settings stored in config.toml inside the platform configuration folder.
#pragma once

#include "hitokoto/core/file_io.hpp"
#include "hitokoto/core/theme.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

inline constexpr std::string_view kSettingsFileName = "config.toml";

struct SettingsDiagnostic {
    std::size_t line = 0;    ///< 1-based (0 for file-level errors)
    std::size_t column = 0;  ///< 1-based (0 for file-level errors)
    std::string message;
    [[nodiscard]] std::string toString() const;
    friend bool operator==(const SettingsDiagnostic&, const SettingsDiagnostic&) = default;
};

struct Settings {
    /// Empty = no explicit theme saved; "light" / "dark" = built-in theme;
    /// otherwise a relative or absolute path (or name) of a `.toml` theme file.
    std::string theme;
    bool smoothScrolling = true;
    bool frontMatterAsProperties = true;
    bool reportBlankLinesAdded = false;

    friend bool operator==(const Settings&, const Settings&) = default;
};

struct SettingsLoadResult {
    Settings settings;
    std::vector<SettingsDiagnostic> diagnostics;
};

/// The text of config.toml after picking a theme (S039).
struct SettingsSourceUpdate {
    std::string source;            ///< the changed text; empty when `error` is set
    std::optional<IoError> error;  ///< why the theme cannot be changed alone; the text is then to be kept as it is
};

[[nodiscard]] SettingsLoadResult parseSettings(std::string_view source);
[[nodiscard]] std::string serializeSettings(const Settings& settings);
/// Changes only the value of the theme setting: the first `theme` key that `parseSettings` reads as the theme
/// (in `[appearance]`, or at the top level before the first section). The whole value is replaced, whatever kind
/// of TOML value it is; every other byte stays. Without such a key, `theme` is added below the first `[appearance]`
/// header, or a new `[appearance]` section at the end. The change is refused when the file defines the theme or
/// `[appearance]` in a form the parser does not read (adding a key would then define it twice in TOML terms), and
/// when the changed text would not parse to the new theme with all other settings and no new diagnostics.
[[nodiscard]] SettingsSourceUpdate updateThemeInSettingsSource(std::string_view source, std::string_view newTheme);
[[nodiscard]] SettingsLoadResult loadSettingsFile(const std::filesystem::path& path) noexcept;
[[nodiscard]] std::optional<IoError> saveSettingsFile(const std::filesystem::path& path, const Settings& settings);
[[nodiscard]] std::optional<IoError> updateThemeSettingInFile(const std::filesystem::path& path,
                                                              std::string_view theme);
/// Changes one boolean key in `[editor]` while preserving all other source bytes.
[[nodiscard]] std::optional<IoError> updateEditorBoolSettingInFile(const std::filesystem::path& path,
                                                                   std::string_view key, bool value);
[[nodiscard]] std::optional<ColorScheme> builtinSchemeFromSetting(std::string_view themeSetting);

}  // namespace hitokoto::core
