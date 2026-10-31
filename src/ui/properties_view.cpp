// SPDX-License-Identifier: GPL-3.0-or-later
#include "properties_view.hpp"

#include "qt_bridge.hpp"

#include <QAccessibleWidget>
#include <QDate>
#include <QDateTime>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTextLayout>
#include <QTime>
#include <QToolTip>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

namespace {

constexpr int kJsonLines = 3;  ///< S024 review O1: an unsupported value shows at most three lines
constexpr int kTextFlags = Qt::TextWordWrap | Qt::TextWrapAnywhere;
const QString kEllipsis = QStringLiteral("…");

/// The first `maxLines` lines of `text` wrapped at `width`; the last one ends in an ellipsis if
/// more text follows. Only a prefix of the text is laid out, so a long value costs no more than a
/// short one.
QStringList wrappedLines(const QString& text, const QFont& font, qreal width, int maxLines) {
    const QFontMetricsF metrics(font);
    // Every character is at least one pixel wide: this prefix fills more than maxLines lines.
    const qsizetype prefix = std::min<qsizetype>(text.size(), static_cast<qsizetype>(maxLines * (width + 2)) + 8);
    QTextLayout layout(text.left(prefix), font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    QStringList lines;
    int end = 0;
    while (lines.size() < maxLines) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        lines << text.mid(line.textStart(), line.textLength());
        end = line.textStart() + line.textLength();
    }
    layout.endLayout();
    if (end < text.size() && !lines.isEmpty()) {
        // More follows: the last line ends in an ellipsis, cut so that it fits.
        const QString last = lines.last() + text.mid(end, 64);
        QString elided = metrics.elidedText(last, Qt::ElideRight, width);
        if (!elided.endsWith(kEllipsis)) {
            elided = metrics.elidedText(lines.last() + kEllipsis + kEllipsis, Qt::ElideRight, width);
        }
        lines.last() = elided;
    }
    return lines;
}

QString shortDate(const core::PropertyDate& d) {
    const QDate date(d.year, d.month, d.day);
    if (d.hour < 0) {
        return QLocale::system().toString(date, QLocale::ShortFormat);
    }
    return QLocale::system().toString(QDateTime(date, QTime(d.hour, d.minute, std::max(d.second, 0))),
                                      QLocale::ShortFormat);
}

// ---- accessibility --------------------------------------------------------------------------

/// The title or one row of the block: an accessible element without a widget of its own.
class ElementAccessible final : public QAccessibleInterface {
public:
    ElementAccessible(PropertiesView* view, int element) : view_(view), element_(element) {}

    [[nodiscard]] bool isValid() const override { return view_ != nullptr && element_ < view_->accessibleCount(); }
    [[nodiscard]] QObject* object() const override { return nullptr; }
    [[nodiscard]] QWindow* window() const override {
        return view_ != nullptr ? view_->window()->windowHandle() : nullptr;
    }
    [[nodiscard]] QAccessibleInterface* childAt(int /*x*/, int /*y*/) const override { return nullptr; }
    [[nodiscard]] QAccessibleInterface* parent() const override {
        return view_ != nullptr ? QAccessible::queryAccessibleInterface(view_.data()) : nullptr;
    }
    [[nodiscard]] QAccessibleInterface* child(int /*index*/) const override { return nullptr; }
    [[nodiscard]] int childCount() const override { return 0; }
    [[nodiscard]] int indexOfChild(const QAccessibleInterface* /*child*/) const override { return -1; }
    [[nodiscard]] QString text(QAccessible::Text t) const override {
        if (!isValid()) {
            return {};
        }
        if (t == QAccessible::Name) {
            return view_->accessibleName(element_);
        }
        if (t == QAccessible::Description) {
            return view_->accessibleDescription(element_);
        }
        return {};
    }
    void setText(QAccessible::Text /*t*/, const QString& /*text*/) override {}
    [[nodiscard]] QRect rect() const override {
        if (!isValid()) {
            return {};
        }
        const QRect r = view_->accessibleRect(element_);
        return {view_->mapToGlobal(r.topLeft()), r.size()};
    }
    [[nodiscard]] QAccessible::Role role() const override {
        return element_ == 0 ? QAccessible::StaticText : QAccessible::ListItem;
    }
    [[nodiscard]] QAccessible::State state() const override {
        QAccessible::State s;
        s.readOnly = true;
        s.invisible = view_ == nullptr || !view_->isVisible();
        return s;
    }
    [[nodiscard]] int element() const noexcept { return element_; }

private:
    QPointer<PropertiesView> view_;
    int element_;
};

/// The block: its children are the title and the rows.
class BlockAccessible final : public QAccessibleWidget {
public:
    explicit BlockAccessible(PropertiesView* view) : QAccessibleWidget(view, QAccessible::List) {}

