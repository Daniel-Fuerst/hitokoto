// SPDX-License-Identifier: GPL-3.0-or-later
// S028: the find bar. Stub for the red tests; the implementation follows.
#include "find_bar.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace hitokoto::ui {

FindBar::FindBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    field_ = new QLineEdit(this);
    previous_ = new QToolButton(this);
    next_ = new QToolButton(this);
    matchCase_ = new QToolButton(this);
    highlightAll_ = new QToolButton(this);
    count_ = new QLabel(this);
    close_ = new QToolButton(this);
    matchCaseAction_ = new QAction(this);
    highlightAllAction_ = new QAction(this);
    for (QWidget* w :
         std::initializer_list<QWidget*>{field_, previous_, next_, matchCase_, highlightAll_, count_, close_}) {
        layout->addWidget(w);
    }
    hide();
}

void FindBar::applyStyle(const core::FindStyle& style) {
    style_ = style;
}

void FindBar::setNoMatch(bool noMatch) {
    noMatch_ = noMatch;
}

}  // namespace hitokoto::ui
