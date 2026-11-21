// SPDX-License-Identifier: GPL-3.0-or-later
#include "properties_embedder.hpp"

#include "block_styler.hpp"
#include "editor_widget.hpp"
#include "properties_view.hpp"
#include "qt_bridge.hpp"

#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace hitokoto::ui {

namespace {

constexpr qint64 kKeyNanoseconds = 4'000'000;    ///< reading and building on a key (S024)
constexpr qint64 kSliceNanoseconds = 6'000'000;  ///< an idle slice, as the styler's
/// Reading and building when a file is opened: one frame, so that the first screen of a note
/// with ordinary front matter shows the properties block.
constexpr qint64 kOpeningNanoseconds = 16'000'000;

/// The badge after the first line of invalid front matter: its text is its accessible name.
class Badge final : public QWidget {
public:
    explicit Badge(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }
    void set(const QString& text, const QFont& font, const QColor& color) {
        text_ = text;
        color_ = color;
        setFont(font);
        setAccessibleName(text);
        const QFontMetricsF metrics(font);
        resize(static_cast<int>(std::ceil(metrics.horizontalAdvance(text))) + 1,
               static_cast<int>(std::ceil(metrics.lineSpacing())));
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setPen(color_);
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, text_);
    }

private:
    QString text_;
    QColor color_;
};

}  // namespace

PropertiesEmbedder::PropertiesEmbedder(EditorWidget* editor, BlockStyler* styler, BlockLayout* layout)
    : QObject(editor), editor_(editor), styler_(styler), layout_(layout) {
    layout_->addClaimant(this);
    work_.setInterval(0);
    connect(&work_, &QTimer::timeout, this, &PropertiesEmbedder::slice);
    styler_->addPendingWork(this, [this] { return busy(); });
}

PropertiesEmbedder::~PropertiesEmbedder() {
    styler_->removePendingWork(this);
    layout_->removeClaimant(this);
}

bool PropertiesEmbedder::claims(const QTextBlock& block) const {
    return shown_ && block.isValid() && block.blockNumber() <= hiddenTo_;
}

void PropertiesEmbedder::setEnabled(bool enabled) {
    if (enabled == enabled_) {
        return;
    }
    enabled_ = enabled;
    dirty_ = true;
    schedule();
}

void PropertiesEmbedder::setLook(const core::PropertiesStyle& style, const QFont& font, const QFont& badgeFont) {
    style_ = style;
    font_ = font;
    badgeFont_ = badgeFont;
    hasLook_ = true;
    if (view_ != nullptr) {
        view_->setLook({style_, font_, width_});
        apply();
    }
    updateBadge();
    schedule();
}

void PropertiesEmbedder::setAvailableWidth(int width) {
    if (width == width_) {
        return;
    }
    width_ = width;
    if (view_ != nullptr) {
        view_->setLook({style_, font_, width_});
        apply();
    }
}

QWidget* PropertiesEmbedder::view() const {
    return shown_ ? view_ : nullptr;
}

QString PropertiesEmbedder::badge() const {
    if (!enabled_ || close_ < 0 || !error_ || forgotten_) {
        return {};
    }
    if (editor_->textCursor().blockNumber() <= close_) {
        return {};  // the caret is in the block: its YAML is being edited
    }
    // Line n of the document, counted from 1: the YAML starts on the second line.
    return tr("%1 YAML (line %2): %3")
        .arg(core::isUnsupported(error_->reason) ? tr("Unsupported") : tr("Invalid"))
        .arg(error_->line + 2)
        .arg(QString::fromLatin1(core::reasonText(error_->reason)));
}

bool PropertiesEmbedder::caretInside() const {
    if (close_ < 0) {
        return false;
    }
    const QTextCursor c = editor_->textCursor();
    const int anchor = editor_->document()->findBlock(c.anchor()).blockNumber();
    return c.blockNumber() <= close_ && anchor <= close_;
}

int PropertiesEmbedder::confirmedClose() const {
    return enabled_ && hasLook_ && !styler_->frontMatterPending() ? styler_->frontMatterClose() : -1;
}

void PropertiesEmbedder::placeCaretAfterOpening(bool loaded) {
    afterOpening_ = false;
    if (confirmedClose() >= 0 && placeCaretBelow()) {
        return;
    }
    editor_->setTextCursor(QTextCursor(editor_->document()));
    afterOpening_ = loaded && enabled_ && styler_->frontMatterPending();  // work() places it once confirmed
}