    [[nodiscard]] int childCount() const override { return view()->accessibleCount(); }
    [[nodiscard]] QAccessibleInterface* child(int index) const override {
        return index >= 0 && index < childCount() ? view()->accessibleElement(index) : nullptr;
    }
    [[nodiscard]] int indexOfChild(const QAccessibleInterface* child) const override {
        const auto* element = dynamic_cast<const ElementAccessible*>(child);
        return element != nullptr && child->parent() == this ? element->element() : -1;
    }
    [[nodiscard]] QAccessibleInterface* childAt(int x, int y) const override {
        const QPoint local = view()->mapFromGlobal(QPoint(x, y));
        for (int i = 0; i < childCount(); ++i) {
            if (view()->accessibleRect(i).contains(local)) {
                return view()->accessibleElement(i);
            }
        }
        return nullptr;
    }

private:
    [[nodiscard]] PropertiesView* view() const { return static_cast<PropertiesView*>(widget()); }
};

QAccessibleInterface* accessibleFactory(const QString& /*className*/, QObject* object) {
    if (auto* view = qobject_cast<PropertiesView*>(object)) {
        return new BlockAccessible(view);
    }
    return nullptr;
}

}  // namespace

// ---- the block ------------------------------------------------------------------------------

PropertiesView::PropertiesView(QWidget* parent) : QWidget(parent) {
    static const bool factoryInstalled = [] {
        QAccessible::installFactory(accessibleFactory);
        return true;
    }();
    (void)factoryInstalled;
    setFocusPolicy(Qt::NoFocus);
    setAccessibleName(tr("Properties"));
    updateMetrics();
}

PropertiesView::~PropertiesView() {
    forgetAccessibleElements();
}

QString PropertiesView::typeName(core::PropertyType type) {
    switch (type) {
        case core::PropertyType::Text:
            return tr("text");
        case core::PropertyType::Number:
            return tr("number");
        case core::PropertyType::Checkbox:
            return tr("checkbox");
        case core::PropertyType::Date:
            return tr("date");
        case core::PropertyType::DateTime:
            return tr("date and time");
        case core::PropertyType::List:
            return tr("list");
        case core::PropertyType::Tags:
            return tr("tags");
        case core::PropertyType::Unsupported:
            return tr("unsupported");
    }
    return {};
}

QString PropertiesView::valueText(const core::Property& p) {
    switch (p.type) {
        case core::PropertyType::Checkbox:
            return p.checked ? tr("checked") : tr("not checked");
        case core::PropertyType::Date:
        case core::PropertyType::DateTime:
            return shortDate(p.date);
        case core::PropertyType::List:
        case core::PropertyType::Tags: {
            QStringList items;
            items.reserve(static_cast<qsizetype>(p.items.size()));
            for (const auto& item : p.items) {
                items << toQString(item);
            }
            return items.join(QStringLiteral(", "));
        }
        default:
            return toQString(p.text);
    }
}

void PropertiesView::updateMetrics() {
    const QFontMetricsF metrics(look_.font);
    line_ = metrics.lineSpacing();
    gap_ = metrics.horizontalAdvance(QLatin1Char(' '));
    icon_ = std::round(metrics.ascent());
    const int padding = static_cast<int>(std::round(look_.style.padding));
    title_ = QRect(padding, padding, std::max(1, look_.width - 2 * padding), static_cast<int>(std::ceil(line_)));
}

int PropertiesView::innerWidth() const {
    return std::max(1, look_.width - 2 * static_cast<int>(std::round(look_.style.padding)));
}

