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
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace hitokoto::ui {

namespace {

constexpr qint64 kKeyNanoseconds = 4'000'000;    ///< reading and building on a key (S024)
constexpr qint64 kSliceNanoseconds = 6'000'000;  ///< an idle slice, as the styler's

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
    QPointer<PropertiesEmbedder> self(this);
    styler_->addPendingWork([self] { return self && self->busy(); });
}

PropertiesEmbedder::~PropertiesEmbedder() {
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
    const int close = enabled_ && hasLook_ && !styler_->frontMatterPending() ? styler_->frontMatterClose() : -1;
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
        if (shown_) {
            reveal();
        }
        read();
    }
    if (!error_ && view_ != nullptr && !view_->built() && !view_->build(timer, nanoseconds)) {
        return false;  // the YAML shows until the block is built
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
    const bool stale =
        dirty_ || (view_ != nullptr && !error_ && !view_->built()) ||
        close_ != (enabled_ && hasLook_ && !styler_->frontMatterPending() ? styler_->frontMatterClose() : -1);
    if (stale) {
        // Leaving the block after an edit: read and build at most 4 ms now, the rest in idle slices.
        if (!caretInside() && !held_ && !work(kKeyNanoseconds)) {
            schedule();
        } else if (caretInside() || held_) {
            schedule();
        }
        updateBadge();
        return;
    }
    apply();
    updateBadge();
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
    const bool want =
        enabled_ && close_ >= 0 && !error_ && !dirty_ && view_ != nullptr && view_->built() && !caretInside();
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
    // An edit at or above the line after the closing line can change the YAML.
    const QTextBlock block = editor_->document()->findBlock(position);
    if (!block.isValid() || block.blockNumber() <= close_ + 1) {
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
    const QTextBlock closing = editor_->document()->findBlockByNumber(close_);
    const int column = std::min(cursor.positionInBlock(), closing.length() - 1);
    cursor.setPosition(closing.position() + column, (event->modifiers() & Qt::ShiftModifier) != 0
                                                        ? QTextCursor::KeepAnchor
                                                        : QTextCursor::MoveAnchor);
    editor_->setTextCursor(cursor);
    return true;
}

void PropertiesEmbedder::activate(int row, bool extend) {
    if (!shown_ || view_ == nullptr) {
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

}  // namespace hitokoto::ui
