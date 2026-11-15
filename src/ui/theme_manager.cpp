// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_manager.hpp"

#include "hitokoto/core/settings.hpp"
#include "qt_bridge.hpp"
#include "theme_applier.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStyleHints>

#include <filesystem>

namespace hitokoto::ui {
namespace {

bool isConfigFileName(const QString& fileName) {
    return fileName.compare(QStringLiteral("config.toml"), Qt::CaseInsensitive) == 0 ||
           fileName.compare(QStringLiteral("settings.toml"), Qt::CaseInsensitive) == 0;
}

bool hasControlCharacters(const QString& s) {
    for (const QChar& c : s) {
        const auto u = c.unicode();
        if (u < 0x20 || u == 0x7f) {
            return true;
        }
    }
    return false;
}

std::optional<ThemeEntry> makeFileThemeEntry(const QFileInfo& info, core::ColorScheme scheme) {
    if (!info.exists() || !info.isFile() || hasControlCharacters(info.fileName())) {
        return std::nullopt;
    }
    const QString absPath = info.absoluteFilePath();
    const core::ThemeLoadResult result = ThemeManager::loadThemeFile(absPath, scheme);
    if (result.usedFallback || hasControlCharacters(QString::fromStdString(result.theme.name))) {
        return std::nullopt;
    }
    QString displayName = info.completeBaseName();
    if (!result.theme.name.empty()) {
        displayName = QString::fromStdString(result.theme.name);
    }
    return ThemeEntry{displayName, absPath, std::nullopt};
}

}  // namespace

ThemeManager::ThemeManager(QApplication& app, QString userThemePath, QObject* parent)
    : QObject(parent),
      app_(app),
      path_(std::move(userThemePath)),
      configDir_(path_.isEmpty() ? defaultConfigDir() : QFileInfo(path_).absolutePath()) {
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { reload(); });
}

QString ThemeManager::defaultThemePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(dir).filePath(QStringLiteral("hitokoto/theme.toml"));
}

QString ThemeManager::defaultConfigDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(dir).filePath(QStringLiteral("hitokoto"));
}

QString ThemeManager::defaultConfigFilePath() {
    return configFilePath(defaultConfigDir());
}

QString ThemeManager::configFilePath(const QString& configDir) {
    if (configDir.isEmpty()) {
        return {};
    }
    return QDir(configDir).filePath(toQString(core::kSettingsFileName));
}

bool ThemeManager::saveSelectedTheme(const QString& configDir, const ThemeEntry& entry) {
    const QString cfgPath = configFilePath(configDir);
    if (cfgPath.isEmpty()) {
        return false;
    }
    const std::filesystem::path fsPath(cfgPath.toStdU16String());
    const core::SettingsLoadResult loaded = core::loadSettingsFile(fsPath);
    core::Settings settings = loaded.settings;

    std::string nextTheme;
    if (entry.builtinScheme.has_value()) {
        nextTheme = (*entry.builtinScheme == core::ColorScheme::Dark) ? "dark" : "light";
    } else if (!entry.path.isEmpty()) {
        const QDir baseDir(configDir);
        const QString absEntry = QFileInfo(entry.path).absoluteFilePath();
        const QString rel = QDir::fromNativeSeparators(baseDir.relativeFilePath(absEntry));
        if (!rel.isEmpty() && !rel.startsWith(QStringLiteral("..")) && !QDir::isAbsolutePath(rel)) {
            nextTheme = rel.toStdString();
        } else {
            nextTheme = QDir::fromNativeSeparators(absEntry).toStdString();
        }
    }

    if (hasControlCharacters(entry.path) || hasControlCharacters(toQString(nextTheme))) {
        return false;
    }

    const auto err = core::updateThemeSettingInFile(fsPath, nextTheme);
    return !err.has_value();
}

