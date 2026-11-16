// SPDX-License-Identifier: GPL-3.0-or-later
#include "floating_toolbar.hpp"

#include "qt_bridge.hpp"

#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace hitokoto::ui {
namespace {

enum class IconKind { Table, InlineCode, Settings };

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

void paintSettingsIcon(QPainter& painter, const QRectF& box, qreal extent) {
    const QPointF center = box.center();
    const qreal axleRadius = extent * 0.16;
    painter.drawEllipse(center, axleRadius, axleRadius);

    constexpr int kTeeth = 8;
    constexpr qreal kToothStep = 2.0 * std::numbers::pi / kTeeth;
    const qreal rootRadius = extent * 0.34;
    const qreal tipRadius = extent * 0.46;

    auto polar = [&center](qreal angle, qreal radius) {
        return QPointF(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius);
    };

    QPainterPath gear;
    for (int i = 0; i < kTeeth; ++i) {
        const qreal base = i * kToothStep;
        const QPointF rootStart = polar(base - 0.30 * kToothStep, rootRadius);
        if (i == 0) {
            gear.moveTo(rootStart);
        } else {
            gear.lineTo(rootStart);
        }
        gear.lineTo(polar(base - 0.16 * kToothStep, tipRadius));
        gear.lineTo(polar(base + 0.16 * kToothStep, tipRadius));
        gear.lineTo(polar(base + 0.30 * kToothStep, rootRadius));
    }
    gear.closeSubpath();
    painter.drawPath(gear);
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
            case IconKind::Table:
                paintTableIcon(painter, box, stroke);
                break;
            case IconKind::InlineCode:
                paintInlineCodeIcon(painter, box);
                break;
            case IconKind::Settings:
                paintSettingsIcon(painter, box, extent);
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

    settingsButton_ = new ToolbarButton(IconKind::Settings, style_, this);
    settingsButton_->setObjectName(QStringLiteral("openSettingsButton"));
    settingsButton_->setCheckable(true);
    settingsButton_->setToolTip(tr("Open settings"));
    settingsButton_->setAccessibleName(tr("Open settings"));
    connect(settingsButton_, &QToolButton::clicked, this, &FloatingToolbar::openSettingsRequested);

    updateLayout();
}

void FloatingToolbar::setSettingsOpen(bool open) {
    settingsButton_->setChecked(open);
    settingsButton_->update();
}

bool FloatingToolbar::isSettingsOpen() const {
    return settingsButton_->isChecked();
}

void FloatingToolbar::applyStyle(const core::ToolbarStyle& style) {
    style_ = style;
    updateLayout();
    update();
    tableButton_->update();
    inlineCodeButton_->update();
    settingsButton_->update();
}

void FloatingToolbar::updateLayout() {
    const int btnSize = std::max(16, static_cast<int>(std::lround(style_.buttonSize)));
    const int pad = std::max(0, static_cast<int>(std::lround(style_.padding)));
    const int gap = std::max(0, static_cast<int>(std::lround(style_.spacing)));

    tableButton_->setGeometry(pad, pad, btnSize, btnSize);
    inlineCodeButton_->setGeometry(pad, pad + btnSize + gap, btnSize, btnSize);
    settingsButton_->setGeometry(pad, pad + 2 * (btnSize + gap), btnSize, btnSize);
    setFixedSize(btnSize + 2 * pad, 3 * btnSize + 2 * gap + 2 * pad);
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
