// SPDX-License-Identifier: GPL-3.0-or-later
// S005: finds, loads and applies the theme; follows the system colour scheme.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

class QApplication;

namespace hitokoto::ui {

class ThemeManager : public QObject {
    Q_OBJECT
public:
    /// `userThemePath` may point to a file that does not exist (then the built-in theme is used).
    ThemeManager(QApplication& app, QString userThemePath, QObject* parent = nullptr);

    /// Default location: <config dir>/hitokoto/theme.toml (see docs/theming.md).
    static QString defaultThemePath();
    static core::ColorScheme systemScheme();

    /// Loads the theme file (if any) and applies it. Emits themeChanged and, if the file was
    /// rejected, problemsFound.
    void reload();

    [[nodiscard]] const core::Theme& current() const noexcept { return current_; }
    [[nodiscard]] const QString& userThemePath() const noexcept { return path_; }

signals:
    void themeChanged(const hitokoto::core::Theme& theme);
    void problemsFound(const QString& path, const QStringList& diagnostics);

private:
    QApplication& app_;
    QString path_;
    core::Theme current_;
};

}  // namespace hitokoto::ui