void PropertiesView::setLook(const Look& look) {
    look_ = look;
    setFont(look_.font);
    updateMetrics();
    // Rows laid out so far follow at once.
    const std::size_t count = rows_.size();
    rows_.clear();
    int top = title_.bottom() + 1 + static_cast<int>(std::round(look_.style.rowSpacing));
    for (std::size_t i = 0; i < count; ++i) {
        rows_.push_back(layoutRow(properties_[i], top, innerWidth()));
        top = rows_.back().rect.bottom() + 1 + static_cast<int>(std::round(look_.style.rowSpacing));
    }
    if (laidOut_) {
        finishLayout();
    }
    update();
}

void PropertiesView::reset(std::vector<core::Property> properties) {
    forgetAccessibleElements();
    properties_ = std::move(properties);
    rows_.clear();
    rows_.reserve(properties_.size());
    laidOut_ = false;
}

bool PropertiesView::build(const QElapsedTimer& timer, qint64 nanoseconds) {
    // In document order: the first rows are on screen when the block is at the top.
    const int spacing = static_cast<int>(std::round(look_.style.rowSpacing));
    while (rows_.size() < properties_.size()) {
        const int top = rows_.empty() ? title_.bottom() + 1 + spacing : rows_.back().rect.bottom() + 1 + spacing;
        rows_.push_back(layoutRow(properties_[rows_.size()], top, innerWidth()));
        if (rows_.size() % 16 == 0 && timer.nsecsElapsed() > nanoseconds) {
            return false;
        }
    }
    if (!laidOut_) {
        finishLayout();
    }
    return true;
}

void PropertiesView::finishLayout() {
    // The padding below the last row, or below the title alone.
    const int padding = static_cast<int>(std::round(look_.style.padding));
    const int bottom = rows_.empty() ? title_.bottom() + 1 : rows_.back().rect.bottom() + 1;
    setFixedSize(look_.width, std::max(1, bottom + padding));
    laidOut_ = true;
}

PropertiesView::Row PropertiesView::layoutRow(const core::Property& p, int top, int width) const {
    const QFontMetricsF metrics(look_.font);
    Row row;
    row.key = toQString(p.key);
    const QString value = valueText(p);
    row.name = row.key + QStringLiteral(":") + (value.isEmpty() ? QString() : QStringLiteral(" ") + value);
    const qreal keyColumn = std::min<qreal>(look_.style.keyWidth, width / 2.0);
    row.keyWidth = std::max<qreal>(1, keyColumn - icon_ - 2 * gap_);
    row.valueX = keyColumn;
    row.valueWidth = std::max<qreal>(1, width - keyColumn);
    const qreal keyHeight =
        std::max(line_, metrics.boundingRect(QRectF(0, 0, row.keyWidth, 1e9), kTextFlags, row.key).height());
    qreal valueHeight = line_;
    switch (p.type) {
        case core::PropertyType::Checkbox:
            break;
        case core::PropertyType::List:
        case core::PropertyType::Tags: {
            const qreal inset = look_.style.chipPadding + look_.style.chipBorderWidth;
            qreal x = 0;
            qreal y = 0;
            for (const auto& item : p.items) {
                const QString text = toQString(item);
                const qreal w = std::min(row.valueWidth, metrics.horizontalAdvance(text) + 2 * inset);
                if (x > 0 && x + w > row.valueWidth) {
                    x = 0;
                    y += line_ + look_.style.rowSpacing;
                }
                row.chips.push_back({QRectF(x, y, w, line_), text});
                x += w + gap_;
            }
            valueHeight = y + line_;
            break;
        }
        case core::PropertyType::Unsupported:
            row.lines = wrappedLines(value, look_.font, row.valueWidth, kJsonLines);
            valueHeight = std::max<qreal>(1, static_cast<qreal>(row.lines.size())) * line_;
            break;
        default:
            row.text = value;
            valueHeight =
                std::max(line_, metrics.boundingRect(QRectF(0, 0, row.valueWidth, 1e9), kTextFlags, value).height());
            break;
    }
    const int left = static_cast<int>(std::round(look_.style.padding));
    row.rect = QRect(left, top, width, static_cast<int>(std::ceil(std::max(keyHeight, valueHeight))));
    return row;
}

