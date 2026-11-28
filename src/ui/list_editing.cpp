// SPDX-License-Identifier: GPL-3.0-or-later
#include "list_editing.hpp"

#include "hitokoto/core/bullet.hpp"
#include "hitokoto/core/ordered_list.hpp"
#include "qt_bridge.hpp"

#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace hitokoto::ui {

namespace {

/// The lines of `document` for the core's renumbering, read block by block as it walks.
core::LineAt linesOf(const QTextDocument* document) {
    return [document](std::size_t i) -> std::optional<std::u16string> {
        if (i > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
        const QTextBlock block = document->findBlockByNumber(static_cast<int>(i));
        if (!block.isValid()) {
            return std::nullopt;
        }
        return block.text().toStdU16String();
    };
}

/// Rewrites the numbers the core asks for (S053). Each edit replaces digits within one line, so the
/// positions of the others stay valid; cursors in the document move with the text.
void applyNumberEdits(QTextDocument* document, const std::vector<core::NumberEdit>& edits) {
    for (const auto& edit : edits) {
        const QTextBlock block = document->findBlockByNumber(static_cast<int>(edit.line));
        if (!block.isValid()) {
            continue;
        }
        QTextCursor digits(block);
        digits.setPosition(block.position() + static_cast<int>(edit.digits.start));
        digits.setPosition(block.position() + static_cast<int>(edit.digits.end()), QTextCursor::KeepAnchor);
        digits.insertText(QString::fromStdU16String(edit.text));
    }
}

/// Changes the start of `block` as indentBulletLine or outdentBulletLine (for an ordered item,
/// indentOrderedLine or outdentOrderedLine) changes its line (all add or remove characters at the
/// start only); returns how far the text after them moved.
int reindent(const QTextBlock& block, bool outdent) {
    const QString line = block.text();
    const bool ordered = core::parseOrderedLine(view(line)).has_value();
    if (!outdent) {
        const QString indented =
            toQString(ordered ? core::indentOrderedLine(view(line)) : core::indentBulletLine(view(line)));
        const auto added = indented.size() - line.size();
        QTextCursor lineStart(block);
        lineStart.insertText(indented.left(added));
        return static_cast<int>(added);
    }
    const QString outdented =
        toQString(ordered ? core::outdentOrderedLine(view(line)) : core::outdentBulletLine(view(line)));
    const auto removed = line.size() - outdented.size();
    if (removed > 0) {
        QTextCursor remover(block);
        remover.setPosition(block.position() + static_cast<int>(removed), QTextCursor::KeepAnchor);
        remover.removeSelectedText();
    }
    return -static_cast<int>(removed);
}

/// Reindents `block` and, if it is an ordered item that moved, renumbers the lists it left and
/// joined (S053, decision 5).
int reindentAndRenumber(const QTextBlock& block, bool outdent) {
    const auto before = core::parseOrderedLine(view(block.text()));
    const int moved = reindent(block, outdent);
    if (before.has_value() && moved != 0) {
        QTextDocument* document = QTextCursor(block).document();
        applyNumberEdits(
            document, core::renumberAfterReindent(
                          linesOf(document), static_cast<std::size_t>(block.blockNumber()), before->column,
                          before->delimiter, outdent ? core::ReindentKind::Outdented : core::ReindentKind::Indented));
    }
    return moved;
}

}  // namespace

ListEditing::ListEditing(std::function<bool(const QTextBlock&)> inCodeBlock) : inCodeBlock_(std::move(inCodeBlock)) {}

bool ListEditing::isListLine(const QTextBlock& block) const {
    const auto line = view(block.text());
    return (core::parseBulletLine(line).has_value() || core::parseOrderedLine(line).has_value()) &&
           !inCodeBlock_(block);
}

ListEditing::Outcome ListEditing::enter(QTextCursor& cursor) const {
    if (core::parseOrderedLine(view(cursor.block().text())).has_value() && !inCodeBlock_(cursor.block())) {
        return enterOrdered(cursor);
    }
    QTextBlock block = cursor.block();
    const auto info = core::parseBulletLine(view(block.text()));
    if (!info.has_value() || inCodeBlock_(block)) {
        return Outcome::None;
    }

    if (cursor.hasSelection()) {
        cursor.removeSelectedText();
        block = cursor.block();
    }

    const int posInBlock = cursor.positionInBlock();

    if (info->isEmpty) {
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        if (info->markerChar > 0) {
            const QString outdented = toQString(core::outdentBulletLine(view(block.text())));
            cursor.insertText(outdented);
        } else {
            cursor.removeSelectedText();
        }
        cursor.endEditBlock();
        return Outcome::CaretPlaced;
    }

    if (posInBlock >= static_cast<int>(info->prefixLength)) {
        cursor.beginEditBlock();
        const QString prefix = block.text().left(static_cast<qsizetype>(info->prefixLength));
        cursor.insertText(QLatin1Char('\n') + prefix);
        cursor.endEditBlock();
        return Outcome::CaretPlaced;
    }

    return Outcome::None;
}

ListEditing::Outcome ListEditing::enterOrdered(QTextCursor& cursor) const {
    // The selection is removed in the same undo step as the rest, so it is opened first.
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        cursor.removeSelectedText();
    }
    const QTextBlock block = cursor.block();
    const QString line = block.text();
    const auto info = core::parseOrderedLine(view(line));
    if (!info.has_value()) {
        cursor.endEditBlock();
        return Outcome::CaretPlaced;  // the selection reached into the marker; it is gone
    }
    QTextDocument* document = cursor.document();
    const auto index = static_cast<std::size_t>(block.blockNumber());

