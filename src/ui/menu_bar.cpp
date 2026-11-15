// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu_bar.hpp"

#include "qt_bridge.hpp"

#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QEvent>
#include <QHoverEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {
namespace {

QString px(double v) {
    return QString::number(std::lround(v)) + QStringLiteral("px");
}

QString hex(const core::Color& c) {
    return toQColor(c).name(c.a == 255 ? QColor::HexRgb : QColor::HexArgb);
}

QString cleanLabel(const QString& text) {
    QString out;
    out.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i] == u'&') {
            if (i + 1 < text.size() && text[i + 1] == u'&') {
                out.append(u'&');
                ++i;
            }
            continue;
        }
        out.append(text[i]);
    }
    return out;
}

}  // namespace

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent) {
    setNativeMenuBar(false);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    applyStyle(style_);
}

void MenuBar::applyStyle(const core::MenuBarStyle& style) {
    style_ = style;

    const QString pad = px(std::max(0.0, style_.padding));
    const QString gap = px(std::max(0.0, style_.spacing));
    const QString border = px(std::max(0.0, style_.borderWidth));
    const QString padV = px(std::max(0.0, style_.itemPaddingVertical));
    const QString padH = px(std::max(0.0, style_.itemPaddingHorizontal));
    const QString itemRadius = px(std::max(0.0, style_.itemRadius));

    setStyleSheet(QStringLiteral("QMenuBar { background: %1; color: %2; border: none; border-bottom: %3 solid %4; "
                                 "padding: %5; spacing: %6; }"
                                 "QMenuBar::item { background: transparent; padding: %7 %8; margin: 0px; "
                                 "border-radius: %9; }")
                      .arg(hex(style_.background), hex(style_.text), border, hex(style_.border), pad, gap, padV, padH,
                           itemRadius));

    QFont f = QApplication::font();
    if (!style_.fontFamily.empty()) {
        f.setFamilies({QString::fromStdString(style_.fontFamily)});
    }
    const qreal scale = style_.fontScale;
    if (f.pointSizeF() > 0) {
        f.setPointSizeF(f.pointSizeF() * scale);
    } else if (f.pixelSize() > 0) {
        f.setPixelSize(std::max(1, static_cast<int>(std::lround(f.pixelSize() * scale))));
    }
    setFont(f);

    updateGeometry();
    update();
}

bool MenuBar::event(QEvent* event) {
    if (event->type() == QEvent::HoverMove) {
        const auto* hover = static_cast<const QHoverEvent*>(event);
        QAction* action = actionAt(hover->position().toPoint());
        if (action != hoveredAction_) {
            hoveredAction_ = action;
            setCursor(action != nullptr ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    } else if (event->type() == QEvent::HoverLeave) {
        if (hoveredAction_ != nullptr) {
            hoveredAction_ = nullptr;
            setCursor(Qt::ArrowCursor);
            update();
        }
    }
    return QMenuBar::event(event);
}

void MenuBar::actionEvent(QActionEvent* event) {
    QMenuBar::actionEvent(event);
    if (event->type() == QEvent::ActionAdded && event->action() != nullptr) {
        if (QMenu* menu = event->action()->menu()) {
            connect(menu, &QMenu::aboutToShow, this, qOverload<>(&QWidget::update), Qt::UniqueConnection);
            connect(menu, &QMenu::aboutToHide, this, &MenuBar::resetAfterMenuClosed, Qt::UniqueConnection);
        }
    }
}

void MenuBar::mouseMoveEvent(QMouseEvent* event) {
    QAction* action = actionAt(event->pos());
    if (action != hoveredAction_) {
        hoveredAction_ = action;
        setCursor(action != nullptr ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QMenuBar::mouseMoveEvent(event);
}

void MenuBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pressed_ = true;
        update();
    }
    QMenuBar::mousePressEvent(event);
}

void MenuBar::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pressed_ = false;
        update();
    }
    QMenuBar::mouseReleaseEvent(event);
}

void MenuBar::leaveEvent(QEvent* event) {
    if (hoveredAction_ != nullptr) {
        hoveredAction_ = nullptr;
        setCursor(Qt::ArrowCursor);
        update();
    }
    QMenuBar::leaveEvent(event);
}

void MenuBar::resetAfterMenuClosed() {
    // A click outside the popup closes it without the bar ever seeing the release or a leave event,
    // so the pressed and hover state are recomputed from the real cursor once the popup is gone.
    QTimer::singleShot(0, this, [this] {
        pressed_ = false;
        const QPoint local = mapFromGlobal(QCursor::pos());
        hoveredAction_ = rect().contains(local) ? actionAt(local) : nullptr;
        setCursor(hoveredAction_ != nullptr ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    });
}

void MenuBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), toQColor(style_.background));

    const qreal border = std::max(0.0, style_.borderWidth);
    if (border > 0) {
        painter.fillRect(QRectF(0, height() - border, width(), border), toQColor(style_.border));
    }

    painter.setFont(font());
    for (QAction* action : actions()) {
        if (action == nullptr || !action->isVisible() || action->isSeparator()) {
            continue;
        }
        const QRect itemRect = actionGeometry(action);
        if (!itemRect.isValid() || itemRect.isEmpty()) {
            continue;
        }
        const bool isOpen =
            (action->menu() != nullptr && action->menu()->isVisible()) || (pressed_ && activeAction() == action);
        // activeAction() lingers after a popup is dismissed by clicking elsewhere; it only counts as a
        // highlight while the bar is in keyboard navigation (Alt / F10), which gives it focus.
        const bool isHovered = !isOpen && (hoveredAction_ == action || (hasFocus() && activeAction() == action));
        if (isOpen || isHovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(toQColor(isOpen ? style_.activeBackground : style_.hoverBackground));
            const qreal radius =
                std::clamp<qreal>(style_.itemRadius, 0.0, std::min(itemRect.width(), itemRect.height()) / 2.0);
            painter.drawRoundedRect(QRectF(itemRect), radius, radius);
        }
        const core::Color& textColor = isOpen ? style_.activeText : (isHovered ? style_.hoverText : style_.text);
        painter.setPen(toQColor(textColor));
        painter.drawText(QRectF(itemRect), Qt::AlignCenter, cleanLabel(action->text()));
    }
}

}  // namespace hitokoto::ui
