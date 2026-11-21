// SPDX-License-Identifier: GPL-3.0-or-later
#include "properties_view.hpp"

#include "qt_bridge.hpp"
#include "text_input.hpp"

#include <QAccessibleWidget>
#include <QContextMenuEvent>
#include <QDate>
#include <QDateTime>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QTextLayout>
#include <QTime>
#include <QToolTip>
#include <QWindow>

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>
#include <utility>

namespace hitokoto::ui {

namespace {

constexpr int kJsonLines = 3;              ///< S024 review O1: an unsupported value shows at most three lines
constexpr std::size_t kToolTipLines = 20;  ///< S024 code review O5: its tooltip at most 20 lines
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

/// The title or one row of the block: an accessible element without a widget of its own. The view
/// deletes its elements before it goes (forgetAccessibleElements), so `view_` is never dangling.
class ElementAccessible final : public QAccessibleInterface {
public:
    ElementAccessible(PropertiesView* view, int element) : view_(view), element_(element) {}

    [[nodiscard]] bool isValid() const override { return element_ < view_->accessibleCount(); }
    [[nodiscard]] QObject* object() const override { return nullptr; }
    [[nodiscard]] QWindow* window() const override { return view_->window()->windowHandle(); }
    [[nodiscard]] QAccessibleInterface* childAt(int /*x*/, int /*y*/) const override { return nullptr; }
    [[nodiscard]] QAccessibleInterface* parent() const override { return QAccessible::queryAccessibleInterface(view_); }
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
        s.invisible = !view_->isVisible();
        return s;
    }
    [[nodiscard]] int element() const noexcept { return element_; }

private:
    PropertiesView* view_;
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
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAccessibleName(tr("Properties"));
    updateMetrics();
}

PropertiesView::~PropertiesView() {
    cancelInlineEditor();
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
        rows_.push_back(beginRow(properties_[i], top, innerWidth()));
        (void)continueRow(rows_.back(), nullptr, 0);
        top = rows_.back().rect.bottom() + 1 + static_cast<int>(std::round(look_.style.rowSpacing));
    }
    if (laidOut_) {
        finishLayout();
    }
    update();
}

void PropertiesView::reset(std::vector<core::Property> properties) {
    cancelInlineEditor();
    forgetAccessibleElements();
    properties_ = std::move(properties);
    rows_.clear();
    rows_.reserve(properties_.size());
    laidOut_ = false;
}

bool PropertiesView::build(const QElapsedTimer& timer, qint64 nanoseconds) {
    // In document order: the first rows are on screen when the block is at the top.
    // A long value is laid out across slices (code review O2): the last row may be unfinished.
    const int spacing = static_cast<int>(std::round(look_.style.rowSpacing));
    if (!rows_.empty() && !rows_.back().done && !continueRow(rows_.back(), &timer, nanoseconds)) {
        return false;
    }
    while (rows_.size() < properties_.size()) {
        const int top = rows_.empty() ? title_.bottom() + 1 + spacing : rows_.back().rect.bottom() + 1 + spacing;
        rows_.push_back(beginRow(properties_[rows_.size()], top, innerWidth()));
        if (!continueRow(rows_.back(), &timer, nanoseconds) ||
            (rows_.size() % 16 == 0 && timer.nsecsElapsed() > nanoseconds)) {
            return false;
        }
    }
    if (!laidOut_) {
        finishLayout();
    }
    return true;
}

void PropertiesView::finishLayout() {
    const int padding = static_cast<int>(std::round(look_.style.padding));
    const int spacing = static_cast<int>(std::round(look_.style.rowSpacing));
    const int top = (rows_.empty() ? title_.bottom() + 1 : rows_.back().rect.bottom() + 1) + spacing;
    const QFontMetricsF metrics(look_.font);
    const int buttonWidth = static_cast<int>(std::ceil(metrics.horizontalAdvance(tr("+ Add property")))) + 4;
    const int buttonHeight = static_cast<int>(std::ceil(line_));
    addButton_ = QRect(static_cast<int>(std::round(look_.style.padding)), top, buttonWidth, buttonHeight);
    setFixedSize(look_.width, std::max(1, addButton_.bottom() + 1 + padding));
    laidOut_ = true;
}

