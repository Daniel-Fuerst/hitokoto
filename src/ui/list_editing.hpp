// SPDX-License-Identifier: GPL-3.0-or-later
// S003: Enter, Tab and Shift+Tab in bullet items, as edits through a cursor of the document. The
// core says what a bullet line is and how it is indented (bullet.hpp); this applies that to the
// lines at a cursor, each key one undo step.
#pragma once

#include <QTextCursor>

#include <functional>

class QTextBlock;

namespace hitokoto::ui {

class ListEditing {
public:
    /// What a key did.
    enum class Outcome {
        None,         ///< not a key for the list: the editor handles it as usual
        Edited,       ///< the lines changed; the caret moved with the text
        CaretPlaced,  ///< the lines changed; `cursor` holds the caret to set
    };

    /// `inCodeBlock` tells whether a block is a line of a fenced code block, where a bullet line
    /// is code and these keys are not list keys.
    explicit ListEditing(std::function<bool(const QTextBlock&)> inCodeBlock);

    /// Enter in a bullet item: continues the list with the item's prefix, or ends it on an empty
    /// item (outdents it one level, or removes its marker at the top level). `cursor` is the caret.
    [[nodiscard]] Outcome enter(QTextCursor& cursor) const;
    /// Tab (`outdent` false) or Shift+Tab on the bullet lines of `cursor`'s selection: each is
    /// indented or outdented one level as core::indentBulletLine or core::outdentBulletLine says.
    [[nodiscard]] Outcome tab(QTextCursor& cursor, bool outdent) const;

private:
    [[nodiscard]] bool isBulletLine(const QTextBlock& block) const;

    std::function<bool(const QTextBlock&)> inCodeBlock_;
};

}  // namespace hitokoto::ui
