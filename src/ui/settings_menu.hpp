// SPDX-License-Identifier: GPL-3.0-or-later
// Non-modal floating settings flyout anchored beside the floating toolbar.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QWidget>

#include <array>

class QGridLayout;
class QLabel;
class QVBoxLayout;

namespace hitokoto::ui {

class SettingsMenu : public QWidget {
    Q_OBJECT
public:
    explicit SettingsMenu(QWidget* parent = nullptr);
    ~SettingsMenu() override = default;
    SettingsMenu(const SettingsMenu&) = delete;
    SettingsMenu& operator=(const SettingsMenu&) = delete;

    void applyTheme(const core::Theme& theme);
    void setDocumentStats(int lineCount, int charCount);
    [[nodiscard]] const core::ToolbarStyle& toolbarStyle() const noexcept { return style_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::ToolbarStyle style_{core::toolbarStyle(core::builtinTheme(core::ColorScheme::Light))};
    QVBoxLayout* mainLayout_ = nullptr;
    QGridLayout* gridLayout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* sectionLabel_ = nullptr;
    std::array<QLabel*, 5> keyLabels_{};
    QLabel* appValue_ = nullptr;
    QLabel* qtValue_ = nullptr;
    QLabel* platformValue_ = nullptr;
    QLabel* themeValue_ = nullptr;
    QLabel* documentValue_ = nullptr;
};

}  // namespace hitokoto::ui