int PropertiesView::rowAt(QPoint position) const {
    // The rows are in order of y; a row owns the spacing below it.
    const auto it = std::upper_bound(rows_.begin(), rows_.end(), position.y(),
                                     [](int y, const Row& row) { return y < row.rect.top(); });
    if (it == rows_.begin()) {
        return -1;
    }
    const auto index = static_cast<int>(std::distance(rows_.begin(), it)) - 1;
    const Row& row = rows_[static_cast<std::size_t>(index)];
    const int spacing = static_cast<int>(std::round(look_.style.rowSpacing));
    return position.y() <= row.rect.bottom() + spacing ? index : -1;
}

// ---- painting -------------------------------------------------------------------------------

void PropertiesView::paintEvent(QPaintEvent* event) {
    const core::PropertiesStyle& s = look_.style;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal border = s.borderWidth;
    painter.setPen(border > 0 ? QPen(toQColor(s.border), border) : QPen(Qt::NoPen));
    painter.setBrush(toQColor(s.background));
    const QRectF r = QRectF(rect()).adjusted(border / 2, border / 2, -border / 2, -border / 2);
    const qreal radius = std::max<qreal>(0, s.radius - border / 2);
    painter.drawRoundedRect(r, radius, radius);
    painter.setFont(look_.font);
    painter.setPen(toQColor(s.titleText));
    painter.drawText(QRectF(title_), Qt::AlignLeft | Qt::AlignVCenter, tr("Properties"));
    // Only the rows in the area to paint.
    const QRect area = event->rect();
    auto it = std::upper_bound(rows_.begin(), rows_.end(), area.top(),
                               [](int y, const Row& row) { return y < row.rect.top(); });
    if (it != rows_.begin()) {
        --it;
    }
    for (; it != rows_.end() && it->rect.top() <= area.bottom(); ++it) {
        paintRow(painter, *it, properties_[static_cast<std::size_t>(std::distance(rows_.begin(), it))]);
    }
}