PropertiesView::Row PropertiesView::beginRow(const core::Property& p, int top, int width) const {
    const QFontMetricsF metrics(look_.font);
    Row row;
    row.key = toQString(p.key);
    const QString value = valueText(p);
    row.name = row.key + QStringLiteral(":") + (value.isEmpty() ? QString() : QStringLiteral(" ") + value);
    const qreal keyColumn = std::min<qreal>(look_.style.keyWidth, width / 2.0);
    row.keyWidth = std::max<qreal>(1, keyColumn - icon_ - 2 * gap_);
    row.valueX = keyColumn;
    constexpr int kDeleteWidth = 24;
    row.valueWidth = std::max<qreal>(1, width - keyColumn - kDeleteWidth);
    row.deleteRect = QRect(look_.width - static_cast<int>(std::round(look_.style.padding)) - kDeleteWidth, top,
                           kDeleteWidth, static_cast<int>(std::ceil(line_)));
    startText(row.keyText, row.key, row.keyWidth);
    row.valueHeight = line_;
    switch (p.type) {
        case core::PropertyType::Checkbox:
            break;
        case core::PropertyType::List:
        case core::PropertyType::Tags: {
            const qreal inset = look_.style.chipPadding + look_.style.chipBorderWidth;
            const qreal removeWidth = line_ * 0.8;
            qreal x = 0;
            qreal y = 0;
            for (const auto& item : p.items) {
                const QString text = toQString(item);
                const qreal w = std::min(row.valueWidth, metrics.horizontalAdvance(text) + 2 * inset + removeWidth);
                if (x > 0 && x + w > row.valueWidth) {
                    x = 0;
                    y += line_ + look_.style.rowSpacing;
                }
                row.chips.push_back({QRectF(x, y, w, line_), QRectF(x + w - removeWidth, y, removeWidth, line_), text});
                x += w + gap_;
            }
            const qreal addWidth = line_ + 2 * inset;
            if (x > 0 && x + addWidth > row.valueWidth) {
                x = 0;
                y += line_ + look_.style.rowSpacing;
            }
            row.addChipRect = QRectF(x, y, addWidth, line_);
            row.valueHeight = y + line_;
            break;
        }
        case core::PropertyType::Unsupported:
            row.lines = wrappedLines(value, look_.font, row.valueWidth, kJsonLines);
            row.valueHeight = std::max<qreal>(1, static_cast<qreal>(row.lines.size())) * line_;
            break;
        default: {
            // A block scalar's final line break shows no empty line.
            QString shown = value;
            while (shown.endsWith(QLatin1Char('\n'))) {
                shown.chop(1);
            }
            startText(row.valueText, shown, row.valueWidth);
            break;
        }
    }
    const int left = static_cast<int>(std::round(look_.style.padding));
    row.rect = QRect(left, top, width, 0);
    return row;
}

bool PropertiesView::continueRow(Row& row, const QElapsedTimer* timer, qint64 nanoseconds) const {
    if (!continueText(row.keyText, timer, nanoseconds)) {
        return false;
    }
    if (row.valueText.layout != nullptr || !row.valueText.line.isEmpty()) {
        if (!continueText(row.valueText, timer, nanoseconds)) {
            return false;
        }
        row.valueHeight = row.valueText.height;
    }
    row.rect.setHeight(static_cast<int>(std::ceil(std::max(row.keyText.height, row.valueHeight))));
    row.deleteRect.moveTop(row.rect.top() + (row.rect.height() - row.deleteRect.height()) / 2);
    row.done = true;
    return true;
}