bool PropertiesEmbedder::placeCaretBelow() {
    const int close = confirmedClose();
    if (close < 0) {
        return false;
    }
    if (close != close_ || dirty_ || forgotten_) {
        if (shown_ && !forgotten_) {
            reveal();
        }
        forgotten_ = false;
        shown_ = false;
        hiddenTo_ = -1;
        if (view_ != nullptr) {
            view_->hide();
        }
        close_ = close;
        read();
    }
    const QTextBlock below = editor_->document()->findBlockByNumber(close_ + 1);
    // A table there would take the caret and the keyboard focus into its first cell (code review
    // O4): the caret stays at the start.
    if (!error_ && below.isValid() && !styler_->renderFor(below).table) {
        editor_->setTextCursor(QTextCursor(below));  // reads and builds for at most 4 ms (caretMoved)
        if (busy() && !caretInside() && work(kOpeningNanoseconds)) {
            work_.stop();  // built within the frame of opening
        }
        return true;
    }
    return false;
}

void PropertiesEmbedder::schedule() {
    if (!work_.isActive()) {
        work_.start();
    }
}

void PropertiesEmbedder::slice() {
    if (work(kSliceNanoseconds)) {
        work_.stop();
    }
}

bool PropertiesEmbedder::work(qint64 nanoseconds) {
    QElapsedTimer timer;
    timer.start();
    if (forgotten_) {
        // The blocks it hid are gone with the old document; nothing to show again.
        forgotten_ = false;
        shown_ = false;
        hiddenTo_ = -1;
        close_ = -1;
        dirty_ = true;
        if (view_ != nullptr) {
            view_->hide();
        }
    }
    const int close = confirmedClose();
    if (afterOpening_ && !styler_->frontMatterPending()) {
        afterOpening_ = false;
        const QTextCursor cursor = editor_->textCursor();
        if (cursor.position() == 0 && !cursor.hasSelection()) {
            (void)placeCaretBelow();  // the caret has not moved since opening
            return work(nanoseconds);
        }
    }
    const bool keepGraphical = shown_;
    if (close != close_) {
        if (shown_) {
            reveal();
        }
        close_ = close;
        dirty_ = true;
    }
    if (close_ < 0) {
        error_.reset();
        if (view_ != nullptr) {
            view_->reset({});
        }
        updateBadge();
        return true;
    }
    if (dirty_) {
        if (caretInside() && !shown_) {
            updateBadge();
            return true;  // read when the caret leaves; typing in the YAML reads nothing
        }
        read();
        if (keepGraphical && !error_) {
            conceal();
        }
    }
    if (!error_ && view_ != nullptr && !view_->built() && !view_->build(timer, nanoseconds)) {
        return false;  // on initial load, the source stays visible until the block is built
    }
    apply();
    updateBadge();
    return true;
}

void PropertiesEmbedder::read() {
    dirty_ = false;
    std::vector<std::u16string> lines;
    lines.reserve(static_cast<std::size_t>(std::max(0, close_ - 1)));
    QTextBlock block = editor_->document()->firstBlock().next();
    for (int n = 1; block.isValid() && n < close_; ++n, block = block.next()) {
        const QString text = block.text();
        lines.emplace_back(reinterpret_cast<const char16_t*>(text.utf16()), static_cast<std::size_t>(text.size()));
    }
    core::Properties properties = core::parseProperties(lines);
    error_ = properties.error;
    if (view_ == nullptr) {
        view_ = new PropertiesView(editor_->viewport());
        view_->hide();
        connect(view_, &PropertiesView::activated, this, &PropertiesEmbedder::activate);
        connect(view_, &PropertiesView::addRequested, this, &PropertiesEmbedder::addProperty);
        connect(view_, &PropertiesView::propertyEditCommitted, this, &PropertiesEmbedder::editCommitted);
        connect(view_, &PropertiesView::itemEditRequested, view_, &PropertiesView::editItem);
        connect(view_, &PropertiesView::itemEditCommitted, this, &PropertiesEmbedder::itemEditCommitted);
        connect(view_, &PropertiesView::newPropertyCommitted, this, &PropertiesEmbedder::addCommitted);
        connect(view_, &PropertiesView::deleteRequested, this, &PropertiesEmbedder::deleteProperty);
        connect(view_, &PropertiesView::itemDeleteRequested, this, &PropertiesEmbedder::deleteItem);
        connect(view_, &PropertiesView::undoRequested, this, &PropertiesEmbedder::undoPropertyEdit);
        connect(view_, &PropertiesView::redoRequested, this, &PropertiesEmbedder::redoPropertyEdit);
        connect(view_, &PropertiesView::pressed, this, &PropertiesEmbedder::setMouseHeld);
    }
    // The old rows go first: a new look lays out the rows it has at once.
    view_->reset(error_ ? std::vector<core::Property>{} : std::move(properties.rows));
    view_->setLook({style_, font_, width_});
}

