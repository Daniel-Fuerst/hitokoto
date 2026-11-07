// SPDX-License-Identifier: GPL-3.0-or-later
// S028: the find bar below the editor (Firefox style): search field, previous and next, the
// toggles "Match case" and "Highlight all", the count and a close button. Not modal.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QWidget>

class QAction;
class QLabel;
class QLineEdit;
class QToolButton;

namespace hitokoto::ui {

class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QWidget* parent = nullptr);
    ~FindBar() override = default;
    FindBar(const FindBar&) = delete;
    FindBar& operator=(const FindBar&) = delete;

    void applyStyle(const core::FindStyle& style);
    [[nodiscard]] const core::FindStyle& findStyle() const noexcept { return style_; }

    [[nodiscard]] QLineEdit* searchField() const noexcept { return field_; }
    [[nodiscard]] QToolButton* previousButton() const noexcept { return previous_; }
    [[nodiscard]] QToolButton* nextButton() const noexcept { return next_; }
    [[nodiscard]] QToolButton* matchCaseToggle() const noexcept { return matchCase_; }
    [[nodiscard]] QToolButton* highlightAllToggle() const noexcept { return highlightAll_; }
    [[nodiscard]] QToolButton* closeButton() const noexcept { return close_; }
    [[nodiscard]] QLabel* countLabel() const noexcept { return count_; }
    /// The actions behind the toggles, with their platform shortcuts (S028 review O10).
    [[nodiscard]] QAction* matchCaseAction() const noexcept { return matchCaseAction_; }
    [[nodiscard]] QAction* highlightAllAction() const noexcept { return highlightAllAction_; }
    /// Marks the field when the whole document has been searched without a match.
    void setNoMatch(bool noMatch);
    [[nodiscard]] bool noMatch() const noexcept { return noMatch_; }

signals:
    void textChanged(const QString& text);
    void nextRequested();
    void previousRequested();
    void closeRequested();
    void matchCaseChanged(bool on);
    void highlightAllChanged(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateFieldStyle();

    core::FindStyle style_{core::findStyle(core::builtinTheme(core::ColorScheme::Light))};
    QLineEdit* field_ = nullptr;
    QToolButton* previous_ = nullptr;
    QToolButton* next_ = nullptr;
    QToolButton* matchCase_ = nullptr;
    QToolButton* highlightAll_ = nullptr;
    QToolButton* close_ = nullptr;
    QLabel* count_ = nullptr;
    QAction* matchCaseAction_ = nullptr;
    QAction* highlightAllAction_ = nullptr;
    bool noMatch_ = false;
};

}  // namespace hitokoto::ui
