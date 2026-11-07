// SPDX-License-Identifier: GPL-3.0-or-later
// S028: the find bar below the editor. Colours, sizes and fonts come from [find] (ADR 0004).
#include "find_bar.hpp"

#include "qt_bridge.hpp"

#include <QAction>
#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {
namespace {

enum class Icon { Previous, Next, MatchCase, HighlightAll, Close };

/// A button of the bar, drawn from the theme like the floating toolbar's (S026).
class BarButton final : public QToolButton {
public:
    BarButton(Icon icon, const core::FindStyle* style, QWidget* parent)
        : QToolButton(parent), icon_(icon), style_(style) {
        setFocusPolicy(Qt::TabFocus);  // code review: reachable with Tab, a click leaves the focus in the field
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        QToolButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent* event) override {
        QToolButton::leaveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool active = isDown() || isChecked();
        if (active || underMouse()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(toQColor(active ? style_->activeBackground : style_->hoverBackground));
            const qreal radius = std::clamp<qreal>(style_->buttonRadius, 0, std::min(width(), height()) / 2.0);
            painter.drawRoundedRect(QRectF(rect()), radius, radius);
        }
        const QColor color = toQColor(isChecked() ? style_->activeIcon : style_->icon);
        const qreal extent = std::min(width(), height()) * 0.5;
        const QRectF box((width() - extent) / 2.0, (height() - extent) / 2.0, extent, extent);
        const qreal stroke = std::max<qreal>(1.25, extent / 8.0);
        painter.setPen(QPen(color, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        switch (icon_) {
            case Icon::Previous:
            case Icon::Next: {
                const qreal top = box.top() + box.height() * 0.25;
                const qreal bottom = box.bottom() - box.height() * 0.25;
                QPainterPath chevron;
                if (icon_ == Icon::Previous) {
                    chevron.moveTo(box.left(), bottom);
                    chevron.lineTo(box.center().x(), top);
                    chevron.lineTo(box.right(), bottom);
                } else {
                    chevron.moveTo(box.left(), top);
                    chevron.lineTo(box.center().x(), bottom);
                    chevron.lineTo(box.right(), top);
                }
                painter.drawPath(chevron);
                break;
            }
            case Icon::MatchCase: {
                QFont f = font();
                f.setPixelSize(std::max(1, static_cast<int>(std::lround(extent))));
                painter.setFont(f);
                painter.drawText(QRectF(rect()), Qt::AlignCenter, QStringLiteral("Aa"));
                break;
            }
            case Icon::HighlightAll: {
                // A marker over a line of text.
                const qreal y = box.bottom() - stroke;
                painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
                QPainterPath tip;
                tip.moveTo(box.left() + box.width() * 0.25, y - stroke * 1.5);
                tip.lineTo(box.right() - box.width() * 0.15, box.top());
                tip.lineTo(box.right(), box.top() + box.height() * 0.2);
                tip.lineTo(box.left() + box.width() * 0.45, y - stroke * 1.5);
                tip.closeSubpath();
                painter.drawPath(tip);
                break;
            }
            case Icon::Close:
                painter.drawLine(box.topLeft(), box.bottomRight());
                painter.drawLine(box.topRight(), box.bottomLeft());
                break;
        }
    }

private:
    Icon icon_;
    const core::FindStyle* style_;
};

}  // namespace

FindBar::FindBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    field_ = new QLineEdit(this);
    field_->setAccessibleName(tr("Find"));
    field_->setPlaceholderText(tr("Find in file"));
    field_->setClearButtonEnabled(false);
    field_->installEventFilter(this);
    previous_ = new BarButton(Icon::Previous, &style_, this);
    previous_->setAccessibleName(tr("Previous match"));
    previous_->setToolTip(tr("Previous match (Shift+Enter)"));
    next_ = new BarButton(Icon::Next, &style_, this);
    next_->setAccessibleName(tr("Next match"));
    next_->setToolTip(tr("Next match (Enter)"));
    matchCase_ = new BarButton(Icon::MatchCase, &style_, this);
    highlightAll_ = new BarButton(Icon::HighlightAll, &style_, this);
    count_ = new QLabel(this);
    count_->setAccessibleName(tr("Matches"));
    close_ = new BarButton(Icon::Close, &style_, this);
    close_->setAccessibleName(tr("Close find bar"));
    close_->setToolTip(tr("Close (Escape)"));