void PropertiesView::startText(Text& text, const QString& string, qreal width) const {
    text.width = width;
    text.layout.reset();
    text.line.clear();
    // Most keys and values fit on one line: they need no layout of their own, which keeps building
    // and replacing 5,000 rows as cheap as measuring them.
    constexpr qsizetype kShort = 256;
    if (string.size() <= kShort && !string.contains(QLatin1Char('\n')) &&
        QFontMetricsF(look_.font).horizontalAdvance(string) <= width) {
        text.line = string;
        text.height = line_;
        text.done = true;
        return;
    }
    QString shown = string;
    shown.replace(QLatin1Char('\n'), QChar::LineSeparator);  // QTextLayout breaks lines only there
    text.layout = std::make_unique<QTextLayout>(shown, look_.font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text.layout->setTextOption(option);
    text.layout->setCacheEnabled(true);
    text.height = 0;
    text.done = false;
    text.layout->beginLayout();
}

bool PropertiesView::continueText(Text& text, const QElapsedTimer* timer, qint64 nanoseconds) const {
    if (text.done) {
        return true;
    }
    for (int lines = 1;; ++lines) {
        QTextLine line = text.layout->createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLeadingIncluded(true);
        line.setLineWidth(text.width);
        line.setPosition(QPointF(0, text.height));
        text.height += line.height();
        // At least 32 lines per call, so that every slice gets on.
        if (timer != nullptr && lines % 32 == 0 && timer->nsecsElapsed() > nanoseconds) {
            return false;
        }
    }
    text.layout->endLayout();
    text.height = std::max(text.height, line_);
    text.done = true;
    return true;
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

QRect PropertiesView::rowDeleteRect(int row) const noexcept {
    return row >= 0 && static_cast<std::size_t>(row) < rows_.size() ? rows_[static_cast<std::size_t>(row)].deleteRect
                                                                    : QRect();
}

QRect PropertiesView::chipRect(int row, int item) const noexcept {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size() || item < 0 ||
        static_cast<std::size_t>(item) >= rows_[static_cast<std::size_t>(row)].chips.size()) {
        return {};
    }
    const Row& propertyRow = rows_[static_cast<std::size_t>(row)];
    return propertyRow.chips[static_cast<std::size_t>(item)]
        .rect.translated(QPointF(propertyRow.rect.left() + propertyRow.valueX, propertyRow.rect.top()))
        .toRect();
}

QRect PropertiesView::chipRemoveRect(int row, int item) const noexcept {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size() || item < 0 ||
        static_cast<std::size_t>(item) >= rows_[static_cast<std::size_t>(row)].chips.size()) {
        return {};
    }
    const Row& propertyRow = rows_[static_cast<std::size_t>(row)];
    return propertyRow.chips[static_cast<std::size_t>(item)]
        .removeRect.translated(QPointF(propertyRow.rect.left() + propertyRow.valueX, propertyRow.rect.top()))
        .toRect();
}

QRect PropertiesView::chipAddRect(int row) const noexcept {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) {
        return {};
    }
    const Row& propertyRow = rows_[static_cast<std::size_t>(row)];
    return propertyRow.addChipRect
        .translated(QPointF(propertyRow.rect.left() + propertyRow.valueX, propertyRow.rect.top()))
        .toRect();
}

void PropertiesView::openInlineEditor(const QRect& rect, const QString& value) {
    cancelInlineEditor();
    inlineEditor_ = new QLineEdit(this);
    inlineEditor_->setObjectName(QStringLiteral("propertiesInlineEditor"));
    inlineEditor_->setFont(look_.font);
    const QColor text = toQColor(look_.style.editorText);
    const QColor background = toQColor(look_.style.editorBackground);
    const QColor border = toQColor(look_.style.editorBorder);
    const QColor focusBorder = toQColor(look_.style.editorFocusBorder);
    inlineEditor_->setStyleSheet(
        QStringLiteral("QLineEdit { color: %1; background: %2; border: %3px solid %4; "
                       "border-radius: %5px; padding: 0 %6px; }"
                       "QLineEdit:focus { border-color: %7; }")
            .arg(text.name(QColor::HexArgb), background.name(QColor::HexArgb),
                 QString::number(look_.style.editorBorderWidth, 'f', 1), border.name(QColor::HexArgb),
                 QString::number(look_.style.editorRadius, 'f', 1), QString::number(look_.style.editorPadding, 'f', 1),
                 focusBorder.name(QColor::HexArgb)));
    inlineEditor_->setGeometry(rect.adjusted(0, 0, -1, -1));
    inlineEditor_->setText(value);
    inlineEditor_->installEventFilter(this);
    connect(inlineEditor_, &QLineEdit::editingFinished, this, &PropertiesView::submitInlineEditor);
    inlineEditor_->show();
    inlineEditor_->setFocus(Qt::MouseFocusReason);
    inlineEditor_->selectAll();
    submitted_ = false;
}

