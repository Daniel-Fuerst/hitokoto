// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu_bar.hpp"

#include "qt_bridge.hpp"

#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QHoverEvent>
#include <QKeyEvent>
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

/// The label without `&` markers, and the index of its access key in it (-1 if it has none).
struct Label {
    QString text;
    qsizetype mnemonic = -1;
};

Label parseLabel(const QString& text) {
    Label out;
    out.text.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i] == u'&') {
            if (i + 1 < text.size() && text[i + 1] == u'&') {
                out.text.append(u'&');
                ++i;
            } else if (i + 1 < text.size() && out.mnemonic < 0) {
                out.mnemonic = out.text.size();
            }
            continue;
        }
        out.text.append(text[i]);
    }
    return out;
}

}  // namespace

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent) {
    setNativeMenuBar(false);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    qApp->installEventFilter(this);
    applyStyle(style_);
}

bool MenuBar::eventFilter(QObject* watched, QEvent* event) {
    // Access keys are hinted only while Alt is held (or the bar is keyboard-driven, see
    // mnemonicHintsVisible()), so the bar stays quiet the rest of the time.
    bool held = altHeld_;
    switch (event->type()) {
        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            const auto* key = static_cast<const QKeyEvent*>(event);
            held = key->key() == Qt::Key_Alt ? event->type() == QEvent::KeyPress
                                             : key->modifiers().testFlag(Qt::AltModifier);
            break;
        }
        case QEvent::ApplicationDeactivate:
        case QEvent::WindowDeactivate:
            held = false;
            break;
        default:
            break;
    }
    if (held != altHeld_) {
        altHeld_ = held;
        update();
    }
    return QMenuBar::eventFilter(watched, event);
}

void MenuBar::focusInEvent(QFocusEvent* event) {
    QMenuBar::focusInEvent(event);
    update();
}

void MenuBar::focusOutEvent(QFocusEvent* event) {
    QMenuBar::focusOutEvent(event);
    update();
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
    const bool hints = mnemonicHintsVisible();
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
        const Label label = parseLabel(action->text());
        painter.setPen(toQColor(textColor));
        if (!hints || label.mnemonic < 0) {
            painter.drawText(QRectF(itemRect), Qt::AlignCenter, label.text);
            continue;
        }

        // Access-key hint: the key letter takes the mnemonic colour and gets a short, rounded
        // underline just below the baseline, instead of the classic full-weight underline.
        const QFontMetricsF fm(font());
        const QString before = label.text.left(label.mnemonic);
        const QString key = label.text.mid(label.mnemonic, 1);
        const QString after = label.text.mid(label.mnemonic + 1);
        const qreal x0 = itemRect.center().x() + 0.5 - fm.horizontalAdvance(label.text) / 2.0;
        const qreal baseline = itemRect.center().y() + 0.5 + (fm.ascent() - fm.descent()) / 2.0;
        const qreal xKey = x0 + fm.horizontalAdvance(before);
        const qreal keyWidth = fm.horizontalAdvance(key);
        painter.drawText(QPointF(x0, baseline), before);
        painter.drawText(QPointF(xKey + keyWidth, baseline), after);
        const QColor accent = toQColor(style_.mnemonic);
        painter.setPen(accent);
        painter.drawText(QPointF(xKey, baseline), key);

        const qreal thickness = std::max<qreal>(1.0, fm.lineWidth());
        const qreal inset = keyWidth * 0.15;
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(QRectF(xKey + inset, baseline + fm.underlinePos(), keyWidth - 2 * inset, thickness),
                                thickness / 2, thickness / 2);
    }
}

}  // namespace hitokoto::ui