std::optional<ThemeEntry> ThemeManager::loadSavedThemeEntry(const QString& configDir) {
    const QString cfgPath = configFilePath(configDir);
    if (cfgPath.isEmpty() || !QFileInfo::exists(cfgPath)) {
        return std::nullopt;
    }

    const core::SettingsLoadResult loaded = core::loadSettingsFile(std::filesystem::path(cfgPath.toStdU16String()));
    const QString raw = toQString(loaded.settings.theme).trimmed();
    if (raw.isEmpty() || hasControlCharacters(raw)) {
        return std::nullopt;
    }

    if (const auto scheme = core::builtinSchemeFromSetting(loaded.settings.theme)) {
        const auto& builtin = core::builtinTheme(*scheme);
        return ThemeEntry{QString::fromStdString(builtin.name), {}, scheme};
    }

    QString normalized = raw;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));

    QStringList candidates;
    if (QFileInfo(normalized).isAbsolute()) {
        candidates.append(normalized);
    } else {
        candidates.append(QDir(configDir).filePath(normalized));
        candidates.append(QDir(configDir).filePath(QStringLiteral("themes/") + normalized));
    }

    const auto scheme = systemScheme();
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile() && !isConfigFileName(info.fileName())) {
            if (const auto entry = makeFileThemeEntry(info, scheme)) {
                return entry;
            }
        }
    }

    const QList<ThemeEntry> entries = availableThemes(configDir);
    for (const ThemeEntry& entry : entries) {
        if (entry.name.compare(raw, Qt::CaseInsensitive) == 0) {
            return entry;
        }
    }
    for (const ThemeEntry& entry : entries) {
        if (entry.builtinScheme.has_value()) {
            continue;
        }
        const QFileInfo info(entry.path);
        if (info.fileName().compare(raw, Qt::CaseInsensitive) == 0 ||
            info.completeBaseName().compare(raw, Qt::CaseInsensitive) == 0) {
            return entry;
        }
    }
    return std::nullopt;
}

QList<ThemeEntry> ThemeManager::availableThemes(const QString& configDir) {
    QList<ThemeEntry> entries;
    const auto light = core::builtinTheme(core::ColorScheme::Light);
    const auto dark = core::builtinTheme(core::ColorScheme::Dark);
    entries.append(ThemeEntry{QString::fromStdString(light.name), {}, core::ColorScheme::Light});
    entries.append(ThemeEntry{QString::fromStdString(dark.name), {}, core::ColorScheme::Dark});

    if (configDir.isEmpty()) {
        return entries;
    }

    const auto scheme = systemScheme();
    QStringList searchDirs{configDir};
    const QString themesSubdir = QDir(configDir).filePath(QStringLiteral("themes"));
    if (QFileInfo::exists(themesSubdir)) {
        searchDirs.append(themesSubdir);
    }

    QStringList seenCanonicalPaths;
    for (const QString& dirPath : searchDirs) {
        const QDir dir(dirPath);
        if (!dir.exists()) {
            continue;
        }
        const QFileInfoList files =
            dir.entryInfoList({QStringLiteral("*.toml")}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& info : files) {
            if (isConfigFileName(info.fileName()) || hasControlCharacters(info.fileName())) {
                continue;
            }
            const QString absPath = info.absoluteFilePath();
            QString canonical = info.canonicalFilePath();
            if (canonical.isEmpty()) {
                canonical = absPath;
            }
            if (seenCanonicalPaths.contains(canonical)) {
                continue;
            }
            seenCanonicalPaths.append(canonical);
            if (const auto entry = makeFileThemeEntry(info, scheme)) {
                entries.append(*entry);
            }
        }
    }
    return entries;
}

core::ThemeLoadResult ThemeManager::loadThemeFile(const QString& path, core::ColorScheme scheme) {
    core::ThemeLoadResult result{core::builtinTheme(scheme), false, {}};
    if (path.isEmpty() || hasControlCharacters(path)) {
        result.usedFallback = true;
        result.diagnostics.push_back({0, 0, path.isEmpty() ? "path is empty" : "path contains control characters"});
        return result;
    }
    const QFileInfo info(path);
    if (!info.exists()) {
        return result;
    }
    if (!info.isFile()) {
        result.usedFallback = true;
        result.diagnostics.push_back({0, 0, "not a regular file"});
        return result;
    }

    constexpr qint64 kMaxThemeFileSize = 1024LL * 1024LL;  // 1 MiB
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.usedFallback = true;
        result.diagnostics.push_back({0, 0, "cannot read file: " + file.errorString().toStdString()});
        return result;
    }

    const QByteArray bytes = file.read(kMaxThemeFileSize + 1);
    if (bytes.size() > kMaxThemeFileSize || !file.atEnd()) {
        result.usedFallback = true;
        result.diagnostics.push_back({0, 0, "theme file exceeds maximum allowed size (1 MiB)"});
        return result;
    }

    return core::loadTheme(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), scheme);
}