    // Review O10: shortcuts that never take a text key from the field (Option+letter types on macOS).
#ifdef Q_OS_MACOS
    const QKeySequence matchCaseKey(Qt::CTRL | Qt::ALT | Qt::Key_C);
    const QKeySequence highlightAllKey(Qt::CTRL | Qt::ALT | Qt::Key_A);
#else
    const QKeySequence matchCaseKey(Qt::ALT | Qt::Key_C);
    const QKeySequence highlightAllKey(Qt::ALT | Qt::Key_A);
#endif
    matchCaseAction_ = new QAction(tr("Match case"), this);
    matchCaseAction_->setCheckable(true);
    matchCaseAction_->setShortcut(matchCaseKey);
    matchCaseAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    matchCaseAction_->setToolTip(tr("Match case (%1)").arg(matchCaseKey.toString(QKeySequence::NativeText)));
    addAction(matchCaseAction_);
    matchCase_->setDefaultAction(matchCaseAction_);
    matchCase_->setAccessibleName(tr("Match case"));

    highlightAllAction_ = new QAction(tr("Highlight all"), this);
    highlightAllAction_->setCheckable(true);
    highlightAllAction_->setShortcut(highlightAllKey);
    highlightAllAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    highlightAllAction_->setToolTip(tr("Highlight all (%1)").arg(highlightAllKey.toString(QKeySequence::NativeText)));
    addAction(highlightAllAction_);
    highlightAll_->setDefaultAction(highlightAllAction_);
    highlightAll_->setAccessibleName(tr("Highlight all"));

    layout->addWidget(field_, 1);
    layout->addWidget(previous_);
    layout->addWidget(next_);
    layout->addWidget(matchCase_);
    layout->addWidget(highlightAll_);
    layout->addWidget(count_);
    layout->addStretch(0);
    layout->addWidget(close_);

    connect(field_, &QLineEdit::textChanged, this, &FindBar::textChanged);
    connect(previous_, &QToolButton::clicked, this, &FindBar::previousRequested);
    connect(next_, &QToolButton::clicked, this, &FindBar::nextRequested);
    connect(close_, &QToolButton::clicked, this, &FindBar::closeRequested);
    connect(matchCaseAction_, &QAction::toggled, this, &FindBar::matchCaseChanged);
    connect(highlightAllAction_, &QAction::toggled, this, &FindBar::highlightAllChanged);

    applyStyle(style_);
    hide();
}

void FindBar::applyStyle(const core::FindStyle& style) {
    style_ = style;
    const auto padding = static_cast<int>(std::lround(style_.padding));
    const auto border = static_cast<int>(std::lround(style_.borderWidth));
    if (auto* l = qobject_cast<QHBoxLayout*>(layout())) {
        l->setContentsMargins(padding, padding + border, padding, padding);
        l->setSpacing(static_cast<int>(std::lround(style_.spacing)));
    }
    const auto size = static_cast<int>(std::lround(style_.buttonSize));
    for (QToolButton* b : {previous_, next_, matchCase_, highlightAll_, close_}) {
        b->setFixedSize(size, size);
    }
    QFont f = font();
    if (!style_.fontFamily.empty()) {
        f.setFamily(QString::fromStdString(style_.fontFamily));
    }
    if (f.pointSizeF() > 0) {
        f.setPointSizeF(f.pointSizeF() * style_.fontScale);
    }
    field_->setFont(f);
    count_->setFont(f);
    QPalette countPalette = count_->palette();
    countPalette.setColor(QPalette::WindowText, toQColor(style_.text));
    count_->setPalette(countPalette);
    updateFieldStyle();
    update();
}

void FindBar::setNoMatch(bool noMatch) {
    if (noMatch_ == noMatch) {
        return;
    }
    noMatch_ = noMatch;
    updateFieldStyle();
}

void FindBar::updateFieldStyle() {
    const QColor background = toQColor(noMatch_ ? style_.noMatchBackground : style_.fieldBackground);
    field_->setStyleSheet(QStringLiteral("QLineEdit { background: %1; color: %2; border: %3px solid %4; "
                                         "border-radius: %5px; padding: %6px; }"
                                         "QLineEdit:focus { border-color: %7; }")
                              .arg(background.name(QColor::HexArgb), toQColor(style_.fieldText).name(QColor::HexArgb))
                              .arg(style_.fieldBorderWidth)
                              .arg(toQColor(style_.fieldBorder).name(QColor::HexArgb))
                              .arg(style_.fieldRadius)
                              .arg(std::max(0.0, style_.spacing / 4))
                              .arg(toQColor(style_.fieldFocusBorder).name(QColor::HexArgb)));
}

void FindBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), toQColor(style_.background));
    const qreal border = std::max(0.0, style_.borderWidth);
    if (border > 0) {
        painter.fillRect(QRectF(0, 0, width(), border), toQColor(style_.border));
    }
}

bool FindBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == field_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if ((key->modifiers() & Qt::ShiftModifier) != 0) {
                emit previousRequested();
            } else {
                emit nextRequested();
            }
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            emit closeRequested();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace hitokoto::ui
