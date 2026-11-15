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

[[nodiscard]] SettingsLoadResult parseSettings(std::string_view source);
[[nodiscard]] std::string serializeSettings(const Settings& settings);
[[nodiscard]] std::string updateThemeInSettingsSource(std::string_view source, std::string_view newTheme);
[[nodiscard]] SettingsLoadResult loadSettingsFile(const std::filesystem::path& path) noexcept;
[[nodiscard]] std::optional<IoError> saveSettingsFile(const std::filesystem::path& path, const Settings& settings);
[[nodiscard]] std::optional<IoError> updateThemeSettingInFile(const std::filesystem::path& path,
                                                              std::string_view theme);
[[nodiscard]] std::optional<ColorScheme> builtinSchemeFromSetting(std::string_view themeSetting);

}  // namespace hitokoto::core
