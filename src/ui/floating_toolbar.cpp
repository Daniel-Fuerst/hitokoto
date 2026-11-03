// SPDX-License-Identifier: GPL-3.0-or-later
#include "floating_toolbar.hpp"

#include "qt_bridge.hpp"

#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {
namespace {

enum class IconKind { Table, InlineCode };

class ToolbarButton final : public QToolButton {
public:
    ToolbarButton(IconKind kind, const core::ToolbarStyle* style, QWidget* parent)
        : QToolButton(parent), kind_(kind), style_(style) {
        setFocusPolicy(Qt::NoFocus);
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
        if (style_ == nullptr) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool pressed = isDown();
        const bool hovered = underMouse();
        if (pressed || hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(toQColor(pressed ? style_->activeBackground : style_->hoverBackground));
            const qreal radius = std::clamp<qreal>(style_->buttonRadius, 0, std::min(width(), height()) / 2.0);
            painter.drawRoundedRect(QRectF(rect()), radius, radius);
        }

        const QColor iconColor = toQColor(pressed ? style_->activeIcon : style_->icon);
        const qreal extent = std::min(width(), height()) * 0.56;
        const QRectF box((width() - extent) / 2.0, (height() - extent) / 2.0, extent, extent);
        const qreal stroke = std::max<qreal>(1.25, extent / 10.0);
        painter.setPen(QPen(iconColor, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);

        if (kind_ == IconKind::Table) {
            const qreal r = std::max<qreal>(1.5, stroke * 1.2);
            painter.drawRoundedRect(box, r, r);
            const qreal yHeader = box.top() + box.height() * 0.36;
            const qreal yRow = box.top() + box.height() * 0.68;
            const qreal xMid = box.center().x();
            painter.drawLine(QPointF(box.left(), yHeader), QPointF(box.right(), yHeader));
            painter.drawLine(QPointF(box.left(), yRow), QPointF(box.right(), yRow));
            painter.drawLine(QPointF(xMid, box.top()), QPointF(xMid, box.bottom()));
        } else {
            const qreal ym = box.center().y();
            const qreal xm = box.center().x();
            const qreal w = box.width();
            const qreal h = box.height();

            QPainterPath leftChevron;
            leftChevron.moveTo(box.left() + w * 0.30, ym - h * 0.24);
            leftChevron.lineTo(box.left() + w * 0.06, ym);
            leftChevron.lineTo(box.left() + w * 0.30, ym + h * 0.24);
            painter.drawPath(leftChevron);

            QPainterPath rightChevron;
            rightChevron.moveTo(box.right() - w * 0.30, ym - h * 0.24);
            rightChevron.lineTo(box.right() - w * 0.06, ym);
            rightChevron.lineTo(box.right() - w * 0.30, ym + h * 0.24);
            painter.drawPath(rightChevron);

            painter.drawLine(QPointF(xm + w * 0.09, ym - h * 0.28), QPointF(xm - w * 0.09, ym + h * 0.28));
        }
    }

private:
    IconKind kind_;
    const core::ToolbarStyle* style_;
};

}  // namespace

FloatingToolbar::FloatingToolbar(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(tr("Formatting toolbar"));

    tableButton_ = new ToolbarButton(IconKind::Table, &style_, this);
    tableButton_->setObjectName(QStringLiteral("createTableButton"));
    tableButton_->setToolTip(tr("Create new table"));
    tableButton_->setAccessibleName(tr("Create new table"));
    connect(tableButton_, &QToolButton::clicked, this, &FloatingToolbar::createTableRequested);

    inlineCodeButton_ = new ToolbarButton(IconKind::InlineCode, &style_, this);
    inlineCodeButton_->setObjectName(QStringLiteral("createInlineCodeButton"));
    inlineCodeButton_->setToolTip(tr("Create inline code"));
    inlineCodeButton_->setAccessibleName(tr("Create inline code"));
    connect(inlineCodeButton_, &QToolButton::clicked, this, &FloatingToolbar::createInlineCodeRequested);

    updateLayout();
}

void FloatingToolbar::applyStyle(const core::ToolbarStyle& style) {
    style_ = style;
    updateLayout();
    update();
    tableButton_->update();
    inlineCodeButton_->update();
}

void FloatingToolbar::updateLayout() {
    const int btnSize = std::max(16, static_cast<int>(std::lround(style_.buttonSize)));
    const int pad = std::max(0, static_cast<int>(std::lround(style_.padding)));
    const int gap = std::max(0, static_cast<int>(std::lround(style_.spacing)));

    tableButton_->setGeometry(pad, pad, btnSize, btnSize);
    inlineCodeButton_->setGeometry(pad, pad + btnSize + gap, btnSize, btnSize);
    setFixedSize(btnSize + 2 * pad, 2 * btnSize + gap + 2 * pad);
}

void FloatingToolbar::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal border = style_.borderWidth;
    painter.setPen(border > 0 ? QPen(toQColor(style_.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(style_.background));
    const QRectF inner = QRectF(rect()).adjusted(border / 2.0, border / 2.0, -border / 2.0, -border / 2.0);
    const qreal radius = std::max<qreal>(0, style_.radius - border / 2.0);
    painter.drawRoundedRect(inner, radius, radius);
}

}  // namespace hitokoto::ui