void PropertiesView::editKeyOrValue(int row, bool key) {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size()) {
        return;
    }
    const Row& propertyRow = rows_[static_cast<std::size_t>(row)];
    const QRect rect =
        key ? QRect(static_cast<int>(propertyRow.rect.left() + icon_ + gap_), propertyRow.rect.top(),
                    static_cast<int>(propertyRow.valueX - icon_ - 2 * gap_), propertyRow.rect.height())
            : QRect(static_cast<int>(propertyRow.rect.left() + propertyRow.valueX), propertyRow.rect.top(),
                    static_cast<int>(propertyRow.valueWidth), propertyRow.rect.height());
    const core::Property& property = properties_[static_cast<std::size_t>(row)];
    openInlineEditor(rect, key ? propertyRow.key : toQString(property.text));
    editingRow_ = row;
    editingKey_ = key;
    editingItem_ = -1;
    addingProperty_ = false;
}

void PropertiesView::editItem(int row, int item) {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size() || item < 0 ||
        static_cast<std::size_t>(item) > rows_[static_cast<std::size_t>(row)].chips.size()) {
        return;
    }
    const Row& propertyRow = rows_[static_cast<std::size_t>(row)];
    const QRect rect =
        item < static_cast<int>(propertyRow.chips.size())
            ? propertyRow.chips[static_cast<std::size_t>(item)]
                  .rect.translated(QPointF(propertyRow.rect.left() + propertyRow.valueX, propertyRow.rect.top()))
                  .toRect()
            : propertyRow.addChipRect
                  .translated(QPointF(propertyRow.rect.left() + propertyRow.valueX, propertyRow.rect.top()))
                  .toRect();
    const core::Property& property = properties_[static_cast<std::size_t>(row)];
    const QString text = item < static_cast<int>(property.items.size())
                             ? toQString(property.items[static_cast<std::size_t>(item)])
                             : QString();
    openInlineEditor(rect, text);
    editingRow_ = row;
    editingKey_ = false;
    editingItem_ = item;
    addingProperty_ = false;
}

void PropertiesView::editNewProperty() {
    openInlineEditor(addButton_, {});
    editingRow_ = -1;
    editingKey_ = false;
    editingItem_ = -1;
    addingProperty_ = true;
}

void PropertiesView::submitInlineEditor() {
    if (inlineEditor_ == nullptr || submitted_) {
        return;
    }
    submitted_ = true;
    // The field's text enters the document as the editor's does (S004).
    const QString value = insertableText(inlineEditor_->text());
    if (addingProperty_) {
        emit newPropertyCommitted(value);
    } else if (editingItem_ >= 0) {
        emit itemEditCommitted(editingRow_, editingItem_, value);
    } else {
        emit propertyEditCommitted(editingRow_, editingKey_, value);
    }
}

void PropertiesView::finishInlineEdit() {
    if (inlineEditor_ != nullptr) {
        inlineEditor_->removeEventFilter(this);
        inlineEditor_->deleteLater();
        inlineEditor_ = nullptr;
    }
    submitted_ = false;
    addingProperty_ = false;
    editingRow_ = -1;
    editingItem_ = -1;
    if (isVisible()) {
        setFocus(Qt::OtherFocusReason);
    }
    update();
}

void PropertiesView::rejectInlineEdit(const QString& message) {
    if (inlineEditor_ == nullptr) {
        return;
    }
    submitted_ = false;
    inlineEditor_->setToolTip(message);
    inlineEditor_->setStyleSheet(inlineEditor_->styleSheet() +
                                 QStringLiteral(" QLineEdit:focus { border-color: %1; }")
                                     .arg(toQColor(look_.style.unsupportedText).name(QColor::HexArgb)));
    inlineEditor_->setFocus(Qt::OtherFocusReason);
    inlineEditor_->selectAll();
}

void PropertiesView::cancelInlineEditor() {
    finishInlineEdit();
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
    paintedTextLines_ = 0;
    for (; it != rows_.end() && it->rect.top() <= area.bottom(); ++it) {
        const auto row = static_cast<int>(std::distance(rows_.begin(), it));
        const auto [lo, hi] = std::equal_range(findMarks_.begin(), findMarks_.end(), FindMark{row, INT_MIN},
                                               [](const FindMark& a, const FindMark& b) { return a.row < b.row; });
        for (auto mark = lo; mark != hi; ++mark) {
            if (mark->part < 0) {
                painter.fillRect(markRect(*mark), findBackground_);  // S028 "Highlight all"; chips below
            }
        }
        paintedTextLines_ += paintRow(painter, *it, properties_[static_cast<std::size_t>(row)], area, row);
    }
    if (hoveredRow_ >= 0 && static_cast<std::size_t>(hoveredRow_) < rows_.size()) {
        painter.setPen(toQColor(look_.style.icon));
        painter.drawText(rows_[static_cast<std::size_t>(hoveredRow_)].deleteRect, Qt::AlignCenter, QStringLiteral("×"));
    }
    if (hoveredAdd_) {
        painter.setBrush(toQColor(look_.style.chipBackground));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(addButton_.adjusted(-2, 0, 2, 0), look_.style.chipRadius, look_.style.chipRadius);
    }
    painter.setPen(toQColor(look_.style.titleText));
    painter.drawText(addButton_, Qt::AlignLeft | Qt::AlignVCenter, tr("+ Add property"));
}

