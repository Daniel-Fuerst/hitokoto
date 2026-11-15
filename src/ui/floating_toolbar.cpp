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

enum class IconKind { NewFile, OpenFile, SaveFile, Find, Table, InlineCode };

void paintTableIcon(QPainter& painter, const QRectF& box, qreal stroke) {
    const qreal r = std::max<qreal>(1.5, stroke * 1.2);
    painter.drawRoundedRect(box, r, r);
    const qreal yHeader = box.top() + box.height() * 0.36;
    const qreal yRow = box.top() + box.height() * 0.68;
    const qreal xMid = box.center().x();
    painter.drawLine(QPointF(box.left(), yHeader), QPointF(box.right(), yHeader));
    painter.drawLine(QPointF(box.left(), yRow), QPointF(box.right(), yRow));
    painter.drawLine(QPointF(xMid, box.top()), QPointF(xMid, box.bottom()));
}

void paintInlineCodeIcon(QPainter& painter, const QRectF& box) {
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

void paintNewFileIcon(QPainter& painter, const QRectF& box) {
    const qreal w = box.width();
    const qreal h = box.height();
    const qreal left = box.left() + w * 0.14;
    const qreal right = box.right() - w * 0.14;
    const qreal top = box.top() + h * 0.04;
    const qreal bottom = box.bottom() - h * 0.04;
    const qreal fold = w * 0.26;

    QPainterPath page;
    page.moveTo(left, top);
    page.lineTo(right - fold, top);
    page.lineTo(right, top + fold);
    page.lineTo(right, bottom);
    page.lineTo(left, bottom);
    page.closeSubpath();
    painter.drawPath(page);

    QPainterPath crease;
    crease.moveTo(right - fold, top);
    crease.lineTo(right - fold, top + fold);
    crease.lineTo(right, top + fold);
    painter.drawPath(crease);

    const qreal cx = (left + right) / 2.0;
    const qreal cy = top + (bottom - top) * 0.60;
    const qreal r = w * 0.15;
    painter.drawLine(QPointF(cx - r, cy), QPointF(cx + r, cy));
    painter.drawLine(QPointF(cx, cy - r), QPointF(cx, cy + r));
}

void paintOpenFileIcon(QPainter& painter, const QRectF& box) {
    const qreal w = box.width();
    const qreal h = box.height();
    const qreal left = box.left() + w * 0.10;
    const qreal right = box.right() - w * 0.16;
    const qreal top = box.top() + h * 0.04;
    const qreal bottom = box.bottom() - h * 0.04;
    const qreal fold = w * 0.26;

    QPainterPath page;
    page.moveTo(box.left() + w * 0.48, bottom);
    page.lineTo(left, bottom);
    page.lineTo(left, top);
    page.lineTo(right - fold, top);
    page.lineTo(right, top + fold);
    page.lineTo(right, box.top() + h * 0.50);
    painter.drawPath(page);

    QPainterPath crease;
    crease.moveTo(right - fold, top);
    crease.lineTo(right - fold, top + fold);
    crease.lineTo(right, top + fold);
    painter.drawPath(crease);

    const qreal tipX = box.left() + w * 0.62;
    const qreal tipY = box.top() + h * 0.62;
    const qreal arm = w * 0.24;
    QPainterPath arrowHead;
    arrowHead.moveTo(tipX, tipY + arm);
    arrowHead.lineTo(tipX, tipY);
    arrowHead.lineTo(tipX + arm, tipY);
    painter.drawPath(arrowHead);
    painter.drawLine(QPointF(tipX, tipY), QPointF(box.right() - w * 0.04, bottom));
}

void paintSaveFileIcon(QPainter& painter, const QRectF& box) {
    const qreal w = box.width();
    const qreal h = box.height();
    const qreal left = box.left() + w * 0.08;
    const qreal right = box.right() - w * 0.08;
    const qreal top = box.top() + h * 0.08;
    const qreal bottom = box.bottom() - h * 0.08;
    const qreal cut = w * 0.20;

    QPainterPath body;
    body.moveTo(left, top);
    body.lineTo(right - cut, top);
    body.lineTo(right, top + cut);
    body.lineTo(right, bottom);
    body.lineTo(left, bottom);
    body.closeSubpath();
    painter.drawPath(body);

    QPainterPath shutter;
    shutter.moveTo(left + w * 0.18, top);
    shutter.lineTo(left + w * 0.18, top + h * 0.28);
    shutter.lineTo(right - cut - w * 0.04, top + h * 0.28);
    shutter.lineTo(right - cut - w * 0.04, top);
    painter.drawPath(shutter);

    QPainterPath label;
    label.moveTo(left + w * 0.16, bottom);
    label.lineTo(left + w * 0.16, bottom - h * 0.36);
    label.lineTo(right - w * 0.16, bottom - h * 0.36);
    label.lineTo(right - w * 0.16, bottom);
    painter.drawPath(label);
}

void paintFindIcon(QPainter& painter, const QRectF& box) {
    const qreal w = box.width();
    const qreal h = box.height();
    const qreal cx = box.left() + w * 0.42;
    const qreal cy = box.top() + h * 0.42;
    const qreal r = std::min(w, h) * 0.32;
    painter.drawEllipse(QPointF(cx, cy), r, r);
    painter.drawLine(QPointF(cx + r * 0.7071, cy + r * 0.7071),
                     QPointF(box.right() - w * 0.08, box.bottom() - h * 0.08));
}

class ToolbarButton final : public QToolButton {
public:
    ToolbarButton(IconKind kind, const core::ToolbarStyle& style, QWidget* parent)
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
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const bool active = isDown() || isChecked();
        const bool hovered = underMouse();
        if (active || hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(toQColor(active ? style_.activeBackground : style_.hoverBackground));
            const qreal radius = std::clamp<qreal>(style_.buttonRadius, 0, std::min(width(), height()) / 2.0);
            painter.drawRoundedRect(QRectF(rect()), radius, radius);
        }

        const QColor iconColor = toQColor(active ? style_.activeIcon : style_.icon);
        const qreal extent = std::min(width(), height()) * 0.56;
        const QRectF box((width() - extent) / 2.0, (height() - extent) / 2.0, extent, extent);
        const qreal stroke = std::max<qreal>(1.25, extent / 10.0);
        painter.setPen(QPen(iconColor, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);

        switch (kind_) {
            case IconKind::NewFile:
                paintNewFileIcon(painter, box);
                break;
            case IconKind::OpenFile:
                paintOpenFileIcon(painter, box);
                break;
            case IconKind::SaveFile:
                paintSaveFileIcon(painter, box);
                break;
            case IconKind::Find:
                paintFindIcon(painter, box);
                break;
            case IconKind::Table:
                paintTableIcon(painter, box, stroke);
                break;
            case IconKind::InlineCode:
                paintInlineCodeIcon(painter, box);
                break;
        }
    }

private:
    IconKind kind_;
    const core::ToolbarStyle& style_;
};

}  // namespace