void PropertiesEmbedder::caretMoved() {
    if (forgotten_) {
        schedule();
        return;
    }
    const bool stale = dirty_ || (view_ != nullptr && !error_ && !view_->built()) || close_ != confirmedClose();
    if (stale) {
        // Leaving the block after an edit: read and build at most 4 ms now, the rest in idle slices.
        if (caretInside() || held_ || !work(kKeyNanoseconds)) {
            schedule();
        }
        updateBadge();
        return;
    }
    apply();
    updateBadge();
}

bool PropertiesEmbedder::handleEditorClick(int blockNumber) {
    if (!shown_ || error_ || view_ == nullptr || blockNumber < 0 || blockNumber > close_) {
        return false;
    }
    view_->setFocus(Qt::MouseFocusReason);
    return true;
}

void PropertiesEmbedder::setMouseHeld(bool held) {
    held_ = held;
    if (!held_) {
        caretMoved();
    }
}

void PropertiesEmbedder::apply() {
    if (held_ || forgotten_) {
        return;
    }
    const bool want = enabled_ && close_ >= 0 && !error_ && !dirty_ && view_ != nullptr && view_->built();
    if (want && !shown_) {
        conceal();
    } else if (!want && shown_) {
        reveal();
    } else if (shown_) {
        const qreal height = view_->height() + 2 * style_.margin;
        const QTextBlock first = editor_->document()->firstBlock();
        if (std::abs(layout_->reservedHeight(first) - height) > 0.5) {
            layout_->setReservedHeight(first, height);
        }
        reposition();
    }
}

void PropertiesEmbedder::conceal() {
    const BlockLayout::Batch batch(layout_);
    const QTextBlock first = editor_->document()->firstBlock();
    shown_ = true;
    hiddenTo_ = close_;
    int n = 1;
    for (QTextBlock b = first.next(); b.isValid() && n <= close_; b = b.next(), ++n) {
        layout_->setHidden(b, true);
    }
    layout_->setReservedHeight(first, view_->height() + 2 * style_.margin);
    view_->show();
    reposition();
}

void PropertiesEmbedder::reveal() {
    const BlockLayout::Batch batch(layout_);
    const int to = hiddenTo_;
    shown_ = false;
    hiddenTo_ = -1;
    const QTextBlock first = editor_->document()->firstBlock();
    layout_->setReservedHeight(first, 0);
    int n = 1;
    for (QTextBlock b = first.next(); b.isValid() && n <= to; b = b.next(), ++n) {
        if (!b.isVisible() && !layout_->claimedByOther(b, this)) {
            layout_->setHidden(b, false);
        }
    }
    if (view_ != nullptr) {
        view_->hide();
    }
}

void PropertiesEmbedder::documentChanged(int position, int charsRemoved, int charsAdded) {
    (void)charsRemoved;
    (void)charsAdded;
    if (close_ < 0) {
        return;  // new front matter comes through the styler's confirmation
    }
    // Only an edit at or above the closing line can change the YAML (review O1): joining the line
    // below to the closing line reports a position on the closing line.
    const QTextBlock block = editor_->document()->findBlock(position);
    if (!block.isValid() || block.blockNumber() <= close_) {
        dirty_ = true;
        schedule();
    }
}

void PropertiesEmbedder::forgetAll() {
    forgotten_ = true;
    shown_ = false;
    hiddenTo_ = -1;
    schedule();
}