int PropertiesView::paintText(QPainter& painter, const Text& text, QPointF origin, const QRect& area) {
    if (!text.done) {
        return 0;
    }
    if (text.layout == nullptr) {
        if (text.line.isEmpty() || origin.y() + text.height < area.top() || origin.y() > area.bottom()) {
            return 0;
        }
        painter.drawText(QRectF(origin.x(), origin.y(), text.width, text.height), Qt::AlignLeft | Qt::AlignTop,
                         text.line);
        return 1;
    }
    // The lines are in order of y: the first one that reaches into the area, then on to its end.
    const QTextLayout& layout = *text.layout;
    int first = 0;
    for (int count = layout.lineCount(); count > 0;) {
        const int half = count / 2;
        const QTextLine line = layout.lineAt(first + half);
        if (origin.y() + line.y() + line.height() < area.top()) {
            first += half + 1;
            count -= half + 1;
        } else {
            count = half;
        }
    }
    int painted = 0;
    for (int i = first; i < layout.lineCount(); ++i) {
        const QTextLine line = layout.lineAt(i);
        if (origin.y() + line.y() > area.bottom()) {
            break;
        }
        line.draw(&painter, origin);
        ++painted;
    }
    return painted;
}

int PropertiesView::paintRow(QPainter& painter, const Row& row, const core::Property& p, const QRect& area,
                             int rowIndex) const {
    const core::PropertiesStyle& s = look_.style;
    const QPointF origin = row.rect.topLeft();
    paintIcon(painter, QRectF(origin.x(), origin.y() + (line_ - icon_) / 2, icon_, icon_), p.type);
    painter.setPen(toQColor(s.keyText));
    int painted = paintText(painter, row.keyText, QPointF(origin.x() + icon_ + gap_, origin.y()), area);
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
            for (std::size_t c = 0; c < row.chips.size(); ++c) {
                const Chip& chip = row.chips[c];
                const QRectF r = chip.rect.translated(value.topLeft());
                const bool marked =
                    std::binary_search(findMarks_.begin(), findMarks_.end(), FindMark{rowIndex, static_cast<int>(c)},
                                       [](const FindMark& a, const FindMark& b) {
                                           return std::pair(a.row, a.part) < std::pair(b.row, b.part);
                                       });
                if (r.bottom() < area.top()) {
                    continue;  // only the chips in the area
                }
                if (r.top() > area.bottom()) {
                    break;
                }
                const qreal border = s.chipBorderWidth;
                painter.setPen(border > 0 ? QPen(toQColor(s.chipBorder), border) : QPen(Qt::NoPen));
                painter.setBrush(marked ? findBackground_ : toQColor(s.chipBackground));
                const qreal radius = std::min<qreal>(s.chipRadius, r.height() / 2);
                painter.drawRoundedRect(r.adjusted(border / 2, border / 2, -border / 2, -border / 2), radius, radius);
                painter.setPen(toQColor(s.chipText));
                const qreal inset = s.chipPadding + border;
                painter.drawText(r.adjusted(inset, 0, -inset - chip.removeRect.width(), 0),
                                 Qt::AlignVCenter | Qt::AlignLeft, chip.text);
                painter.drawText(chip.removeRect.translated(value.topLeft()), Qt::AlignCenter, QStringLiteral("×"));
            }
            {
                const QRectF r = row.addChipRect.translated(value.topLeft());
                const qreal border = s.chipBorderWidth;
                painter.setPen(border > 0 ? QPen(toQColor(s.chipBorder), border) : QPen(Qt::NoPen));
                painter.setBrush(toQColor(s.chipBackground));
                painter.drawRoundedRect(r.adjusted(border / 2, border / 2, -border / 2, -border / 2),
                                        std::min<qreal>(s.chipRadius, r.height() / 2),
                                        std::min<qreal>(s.chipRadius, r.height() / 2));
                painter.setPen(toQColor(s.chipText));
                painter.drawText(r, Qt::AlignCenter, QStringLiteral("+"));
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
            painted += paintText(painter, row.valueText, value.topLeft(), area);
            break;
    }
    return painted;
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
    if (inlineEditor_ != nullptr) {
        submitInlineEditor();
        if (inlineEditor_ != nullptr) {
            event->accept();
            return;
        }
    }
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
    const QPoint position = event->position().toPoint();
    const int row = rowAt(event->position().toPoint());
    const bool extend = (event->modifiers() & Qt::ShiftModifier) != 0;
    emit pressed(false);
    if (addButton_.contains(position)) {
        emit addRequested();
    } else if (row >= 0) {
        const Row& activatedRow = rows_[static_cast<std::size_t>(row)];
        if (activatedRow.deleteRect.contains(position)) {
            emit deleteRequested(row);
        } else if (!extend && (properties_[static_cast<std::size_t>(row)].type == core::PropertyType::List ||
                               properties_[static_cast<std::size_t>(row)].type == core::PropertyType::Tags)) {
            const QPointF valueOrigin(activatedRow.rect.left() + activatedRow.valueX, activatedRow.rect.top());
            bool handled = false;
            for (std::size_t i = 0; i < activatedRow.chips.size(); ++i) {
                const Chip& chip = activatedRow.chips[i];
                if (chip.rect.translated(valueOrigin).contains(position)) {
                    if (chip.removeRect.translated(valueOrigin).contains(position)) {
                        emit itemDeleteRequested(row, static_cast<int>(i));
                    } else {
                        emit itemEditRequested(row, static_cast<int>(i));
                    }
                    handled = true;
                    break;
                }
            }
            if (!handled && activatedRow.addChipRect.translated(valueOrigin).contains(position)) {
                emit itemEditRequested(row, static_cast<int>(activatedRow.chips.size()));
                handled = true;
            }
            if (!handled && position.x() >= activatedRow.rect.left() + activatedRow.valueX &&
                position.x() < activatedRow.deleteRect.left()) {
                emit itemEditRequested(row, static_cast<int>(activatedRow.chips.size()));
                handled = true;
            }
            if (!handled) {
                emit activated(row, false, position.x() < activatedRow.rect.left() + activatedRow.valueX);
            }
        } else {
            emit activated(row, extend, position.x() < activatedRow.rect.left() + activatedRow.valueX);
        }
    }
    event->accept();
}

