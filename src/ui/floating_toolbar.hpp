// SPDX-License-Identifier: GPL-3.0-or-later
// S026: vertical floating toolbar on the right-hand side of the editor for quick insertion of
// Markdown elements (tables, inline code).
// Navigator actions: New, Open, Save, Find.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QWidget>

class QToolButton;

namespace hitokoto::ui {

class FloatingToolbar : public QWidget {
    Q_OBJECT
public:
    explicit FloatingToolbar(QWidget* parent = nullptr);
    ~FloatingToolbar() override = default;
    FloatingToolbar(const FloatingToolbar&) = delete;
    FloatingToolbar& operator=(const FloatingToolbar&) = delete;

    void applyStyle(const core::ToolbarStyle& style);
    [[nodiscard]] const core::ToolbarStyle& toolbarStyle() const noexcept { return style_; }

signals:
    void newFileRequested();
    void openFileRequested();
    void saveFileRequested();
    void findRequested();
    void createTableRequested();
    void createInlineCodeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void updateLayout();

    core::ToolbarStyle style_{core::toolbarStyle(core::builtinTheme(core::ColorScheme::Light))};
    QToolButton* newFileButton_ = nullptr;
    QToolButton* openFileButton_ = nullptr;
    QToolButton* saveFileButton_ = nullptr;
    QToolButton* findButton_ = nullptr;
    QToolButton* tableButton_ = nullptr;
    QToolButton* inlineCodeButton_ = nullptr;
};

}  // namespace hitokoto::ui