void PropertiesEmbedder::reposition() {
    const qreal margin = editor_->document()->documentMargin();
    const QPointF offset = editor_->contentOffset();
    const QTextBlock first = editor_->document()->firstBlock();
    const QRectF rect = layout_->blockBoundingRect(first);
    if (shown_ && view_ != nullptr) {
        view_->move(static_cast<int>(margin + offset.x()),
                    static_cast<int>(std::lround(rect.top() + offset.y() + style_.margin)));
    }
    if (badge_ != nullptr && badge_->isVisible()) {
        const QTextLayout* layout = first.layout();
        if (layout == nullptr || layout->lineCount() == 0) {
            layout_->ensureBlockLayout(first);
        }
        if (layout != nullptr && layout->lineCount() > 0) {
            const QTextLine line = layout->lineAt(0);
            const qreal gap = QFontMetricsF(badgeFont_).horizontalAdvance(QLatin1Char(' ')) * 2;
            const qreal x = rect.left() + layout->position().x() + line.x() + line.naturalTextWidth() + gap;
            const qreal y = rect.top() + layout->position().y() + line.y() + (line.height() - badge_->height()) / 2;
            badge_->move(static_cast<int>(x + offset.x()), static_cast<int>(std::lround(y + offset.y())));
        }
    }
}

void PropertiesEmbedder::updateBadge() {
    const QString text = badge();
    if (text.isEmpty()) {
        if (badge_ != nullptr) {
            badge_->hide();
        }
        return;
    }
    if (badge_ == nullptr) {
        badge_ = new Badge(editor_->viewport());
    }
    static_cast<Badge*>(badge_)->set(text, badgeFont_, toQColor(style_.badgeText));
    badge_->show();
    reposition();
}

bool PropertiesEmbedder::handleEditorKey(QKeyEvent* event) {
    if (!shown_ || event->key() != Qt::Key_Up ||
        (event->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) != Qt::NoModifier) {
        return false;
    }
    QTextCursor cursor = editor_->textCursor();
    if (cursor.blockNumber() != close_ + 1) {
        return false;
    }
    if (const QTextLayout* layout = cursor.block().layout();
        layout != nullptr && layout->lineCount() > 1 &&
        layout->lineForTextPosition(cursor.positionInBlock()).lineNumber() > 0) {
        return false;  // a wrapped line: Up stays in it
    }
    // The properties view has no source-navigation boundary: Up must not expose its YAML.
    return true;
}