void PropertiesView::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Z) {
        emit undoRequested();
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Y) {
        emit redoRequested();
    }
    // Every other key ends here: unaccepted, Qt would hand it to the editor, which would change
    // the body at its text caret while the writer looks at the block.
    event->accept();
}

void PropertiesView::mouseMoveEvent(QMouseEvent* event) {
    const QPoint position = event->position().toPoint();
    const int row = rowAt(position);
    const bool add = addButton_.contains(position);
    if (row != hoveredRow_ || add != hoveredAdd_) {
        hoveredRow_ = row;
        hoveredAdd_ = add;
        update();
    }
    bool action = add;
    if (row >= 0) {
        const Row& propertyRow = rows_[static_cast<std::size_t>(row)];
        action = action || propertyRow.deleteRect.contains(position);
        const QPointF valueOrigin(propertyRow.rect.left() + propertyRow.valueX, propertyRow.rect.top());
        action = action || propertyRow.addChipRect.translated(valueOrigin).contains(position);
        for (const Chip& chip : propertyRow.chips) {
            action = action || chip.removeRect.translated(valueOrigin).contains(position);
        }
    }
    setCursor(action ? Qt::PointingHandCursor : row >= 0 ? Qt::IBeamCursor : Qt::ArrowCursor);
    event->accept();
}

void PropertiesView::leaveEvent(QEvent* event) {
    if (hoveredRow_ >= 0 || hoveredAdd_) {
        hoveredRow_ = -1;
        hoveredAdd_ = false;
        update();
    }
    unsetCursor();
    QWidget::leaveEvent(event);
}

