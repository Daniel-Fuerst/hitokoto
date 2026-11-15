// SPDX-License-Identifier: GPL-3.0-or-later
// S005: finds, loads and applies the theme; follows the system colour scheme.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

class QApplication;

namespace hitokoto::ui {

struct ThemeEntry {
    QString name;
    QString path;
    std::optional<core::ColorScheme> builtinScheme;
    friend bool operator==(const ThemeEntry&, const ThemeEntry&) = default;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    /// `userThemePath` may point to a file that does not exist (then the built-in theme is used).
    ThemeManager(QApplication& app, QString userThemePath, QObject* parent = nullptr);

    /// Default location: <config dir>/hitokoto/theme.toml (see docs/theming.md).
    static QString defaultThemePath();
    /// Default configuration directory: <config dir>/hitokoto.
    static QString defaultConfigDir();
    /// Default configuration file: <config dir>/hitokoto/config.toml.
    static QString defaultConfigFilePath();
    /// Configuration file inside `configDir`: `<configDir>/config.toml` (empty if `configDir` is empty).
    static QString configFilePath(const QString& configDir);
    /// Persists the selected theme entry to `<configDir>/config.toml`, preserving existing settings.
    static bool saveSelectedTheme(const QString& configDir, const ThemeEntry& entry);
    /// Resolves the persisted theme entry from `<configDir>/config.toml`, if configured and found.
    static std::optional<ThemeEntry> loadSavedThemeEntry(const QString& configDir);
    /// Built-in themes plus any valid/discoverable `.toml` theme files in `configDir` (and `<configDir>/themes`).
    static QList<ThemeEntry> availableThemes(const QString& configDir);
    /// Parses `path` using `scheme` as fallback base. Non-existent files return builtin(`scheme`) without fallback
    /// flag.
    static core::ThemeLoadResult loadThemeFile(const QString& path, core::ColorScheme scheme);
    static QStringList formatDiagnostics(const core::ThemeLoadResult& result);
    static core::ColorScheme systemScheme();

    /// Loads the theme file (if any) and applies it. Emits themeChanged and, if the file was
    /// rejected, problemsFound.
    void reload();

    void setConfigDir(QString dir);
    [[nodiscard]] const QString& configDir() const noexcept { return configDir_; }
    void setCliThemeOverride(bool override);
    [[nodiscard]] bool cliThemeOverride() const noexcept { return cliThemeOverride_; }

    void selectThemeFile(const QString& path);
    void selectBuiltinTheme(core::ColorScheme scheme);
    /// Synchronises tracked selection state when another component has already loaded and applied `entry`.
    void syncSelectedTheme(const ThemeEntry& entry, core::Theme theme);

    [[nodiscard]] const core::Theme& current() const noexcept { return current_; }
    [[nodiscard]] const QString& userThemePath() const noexcept { return path_; }
    [[nodiscard]] const QString& activeThemePath() const noexcept { return activeThemePath_; }

signals:
    void themeChanged(const hitokoto::core::Theme& theme);
    void problemsFound(const QString& path, const QStringList& diagnostics);

private:
    QApplication& app_;
    QString path_;
    QString configDir_;
    QString activeThemePath_;
    std::optional<core::ColorScheme> builtinOverride_;
    bool cliThemeOverride_ = false;
    core::Theme current_;
};

}  // namespace hitokoto::ui
