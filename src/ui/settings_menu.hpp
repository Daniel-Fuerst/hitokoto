// SPDX-License-Identifier: GPL-3.0-or-later
// Non-modal floating settings flyout anchored beside the floating toolbar.
#pragma once

#include "hitokoto/core/theme.hpp"
#include "theme_manager.hpp"

#include <QColor>
#include <QList>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace hitokoto::ui {

class ToggleSwitch;

/// S044: the line to the right of a section heading, centred vertically in its height.
class SectionRule : public QWidget {
public:
    explicit SectionRule(QWidget* parent = nullptr);
    void setRule(const QColor& color, double width);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor color_;
    double width_ = 1;
};

class SettingsMenu : public QWidget {
    Q_OBJECT
public:
    explicit SettingsMenu(QWidget* parent = nullptr);
    ~SettingsMenu() override = default;
    SettingsMenu(const SettingsMenu&) = delete;
    SettingsMenu& operator=(const SettingsMenu&) = delete;

    void applyTheme(const core::Theme& theme);
    void setAvailableThemes(const QList<ThemeEntry>& entries, const QString& activePath,
                            const core::Theme& activeTheme);
    [[nodiscard]] QComboBox* themeSelector() const noexcept { return themeCombo_; }
    [[nodiscard]] const QList<ThemeEntry>& themeEntries() const noexcept { return entries_; }
    [[nodiscard]] const core::ToolbarStyle& toolbarStyle() const noexcept { return style_; }
    /// S043: shows the state of smooth scrolling without animating or emitting smoothScrollingToggled().
    void setSmoothScrolling(bool on);
    [[nodiscard]] ToggleSwitch* smoothScrollingSwitch() const noexcept { return smoothSwitch_; }

signals:
    void themeEntrySelected(const hitokoto::ui::ThemeEntry& entry);
    /// S043: the user flipped the switch "Smooth scrolling".
    void smoothScrollingToggled(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::ToolbarStyle style_{core::toolbarStyle(core::builtinTheme(core::ColorScheme::Light))};
    QVBoxLayout* mainLayout_ = nullptr;
    QGridLayout* appearanceLayout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* appearanceLabel_ = nullptr;
    SectionRule* appearanceRule_ = nullptr;
    QHBoxLayout* appearanceHeading_ = nullptr;
    QLabel* themeKeyLabel_ = nullptr;
    QComboBox* themeCombo_ = nullptr;
    QLabel* feelLabel_ = nullptr;
    SectionRule* feelRule_ = nullptr;
    QHBoxLayout* feelHeading_ = nullptr;
    QGridLayout* feelLayout_ = nullptr;
    QLabel* smoothKeyLabel_ = nullptr;
    ToggleSwitch* smoothSwitch_ = nullptr;
    QLabel* versionLabel_ = nullptr;
    QList<ThemeEntry> entries_;
};

}  // namespace hitokoto::ui