bool PropertiesView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == inlineEditor_) {
        if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape) {
                cancelInlineEditor();
                return true;
            }
            // Return commits here and goes no further: QLineEdit leaves it unaccepted, and the
            // editor would break the line at its text caret.
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                submitInlineEditor();
                return true;
            }
            if (key->modifiers() == Qt::ControlModifier && key->key() == Qt::Key_Z) {
                emit undoRequested();
                return true;
            }
            if (key->modifiers() == Qt::ControlModifier && key->key() == Qt::Key_Y) {
                emit redoRequested();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut && !submitted_) {
            submitInlineEditor();
            return inlineEditor_ != nullptr;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QString PropertiesView::toolTipText(int row) const {
    if (row < 0 || static_cast<std::size_t>(row) >= rows_.size() ||
        properties_[static_cast<std::size_t>(row)].type != core::PropertyType::Unsupported) {
        return {};
    }
    return toQString(core::prettyJson(properties_[static_cast<std::size_t>(row)].text, kToolTipLines));
}

bool PropertiesView::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        // The JSON of an unsupported value (review O1).
        auto* help = static_cast<QHelpEvent*>(event);
        const int row = rowAt(help->pos());
        if (const QString tip = toolTipText(row); !tip.isEmpty()) {
            // As its characters, also where the value holds tags (S022: link titles alike).
            QToolTip::showText(help->globalPos(), Qt::convertFromPlainText(tip, Qt::WhiteSpacePre), this,
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
    if (accessibleElements_.size() <= index) {
        accessibleElements_.resize(index + 1, nullptr);
    }
    QAccessibleInterface*& slot = accessibleElements_[index];
    if (slot == nullptr) {
        // Registered, Qt's cache owns it; forgetAccessibleElements() deletes it.
        slot = new ElementAccessible(this, element);
        QAccessible::registerAccessibleInterface(slot);
    }
    return slot;
}

void PropertiesView::forgetAccessibleElements() {
    for (QAccessibleInterface* element : accessibleElements_) {
        if (element != nullptr) {
            QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(element));
        }
    }
    accessibleElements_.clear();
}

QRectF PropertiesView::markRect(const FindMark& mark) const {
    if (mark.row < 0 || mark.row >= static_cast<int>(rows_.size())) {
        return {};
    }
    const Row& row = rows_[static_cast<std::size_t>(mark.row)];
    const QPointF origin = row.rect.topLeft();
    const QFontMetricsF metrics(look_.font);
    const auto extent = [&](const Text& text) {
        const qreal width =
            text.layout == nullptr ? std::min(text.width, metrics.horizontalAdvance(text.line)) : text.width;
        return QSizeF(std::max<qreal>(1, width), std::max(line_, text.height));
    };
    if (mark.part == FindMark::kKey) {
        return {QPointF(origin.x() + icon_ + gap_, origin.y()), extent(row.keyText)};
    }
    const QPointF value(origin.x() + row.valueX, origin.y());
    if (mark.part >= 0) {
        if (mark.part >= static_cast<int>(row.chips.size())) {
            return {};
        }
        return row.chips[static_cast<std::size_t>(mark.part)].rect.translated(value);
    }
    const bool noText = row.valueText.line.isEmpty() && row.valueText.layout == nullptr;
    if (!row.chips.empty() || !row.lines.isEmpty() || noText) {
        return {value, QSizeF(row.valueWidth, row.valueHeight)};  // a checkbox, JSON, or an empty value
    }
    return {value, extent(row.valueText)};
}

std::vector<QRect> PropertiesView::findMarkRects() const {
    std::vector<QRect> out;
    for (const FindMark& mark : findMarks_) {
        const QRectF r = markRect(mark);
        if (!r.isEmpty()) {
            out.push_back(r.toAlignedRect());
        }
    }
    return out;
}

void PropertiesView::setFindMarks(std::vector<FindMark> marks, const QColor& background) {
    // Sorted by row and part, each once: the paint event looks them up per row.
    const auto less = [](const FindMark& a, const FindMark& b) {
        return std::pair(a.row, a.part) < std::pair(b.row, b.part);
    };
    std::sort(marks.begin(), marks.end(), less);
    marks.erase(std::unique(marks.begin(), marks.end()), marks.end());
    if (marks == findMarks_ && background == findBackground_) {
        return;
    }
    findMarks_ = std::move(marks);
    findBackground_ = background;
    update();
}

}  // namespace hitokoto::ui