void PropertiesView::paintRow(QPainter& painter, const Row& row, const core::Property& p) const {
    const core::PropertiesStyle& s = look_.style;
    const QPointF origin = row.rect.topLeft();
    paintIcon(painter, QRectF(origin.x(), origin.y() + (line_ - icon_) / 2, icon_, icon_), p.type);
    painter.setPen(toQColor(s.keyText));
    painter.drawText(QRectF(origin.x() + icon_ + gap_, origin.y(), row.keyWidth, row.rect.height()), kTextFlags,
                     row.key);
    const QRectF value(origin.x() + row.valueX, origin.y(), row.valueWidth, row.rect.height());
    switch (p.type) {
        case core::PropertyType::Checkbox: {
            const qreal size = icon_;
            const QRectF box(value.left(), value.top() + (line_ - size) / 2, size, size);
            const qreal border = std::max<qreal>(1, size / 10);
            painter.setPen(QPen(toQColor(s.checkbox), border));
            painter.setBrush(toQColor(s.checkboxBackground));
            painter.drawRoundedRect(box.adjusted(border / 2, border / 2, -border / 2, -border / 2), size / 5, size / 5);
            if (p.checked) {
                QPainterPath tick;
                tick.moveTo(box.left() + size * 0.25, box.top() + size * 0.52);
                tick.lineTo(box.left() + size * 0.43, box.top() + size * 0.7);
                tick.lineTo(box.left() + size * 0.76, box.top() + size * 0.3);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(toQColor(s.checkbox), border * 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPath(tick);
            }
            break;
        }
        case core::PropertyType::List:
        case core::PropertyType::Tags:
            for (const Chip& chip : row.chips) {
                const QRectF r = chip.rect.translated(value.topLeft());
                const qreal border = s.chipBorderWidth;
                painter.setPen(border > 0 ? QPen(toQColor(s.chipBorder), border) : QPen(Qt::NoPen));
                painter.setBrush(toQColor(s.chipBackground));
                const qreal radius = std::min<qreal>(s.chipRadius, r.height() / 2);
                painter.drawRoundedRect(r.adjusted(border / 2, border / 2, -border / 2, -border / 2), radius, radius);
                painter.setPen(toQColor(s.chipText));
                const qreal inset = s.chipPadding + border;
                painter.drawText(r.adjusted(inset, 0, -inset, 0), Qt::AlignVCenter | Qt::AlignLeft, chip.text);
            }
            break;
        case core::PropertyType::Unsupported: {
            painter.setPen(toQColor(s.unsupportedText));
            qreal y = value.top();
            for (const QString& line : row.lines) {
                painter.drawText(QRectF(value.left(), y, value.width(), line_), Qt::AlignLeft | Qt::AlignVCenter, line);
                y += line_;
            }
            break;
        }
        default:
            painter.setPen(toQColor(s.text));
            painter.drawText(value, kTextFlags, row.text);
            break;
    }
}

void PropertiesView::paintIcon(QPainter& painter, const QRectF& r, core::PropertyType type) const {
    // Drawn, no image assets: Obsidian's icons, simplified.
    const qreal w = std::max<qreal>(1, r.width() / 9);
    painter.save();
    painter.setPen(QPen(toQColor(look_.style.icon), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    const qreal x0 = r.left() + r.width() * 0.15;
    const qreal x1 = r.right() - r.width() * 0.15;
    const qreal y0 = r.top() + r.height() * 0.15;
    const qreal y1 = r.bottom() - r.height() * 0.15;
    const qreal xm = r.center().x();
    const qreal ym = r.center().y();
    switch (type) {
        case core::PropertyType::Text:
            for (const qreal f : {0.25, 0.5, 0.75}) {
                const qreal y = r.top() + r.height() * f;
                painter.drawLine(QPointF(x0, y), QPointF(f == 0.75 ? xm : x1, y));
            }
            break;
        case core::PropertyType::Number: {
            const qreal dx = r.width() * 0.12;
            painter.drawLine(QPointF(xm - dx + dx / 2, y0), QPointF(xm - dx - dx / 2, y1));
            painter.drawLine(QPointF(xm + dx + dx / 2, y0), QPointF(xm + dx - dx / 2, y1));
            painter.drawLine(QPointF(x0, r.top() + r.height() * 0.38), QPointF(x1, r.top() + r.height() * 0.38));
            painter.drawLine(QPointF(x0, r.top() + r.height() * 0.62), QPointF(x1, r.top() + r.height() * 0.62));
            break;
        }
        case core::PropertyType::Checkbox: {
            painter.drawRoundedRect(QRectF(x0, y0, x1 - x0, y1 - y0), w * 1.5, w * 1.5);
            QPainterPath tick;
            tick.moveTo(x0 + (x1 - x0) * 0.25, ym);
            tick.lineTo(xm - (x1 - x0) * 0.05, y1 - (y1 - y0) * 0.25);
            tick.lineTo(x1 - (x1 - x0) * 0.2, y0 + (y1 - y0) * 0.25);
            painter.drawPath(tick);
            break;
        }
        case core::PropertyType::Date: {
            const QRectF box(x0, y0 + (y1 - y0) * 0.1, x1 - x0, (y1 - y0) * 0.9);
            painter.drawRoundedRect(box, w, w);
            painter.drawLine(QPointF(box.left(), box.top() + box.height() * 0.3),
                             QPointF(box.right(), box.top() + box.height() * 0.3));
            painter.drawLine(QPointF(x0 + (x1 - x0) * 0.3, y0), QPointF(x0 + (x1 - x0) * 0.3, box.top() + w));
            painter.drawLine(QPointF(x1 - (x1 - x0) * 0.3, y0), QPointF(x1 - (x1 - x0) * 0.3, box.top() + w));
            break;
        }
        case core::PropertyType::DateTime:
            painter.drawEllipse(QRectF(x0, y0, x1 - x0, y1 - y0));
            painter.drawLine(QPointF(xm, ym), QPointF(xm, y0 + (y1 - y0) * 0.22));
            painter.drawLine(QPointF(xm, ym), QPointF(xm + (x1 - x0) * 0.22, ym));
            break;
        case core::PropertyType::List:
            for (const qreal f : {0.25, 0.5, 0.75}) {
                const qreal y = r.top() + r.height() * f;
                painter.drawPoint(QPointF(x0, y));
                painter.drawLine(QPointF(x0 + (x1 - x0) * 0.3, y), QPointF(x1, y));
            }
            break;
        case core::PropertyType::Tags: {
            QPainterPath tag;
            tag.moveTo(x0, y0);
            tag.lineTo(xm, y0);
            tag.lineTo(x1, ym);
            tag.lineTo(xm, y1);
            tag.lineTo(x0, y1);
            tag.closeSubpath();
            painter.drawPath(tag);
            painter.drawPoint(QPointF(x0 + (x1 - x0) * 0.25, y0 + (y1 - y0) * 0.3));
            break;
        }
        case core::PropertyType::Unsupported: {
            painter.drawEllipse(QRectF(x0, y0, x1 - x0, y1 - y0));
            QPainterPath q;
            q.moveTo(xm - (x1 - x0) * 0.16, ym - (y1 - y0) * 0.12);
            q.cubicTo(xm - (x1 - x0) * 0.16, y0 + (y1 - y0) * 0.1, xm + (x1 - x0) * 0.2, y0 + (y1 - y0) * 0.1,
                      xm + (x1 - x0) * 0.16, ym - (y1 - y0) * 0.1);
            q.cubicTo(xm + (x1 - x0) * 0.12, ym, xm, ym, xm, ym + (y1 - y0) * 0.1);
            painter.drawPath(q);
            painter.drawPoint(QPointF(xm, y1 - (y1 - y0) * 0.2));
            break;
        }
    }
    painter.restore();
}

// ---- mouse, tooltip, accessibility ----------------------------------------------------------

void PropertiesView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pressed_ = true;
        emit pressed(true);
    }
    event->accept();
}

void PropertiesView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !pressed_) {
        event->accept();
        return;
    }
    pressed_ = false;
    const int row = rowAt(event->position().toPoint());
    const bool extend = (event->modifiers() & Qt::ShiftModifier) != 0;
    emit pressed(false);
    emit activated(row, extend);
    event->accept();
}