    if (info->isEmpty) {
        // Outdents a nested item by its marker width, or removes the marker of a top-level item
        // (S003); then renumbers what that changed (S053, decision 4).
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        const bool nested = info->digits.start > 0;
        cursor.insertText(nested ? toQString(core::outdentOrderedLine(view(line))) : QString());
        applyNumberEdits(document, core::renumberAfterReindent(
                                       linesOf(document), index, info->column, info->delimiter,
                                       nested ? core::ReindentKind::Outdented : core::ReindentKind::Removed));
        cursor.endEditBlock();
        return Outcome::CaretPlaced;
    }

    if (cursor.positionInBlock() < static_cast<int>(info->prefixLength)) {
        cursor.endEditBlock();
        return Outcome::None;
    }
    cursor.insertText(QLatin1Char('\n') + toQString(core::nextOrderedPrefix(view(line), *info)));
    applyNumberEdits(document, core::renumberOrderedList(linesOf(document), index + 1U));
    cursor.endEditBlock();
    return Outcome::CaretPlaced;
}

ListEditing::Outcome ListEditing::tab(QTextCursor& cursor, bool outdent) const {
    const QTextDocument* document = cursor.document();
    const int startPos = cursor.selectionStart();
    const int endPos = cursor.selectionEnd();
    QTextBlock startBlock = document->findBlock(startPos);
    QTextBlock endBlock = document->findBlock(endPos);
    if (endPos > startPos && endBlock.position() == endPos) {
        endBlock = endBlock.previous();
    }

    if (!startBlock.isValid() || !endBlock.isValid()) {
        return Outcome::None;
    }

    if (startBlock == endBlock) {
        if (!isListLine(startBlock)) {
            return Outcome::None;
        }
        cursor.beginEditBlock();
        const auto before = core::parseOrderedLine(view(startBlock.text()));
        const int oldPos = cursor.position();
        const int moved = reindent(startBlock, outdent);
        if (moved != 0) {
            cursor.setPosition(std::max(startBlock.position(), oldPos + moved));
        }
        // Renumbering after the caret is placed: the caret then moves with the digits it follows.
        if (before.has_value() && moved != 0) {
            QTextDocument* doc = cursor.document();
            applyNumberEdits(
                doc, core::renumberAfterReindent(
                         linesOf(doc), static_cast<std::size_t>(startBlock.blockNumber()), before->column,
                         before->delimiter, outdent ? core::ReindentKind::Outdented : core::ReindentKind::Indented));
        }
        cursor.endEditBlock();
        return Outcome::CaretPlaced;
    }

    bool anyItem = false;
    for (QTextBlock b = startBlock; b.isValid() && b.blockNumber() <= endBlock.blockNumber(); b = b.next()) {
        if (isListLine(b)) {
            anyItem = true;
            break;
        }
    }
    if (!anyItem) {
        return Outcome::None;
    }

    cursor.beginEditBlock();
    for (QTextBlock b = startBlock; b.isValid() && b.blockNumber() <= endBlock.blockNumber(); b = b.next()) {
        if (isListLine(b)) {
            (void)reindentAndRenumber(b, outdent);
        }
    }
    cursor.endEditBlock();
    return Outcome::Edited;
}

}  // namespace hitokoto::ui
