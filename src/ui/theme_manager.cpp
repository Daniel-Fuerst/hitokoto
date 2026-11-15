// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme_manager.hpp"

#include "theme_applier.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStyleHints>

namespace hitokoto::ui {

ThemeManager::ThemeManager(QApplication& app, QString userThemePath, QObject* parent)
    : QObject(parent), app_(app), path_(std::move(userThemePath)), configDir_(defaultConfigDir()) {
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
            const QString absPath = info.absoluteFilePath();
            QString canonical = info.canonicalFilePath();
            if (canonical.isEmpty()) {
                canonical = absPath;
            }
            if (seenCanonicalPaths.contains(canonical)) {
                continue;
            }
            seenCanonicalPaths.append(canonical);

            QString displayName = info.completeBaseName();
            const core::ThemeLoadResult result = loadThemeFile(absPath, scheme);
            if (!result.usedFallback && !result.theme.name.empty()) {
                displayName = QString::fromStdString(result.theme.name);
            }
            entries.append(ThemeEntry{displayName, absPath, std::nullopt});
        }
    }
    return entries;
}

core::ThemeLoadResult ThemeManager::loadThemeFile(const QString& path, core::ColorScheme scheme) {
    core::ThemeLoadResult result{core::builtinTheme(scheme), false, {}};
    QFile file(path);
    if (!path.isEmpty() && file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.readAll();
            result =
                core::loadTheme(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), scheme);
        } else {
            result.usedFallback = true;
            result.diagnostics.push_back({0, 0, "cannot read file: " + file.errorString().toStdString()});
        }
    }
    return result;
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

void ThemeManager::selectThemeFile(const QString& path) {
    builtinOverride_.reset();
    path_ = path;
    reload();
}

void ThemeManager::selectBuiltinTheme(core::ColorScheme scheme) {
    builtinOverride_ = scheme;
    path_.clear();
    reload();
}

void ThemeManager::syncSelectedTheme(const ThemeEntry& entry, core::Theme theme) {
    builtinOverride_ = entry.builtinScheme;
    path_ = entry.builtinScheme.has_value() ? QString() : entry.path;
    current_ = std::move(theme);
}

core::ColorScheme ThemeManager::systemScheme() {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::ColorScheme::Dark
                                                                                 : core::ColorScheme::Light;
}

void ThemeManager::reload() {
    const auto scheme = systemScheme();
    core::ThemeLoadResult result{core::builtinTheme(builtinOverride_.value_or(scheme)), false, {}};
    if (!builtinOverride_.has_value()) {
        result = loadThemeFile(path_, scheme);
    }
    current_ = result.theme;
    applyThemeToApplication(app_, current_);
    emit themeChanged(current_);
    if (result.usedFallback) {
        emit problemsFound(path_, formatDiagnostics(result));
    }
}

}  // namespace hitokoto::ui
