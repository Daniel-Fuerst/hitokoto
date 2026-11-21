// SPDX-License-Identifier: GPL-3.0-or-later
// Non-modal floating settings flyout anchored beside the floating toolbar.
#pragma once

#include "hitokoto/core/theme.hpp"
#include "theme_manager.hpp"

#include <QList>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QFrame;
class QVBoxLayout;

namespace hitokoto::ui {

class ToggleSwitch;

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
    void setSmoothScrolling(bool on) noexcept;
    [[nodiscard]] QComboBox* themeSelector() const noexcept { return themeCombo_; }
    [[nodiscard]] ToggleSwitch* smoothScrollingToggle() const noexcept { return smoothScrollingToggle_; }
    [[nodiscard]] const QList<ThemeEntry>& themeEntries() const noexcept { return entries_; }
    [[nodiscard]] const core::ToolbarStyle& toolbarStyle() const noexcept { return style_; }

signals:
    void themeEntrySelected(const hitokoto::ui::ThemeEntry& entry);
    void smoothScrollingChanged(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::ToolbarStyle style_{core::toolbarStyle(core::builtinTheme(core::ColorScheme::Light))};
    QVBoxLayout* mainLayout_ = nullptr;
    QGridLayout* appearanceLayout_ = nullptr;
    QGridLayout* feelLayout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* appearanceLabel_ = nullptr;
    QLabel* themeKeyLabel_ = nullptr;
    QFrame* appearanceRule_ = nullptr;
    QComboBox* themeCombo_ = nullptr;
    QLabel* feelLabel_ = nullptr;
    QLabel* smoothScrollingKeyLabel_ = nullptr;
    QFrame* feelRule_ = nullptr;
    ToggleSwitch* smoothScrollingToggle_ = nullptr;
    QLabel* versionLabel_ = nullptr;
    QList<ThemeEntry> entries_;
};

}  // namespace hitokoto::ui