QStringList ThemeManager::formatDiagnostics(const core::ThemeLoadResult& result) {
    QStringList lines;
    for (const auto& d : result.diagnostics) {
        lines << QString::fromStdString(d.line > 0 ? d.toString() : d.message);
    }
    return lines;
}

void ThemeManager::setConfigDir(QString dir) {
    configDir_ = std::move(dir);
}

void ThemeManager::setCliThemeOverride(bool override) {
    cliThemeOverride_ = override;
}

void ThemeManager::selectThemeFile(const QString& path) {
    builtinOverride_.reset();
    cliThemeOverride_ = false;
    path_ = path;
    const auto scheme = systemScheme();
    const core::ThemeLoadResult check = loadThemeFile(path_, scheme);
    if (check.usedFallback) {
        activeThemePath_.clear();
        current_ = check.theme;
        applyThemeToApplication(app_, current_);
        emit themeChanged(current_);
        emit problemsFound(path_, formatDiagnostics(check));
        return;
    }
    if (!path_.isEmpty() && QFileInfo::exists(path_)) {
        saveSelectedTheme(configDir_, ThemeEntry{{}, QFileInfo(path_).absoluteFilePath(), std::nullopt});
    }
    reload();
}

void ThemeManager::selectBuiltinTheme(core::ColorScheme scheme) {
    builtinOverride_ = scheme;
    cliThemeOverride_ = false;
    path_.clear();
    const auto& builtin = core::builtinTheme(scheme);
    saveSelectedTheme(configDir_, ThemeEntry{QString::fromStdString(builtin.name), {}, scheme});
    reload();
}

void ThemeManager::syncSelectedTheme(const ThemeEntry& entry, core::Theme theme) {
    builtinOverride_ = entry.builtinScheme;
    cliThemeOverride_ = false;
    path_ = entry.builtinScheme.has_value() ? QString() : entry.path;
    activeThemePath_ = entry.builtinScheme.has_value() ? QString() : QFileInfo(entry.path).absoluteFilePath();
    current_ = std::move(theme);
    saveSelectedTheme(configDir_, entry);
}

core::ColorScheme ThemeManager::systemScheme() {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::ColorScheme::Dark
                                                                                 : core::ColorScheme::Light;
}

void ThemeManager::reload() {
    const auto scheme = systemScheme();
    core::ThemeLoadResult result{core::builtinTheme(scheme), false, {}};
    QString loadedPath;

    if (builtinOverride_.has_value()) {
        result.theme = core::builtinTheme(*builtinOverride_);
        activeThemePath_.clear();
    } else if (const auto saved = cliThemeOverride_ ? std::nullopt : loadSavedThemeEntry(configDir_)) {
        if (saved->builtinScheme.has_value()) {
            result.theme = core::builtinTheme(*saved->builtinScheme);
            activeThemePath_.clear();
        } else {
            loadedPath = saved->path;
            result = loadThemeFile(loadedPath, scheme);
            activeThemePath_ = result.usedFallback ? QString() : QFileInfo(loadedPath).absoluteFilePath();
        }
    } else {
        loadedPath = path_;
        result = loadThemeFile(loadedPath, scheme);
        activeThemePath_ = (!loadedPath.isEmpty() && QFileInfo::exists(loadedPath) && !result.usedFallback)
                               ? QFileInfo(loadedPath).absoluteFilePath()
                               : QString();
    }

    current_ = result.theme;
    applyThemeToApplication(app_, current_);
    emit themeChanged(current_);
    if (result.usedFallback) {
        emit problemsFound(loadedPath, formatDiagnostics(result));
    }
}

}  // namespace hitokoto::ui