FloatingToolbar::FloatingToolbar(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(tr("Formatting toolbar"));

    newFileButton_ = new ToolbarButton(IconKind::NewFile, style_, this);
    newFileButton_->setObjectName(QStringLiteral("newFileButton"));
    newFileButton_->setToolTip(tr("New"));
    newFileButton_->setAccessibleName(tr("New"));
    connect(newFileButton_, &QToolButton::clicked, this, &FloatingToolbar::newFileRequested);

    openFileButton_ = new ToolbarButton(IconKind::OpenFile, style_, this);
    openFileButton_->setObjectName(QStringLiteral("openFileButton"));
    openFileButton_->setToolTip(tr("Open"));
    openFileButton_->setAccessibleName(tr("Open"));
    connect(openFileButton_, &QToolButton::clicked, this, &FloatingToolbar::openFileRequested);

    saveFileButton_ = new ToolbarButton(IconKind::SaveFile, style_, this);
    saveFileButton_->setObjectName(QStringLiteral("saveFileButton"));
    saveFileButton_->setToolTip(tr("Save"));
    saveFileButton_->setAccessibleName(tr("Save"));
    connect(saveFileButton_, &QToolButton::clicked, this, &FloatingToolbar::saveFileRequested);

    findButton_ = new ToolbarButton(IconKind::Find, style_, this);
    findButton_->setObjectName(QStringLiteral("findButton"));
    findButton_->setToolTip(tr("Find"));
    findButton_->setAccessibleName(tr("Find"));
    connect(findButton_, &QToolButton::clicked, this, &FloatingToolbar::findRequested);

    tableButton_ = new ToolbarButton(IconKind::Table, style_, this);
    tableButton_->setObjectName(QStringLiteral("createTableButton"));
    tableButton_->setToolTip(tr("Create new table"));
    tableButton_->setAccessibleName(tr("Create new table"));
    connect(tableButton_, &QToolButton::clicked, this, &FloatingToolbar::createTableRequested);

    inlineCodeButton_ = new ToolbarButton(IconKind::InlineCode, style_, this);
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
    newFileButton_->update();
    openFileButton_->update();
    saveFileButton_->update();
    findButton_->update();
    tableButton_->update();
    inlineCodeButton_->update();
}

void FloatingToolbar::updateLayout() {
    const int btnSize = std::max(16, static_cast<int>(std::lround(style_.buttonSize)));
    const int pad = std::max(0, static_cast<int>(std::lround(style_.padding)));
    const int gap = std::max(0, static_cast<int>(std::lround(style_.spacing)));

    newFileButton_->setGeometry(pad, pad, btnSize, btnSize);
    openFileButton_->setGeometry(pad, pad + btnSize + gap, btnSize, btnSize);
    saveFileButton_->setGeometry(pad, pad + 2 * (btnSize + gap), btnSize, btnSize);
    findButton_->setGeometry(pad, pad + 3 * (btnSize + gap), btnSize, btnSize);
    tableButton_->setGeometry(pad, pad + 4 * (btnSize + gap), btnSize, btnSize);
    inlineCodeButton_->setGeometry(pad, pad + 5 * (btnSize + gap), btnSize, btnSize);
    setFixedSize(btnSize + 2 * pad, 6 * btnSize + 5 * gap + 2 * pad);
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
