// SPDX-License-Identifier: GPL-3.0-or-later
#include "list_editing.hpp"

#include "hitokoto/core/bullet.hpp"
#include "hitokoto/core/task.hpp"
#include "qt_bridge.hpp"

#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>
#include <utility>

namespace hitokoto::ui {

namespace {

/// Changes the start of `block` as indentBulletLine or outdentBulletLine changes its line (both
/// add or remove characters at the start only); returns how far the text after them moved.
int reindent(const QTextBlock& block, bool outdent) {
    const QString line = block.text();
    if (!outdent) {
        const QString indented = toQString(core::indentBulletLine(view(line)));
        const auto added = indented.size() - line.size();
        QTextCursor lineStart(block);
        lineStart.insertText(indented.left(added));
        return static_cast<int>(added);
    }
    const auto removed = line.size() - toQString(core::outdentBulletLine(view(line))).size();
    if (removed > 0) {
        QTextCursor remover(block);
        remover.setPosition(block.position() + static_cast<int>(removed), QTextCursor::KeepAnchor);
        remover.removeSelectedText();
    }
    return -static_cast<int>(removed);
}

}  // namespace

ListEditing::ListEditing(std::function<bool(const QTextBlock&)> inCodeBlock) : inCodeBlock_(std::move(inCodeBlock)) {}

bool ListEditing::isBulletLine(const QTextBlock& block) const {
    return core::parseBulletLine(view(block.text())).has_value() && !inCodeBlock_(block);
}

ListEditing::Outcome ListEditing::enter(QTextCursor& cursor) const {
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
    // S051: a task item continues with an unchecked box and counts as empty without text after it.
    const auto task = core::parseTaskLine(view(block.text()));
    const bool isEmpty = task.has_value() ? task->isEmpty : info->isEmpty;
    const auto prefixLength = task.has_value() ? task->contentStart : info->prefixLength;

    if (isEmpty) {
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

    if (posInBlock >= static_cast<int>(prefixLength)) {
        cursor.beginEditBlock();
        const QString prefix = task.has_value() ? toQString(core::continuedTaskPrefix(view(block.text())))
                                                : block.text().left(static_cast<qsizetype>(info->prefixLength));
        cursor.insertText(QLatin1Char('\n') + prefix);
        cursor.endEditBlock();
        return Outcome::CaretPlaced;
    }

    return Outcome::None;
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
        if (!isBulletLine(startBlock)) {
            return Outcome::None;
        }
        cursor.beginEditBlock();
        const int oldPos = cursor.position();
        const int moved = reindent(startBlock, outdent);
        if (moved != 0) {
            cursor.setPosition(std::max(startBlock.position(), oldPos + moved));
        }
        cursor.endEditBlock();
        return Outcome::CaretPlaced;
    }

    bool anyBullet = false;
    for (QTextBlock b = startBlock; b.isValid() && b.blockNumber() <= endBlock.blockNumber(); b = b.next()) {
        if (isBulletLine(b)) {
            anyBullet = true;
            break;
        }
    }
    if (!anyBullet) {
        return Outcome::None;
    }

    cursor.beginEditBlock();
    for (QTextBlock b = startBlock; b.isValid() && b.blockNumber() <= endBlock.blockNumber(); b = b.next()) {
        if (isBulletLine(b)) {
            (void)reindent(b, outdent);
        }
    }
    cursor.endEditBlock();
    return Outcome::Edited;
}

}  // namespace hitokoto::ui