void PropertiesEmbedder::activate(int row, bool extend, bool key) {
    if (!shown_ || view_ == nullptr) {
        return;
    }
    if (!extend) {
        if (row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
            return;
        }
        const core::Property& property = view_->property(static_cast<std::size_t>(row));
        if (!key && property.type == core::PropertyType::Checkbox) {
            replaceProperty(row, property.checked ? QStringLiteral("false") : QStringLiteral("true"));
            return;
        }
        view_->editKeyOrValue(row, key);
        return;
    }
    // A row: the first character of its value in the YAML; the title or elsewhere: the start of the
    // first YAML line.
    int line = 1;
    int column = 0;
    if (row >= 0 && static_cast<std::size_t>(row) < view_->rowCount()) {
        const core::Property& p = view_->property(static_cast<std::size_t>(row));
        line = 1 + static_cast<int>(p.valueLine);
        column = static_cast<int>(p.valueColumn);
    }
    const QTextBlock block = editor_->document()->findBlockByNumber(std::min(line, close_));
    QTextCursor cursor = editor_->textCursor();
    cursor.setPosition(block.position() + std::min(column, block.length() - 1),
                       extend ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
    editor_->setFocus();
    editor_->setTextCursor(cursor);
}

void PropertiesEmbedder::addProperty() {
    if (view_ != nullptr) {
        view_->editNewProperty();
    }
}

bool PropertiesEmbedder::keyExists(std::u16string_view key, int exceptRow) const {
    if (view_ == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < view_->rowCount(); ++i) {
        if (static_cast<int>(i) != exceptRow && view_->property(i).key == key) {
            return true;
        }
    }
    return false;
}

void PropertiesEmbedder::editCommitted(int row, bool key, const QString& value) {
    if (view_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
        return;
    }
    const core::Property& property = view_->property(static_cast<std::size_t>(row));
    if (key) {
        const auto keyView = hitokoto::ui::view(value);
        if (value.trimmed().isEmpty()) {
            view_->rejectInlineEdit(tr("Property names cannot be empty."));
            return;
        }
        if (keyView != property.key && keyExists(keyView, row)) {
            view_->rejectInlineEdit(tr("A property with that name already exists."));
            return;
        }
        view_->finishInlineEdit();
        if (keyView == property.key) {
            return;
        }
        if (property.type == core::PropertyType::List || property.type == core::PropertyType::Tags) {
            replacePropertyItems(row, property.items, value);
        } else {
            QString currentValue;
            if (property.type == core::PropertyType::Checkbox) {
                currentValue = property.checked ? QStringLiteral("true") : QStringLiteral("false");
            } else {
                currentValue = toQString(property.text);
            }
            replaceProperty(row, currentValue, value);
        }
        return;
    }
    if (hitokoto::ui::view(value) == property.text) {
        view_->finishInlineEdit();
        return;
    }
    view_->finishInlineEdit();
    replaceProperty(row, value);
}

void PropertiesEmbedder::itemEditCommitted(int row, int item, const QString& value) {
    if (view_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
        return;
    }
    const core::Property& property = view_->property(static_cast<std::size_t>(row));
    std::vector<std::u16string> items = property.items;
    const std::u16string itemText(hitokoto::ui::view(value));
    if (item < 0 || static_cast<std::size_t>(item) > items.size()) {
        view_->finishInlineEdit();
        return;
    }
    if (static_cast<std::size_t>(item) < items.size() && items[static_cast<std::size_t>(item)] == itemText) {
        view_->finishInlineEdit();
        return;
    }
    if (static_cast<std::size_t>(item) == items.size()) {
        if (value.isEmpty()) {
            view_->finishInlineEdit();
            return;
        }
        items.push_back(itemText);
    } else if (value.isEmpty()) {
        items.erase(items.begin() + item);
    } else {
        items[static_cast<std::size_t>(item)] = itemText;
    }
    view_->finishInlineEdit();
    replacePropertyItems(row, items);
}

void PropertiesEmbedder::addCommitted(const QString& key) {
    if (view_ == nullptr) {
        return;
    }
    if (key.trimmed().isEmpty()) {
        view_->rejectInlineEdit(tr("Property names cannot be empty."));
        return;
    }
    if (keyExists(hitokoto::ui::view(key))) {
        view_->rejectInlineEdit(tr("A property with that name already exists."));
        return;
    }
    view_->finishInlineEdit();
    insertProperty(key, {});
}

void PropertiesEmbedder::undoPropertyEdit() {
    if (propertyUndoSteps_ <= 0) {
        editor_->undoEdit();
        return;
    }
    const int steps = std::exchange(propertyUndoSteps_, 0);
    for (int i = 0; i < steps; ++i) {
        editor_->undoEdit();
    }
    propertyRedoSteps_ = steps;
}

void PropertiesEmbedder::redoPropertyEdit() {
    if (propertyRedoSteps_ <= 0) {
        editor_->redoEdit();
        return;
    }
    const int steps = std::exchange(propertyRedoSteps_, 0);
    for (int i = 0; i < steps; ++i) {
        editor_->redoEdit();
    }
    propertyUndoSteps_ = steps;
}

void PropertiesEmbedder::deleteProperty(int row) {
    if (view_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
        return;
    }
    const std::size_t startLine = 1 + view_->property(static_cast<std::size_t>(row)).keyLine;
    const std::size_t endLine = row + 1 < static_cast<int>(view_->rowCount())
                                    ? 1 + view_->property(static_cast<std::size_t>(row + 1)).keyLine
                                    : static_cast<std::size_t>(close_);
    QTextBlock start = editor_->document()->findBlockByNumber(static_cast<int>(startLine));
    QTextBlock end = editor_->document()->findBlockByNumber(static_cast<int>(endLine));
    if (!start.isValid() || !end.isValid()) {
        return;
    }
    const int stepsBefore = editor_->document()->availableUndoSteps();
    QTextCursor cursor(editor_->document());
    cursor.beginEditBlock();
    cursor.setPosition(start.position());
    cursor.setPosition(end.position(), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    recordUndoGroup(stepsBefore);
}

void PropertiesEmbedder::deleteItem(int row, int item) {
    if (view_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
        return;
    }
    const core::Property& property = view_->property(static_cast<std::size_t>(row));
    if (item < 0 || static_cast<std::size_t>(item) >= property.items.size()) {
        return;
    }
    std::vector<std::u16string> items = property.items;
    items.erase(items.begin() + item);
    replacePropertyItems(row, items);
}

void PropertiesEmbedder::replaceProperty(int row, const QString& value, const std::optional<QString>& key) {
    if (view_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
        return;
    }
    const core::Property property = view_->property(static_cast<std::size_t>(row));
    QString encodedValue;
    if (property.type == core::PropertyType::Checkbox &&
        (value == QStringLiteral("true") || value == QStringLiteral("false"))) {
        encodedValue = value;
    } else {
        encodedValue = toQString(core::serializePropertyValue(property.type, hitokoto::ui::view(value)));
    }
    const std::u16string_view propertyKey = key ? hitokoto::ui::view(*key) : property.key;
    const QString line = toQString(core::serializePropertyScalar(propertyKey)) + QStringLiteral(": ") + encodedValue;
    const std::size_t startLine = 1 + property.keyLine;
    const std::size_t endLine = row + 1 < static_cast<int>(view_->rowCount())
                                    ? 1 + view_->property(static_cast<std::size_t>(row + 1)).keyLine
                                    : static_cast<std::size_t>(close_);
    QTextBlock start = editor_->document()->findBlockByNumber(static_cast<int>(startLine));
    QTextBlock end = editor_->document()->findBlockByNumber(static_cast<int>(endLine));
    if (!start.isValid() || !end.isValid()) {
        return;
    }
    const int stepsBefore = editor_->document()->availableUndoSteps();
    QTextCursor cursor(editor_->document());
    cursor.beginEditBlock();
    cursor.setPosition(start.position());
    cursor.setPosition(end.position(), QTextCursor::KeepAnchor);
    cursor.insertText(line + QLatin1Char('\n'));
    cursor.endEditBlock();
    recordUndoGroup(stepsBefore);
}

void PropertiesEmbedder::replacePropertyItems(int row, const std::vector<std::u16string>& items,
                                              const std::optional<QString>& key) {
    if (view_ == nullptr || row < 0 || static_cast<std::size_t>(row) >= view_->rowCount()) {
        return;
    }
    const core::Property& property = view_->property(static_cast<std::size_t>(row));
    const std::u16string_view propertyKey = key ? hitokoto::ui::view(*key) : property.key;
    const QString line = toQString(core::serializePropertyScalar(propertyKey)) + QStringLiteral(": ") +
                         toQString(core::serializePropertyList(items));
    const std::size_t startLine = 1 + property.keyLine;
    const std::size_t endLine = row + 1 < static_cast<int>(view_->rowCount())
                                    ? 1 + view_->property(static_cast<std::size_t>(row + 1)).keyLine
                                    : static_cast<std::size_t>(close_);
    QTextBlock start = editor_->document()->findBlockByNumber(static_cast<int>(startLine));
    QTextBlock end = editor_->document()->findBlockByNumber(static_cast<int>(endLine));
    if (!start.isValid() || !end.isValid()) {
        return;
    }
    const int stepsBefore = editor_->document()->availableUndoSteps();
    QTextCursor cursor(editor_->document());
    cursor.beginEditBlock();
    cursor.setPosition(start.position());
    cursor.setPosition(end.position(), QTextCursor::KeepAnchor);
    cursor.insertText(line + QLatin1Char('\n'));
    cursor.endEditBlock();
    recordUndoGroup(stepsBefore);
}

void PropertiesEmbedder::recordUndoGroup(int stepsBefore) {
    propertyUndoSteps_ = std::max(1, editor_->document()->availableUndoSteps() - stepsBefore);
    propertyRedoSteps_ = 0;
}

void PropertiesEmbedder::insertProperty(const QString& key, const QString& value) {
    if (close_ < 0) {
        return;
    }
    const QTextBlock closing = editor_->document()->findBlockByNumber(close_);
    if (!closing.isValid()) {
        return;
    }
    const QString line = toQString(core::serializePropertyScalar(hitokoto::ui::view(key))) + QStringLiteral(": ") +
                         toQString(core::serializePropertyScalar(hitokoto::ui::view(value))) + QLatin1Char('\n');
    const int stepsBefore = editor_->document()->availableUndoSteps();
    QTextCursor cursor(editor_->document());
    cursor.beginEditBlock();
    cursor.setPosition(closing.position());
    cursor.insertText(line);
    cursor.endEditBlock();
    recordUndoGroup(stepsBefore);
}

}  // namespace hitokoto::ui
