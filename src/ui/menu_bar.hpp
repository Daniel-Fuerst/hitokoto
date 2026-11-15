// SPDX-License-Identifier: GPL-3.0-or-later
// S038: subtle top menu bar without a dividing line or persistent mnemonic underlines.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QMenuBar>

class QAction;
class QActionEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;

namespace hitokoto::ui {

class MenuBar final : public QMenuBar {
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);
    ~MenuBar() override = default;
    MenuBar(const MenuBar&) = delete;
    MenuBar& operator=(const MenuBar&) = delete;

    void applyStyle(const core::MenuBarStyle& style);
    [[nodiscard]] const core::MenuBarStyle& menuBarStyle() const noexcept { return style_; }

protected:
    bool event(QEvent* event) override;
    void actionEvent(QActionEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    core::MenuBarStyle style_{core::menuBarStyle(core::builtinTheme(core::ColorScheme::Light))};
    QAction* hoveredAction_ = nullptr;
    bool pressed_ = false;
};

}  // namespace hitokoto::ui
