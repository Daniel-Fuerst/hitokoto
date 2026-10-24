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
    : QObject(parent), app_(app), path_(std::move(userThemePath)) {
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { reload(); });
}

QString ThemeManager::defaultThemePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(dir).filePath(QStringLiteral("hitokoto/theme.toml"));
}

core::ColorScheme ThemeManager::systemScheme() {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? core::ColorScheme::Dark
                                                                                 : core::ColorScheme::Light;
}

void ThemeManager::reload() {
    const auto scheme = systemScheme();
    core::ThemeLoadResult result{core::builtinTheme(scheme), false, {}};
    QFile file(path_);
    if (!path_.isEmpty() && file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.readAll();
            result =
                core::loadTheme(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), scheme);
        } else {
            result.usedFallback = true;
            result.diagnostics.push_back({0, 0, "cannot read file: " + file.errorString().toStdString()});
        }
    }
    current_ = result.theme;
    applyThemeToApplication(app_, current_);
    emit themeChanged(current_);
    if (result.usedFallback) {
        QStringList lines;
        for (const auto& d : result.diagnostics) {
            lines << QString::fromStdString(d.line > 0 ? d.toString() : d.message);
        }
        emit problemsFound(path_, lines);
    }
}

}  // namespace hitokoto::ui