bool PropertiesView::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        // The full JSON of an unsupported value (review O1).
        auto* help = static_cast<QHelpEvent*>(event);
        const int row = rowAt(help->pos());
        if (row >= 0 && properties_[static_cast<std::size_t>(row)].type == core::PropertyType::Unsupported) {
            QToolTip::showText(help->globalPos(), toQString(properties_[static_cast<std::size_t>(row)].text), this,
                               rows_[static_cast<std::size_t>(row)].rect);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

QString PropertiesView::accessibleName(int element) const {
    if (element == 0) {
        return tr("Properties");
    }
    return element > 0 && element < accessibleCount() ? rows_[static_cast<std::size_t>(element - 1)].name : QString();
}

QString PropertiesView::accessibleDescription(int element) const {
    if (element <= 0 || element >= accessibleCount()) {
        return {};
    }
    return typeName(properties_[static_cast<std::size_t>(element - 1)].type);
}

QRect PropertiesView::accessibleRect(int element) const {
    if (element == 0) {
        return title_;
    }
    return element > 0 && element < accessibleCount() ? rows_[static_cast<std::size_t>(element - 1)].rect : QRect();
}

QAccessibleInterface* PropertiesView::accessibleElement(int element) {
    if (element < 0 || element >= accessibleCount()) {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(element);
    if (accessibleIds_.size() <= index) {
        accessibleIds_.resize(index + 1, 0);
    }
    if (accessibleIds_[index] == 0) {
        accessibleIds_[index] = QAccessible::registerAccessibleInterface(new ElementAccessible(this, element));
    }
    return QAccessible::accessibleInterface(accessibleIds_[index]);
}

void PropertiesView::forgetAccessibleElements() {
    for (const QAccessible::Id id : accessibleIds_) {
        if (id != 0) {
            QAccessible::deleteAccessibleInterface(id);
        }
    }
    accessibleIds_.clear();
}

}  // namespace hitokoto::ui
