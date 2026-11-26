// SPDX-License-Identifier: GPL-3.0-or-later
// What the reader sees stays where it is (S033): the reading position across resizes and theme
// changes, the text on screen while blocks above it change height, and the caret on screen while
// it is followed. Owns the state for that and the connections to the editor's vertical scroll bar.
#pragma once

#include <QObject>
#include <QTextCursor>

class QTextEdit;

namespace hitokoto::ui {

class BlockLayout;
class SmoothScroller;

class ViewAnchor : public QObject {
    Q_OBJECT
public:
    /// The reading position: the first character of the line at the top of the viewport (a cursor,
    /// so that it follows edits) and how far below the top of that line the view starts. Null at
    /// the top of the document.
    struct ScrollAnchor {
        QTextCursor text;
        qreal offset = 0;
    };

    /// For the lifetime of an Input the editor handles a key, an input method's text, a paste,
    /// undo or redo: Qt scrolls to the caret from layouts the edit has not updated yet, which is
    /// no scroll of the reader's, and the caret is followed further.
    class Input {
    public:
        explicit Input(ViewAnchor& anchor) : anchor_(anchor), previous_(anchor.typing_) { anchor_.typing_ = true; }
        ~Input() { anchor_.typing_ = previous_; }
        Input(const Input&) = delete;
        Input& operator=(const Input&) = delete;

    private:
        ViewAnchor& anchor_;
        bool previous_;
    };

    /// For the lifetime of a Reshape the editor takes a new width or theme: scrolls are none of the
    /// reader's, and the caret is not scrolled back on screen.
    class Reshape {
    public:
        explicit Reshape(ViewAnchor& anchor) : anchor_(anchor), previous_(anchor.resizing_) {
            anchor_.resizing_ = true;
        }
        ~Reshape() { anchor_.resizing_ = previous_; }
        Reshape(const Reshape&) = delete;
        Reshape& operator=(const Reshape&) = delete;

    private:
        ViewAnchor& anchor_;
        bool previous_;
    };

    /// `insideEdit`: whether the editor is between contentsChange and contentsChanged of its
    /// document (it outlives this object).
    ViewAnchor(QTextEdit* editor, BlockLayout* layout, SmoothScroller* scroller, const bool& insideEdit);

    /// Inside a Reshape.
    [[nodiscard]] bool reshaping() const noexcept { return resizing_; }
    /// The reading position held since the last scroll, taken now if none is held: consecutive
    /// resizes and theme changes return to the same line instead of a position taken anew each
    /// time, which would drift (S033).
    [[nodiscard]] ScrollAnchor readingAnchor();
    /// After a new width or theme: shows `anchor` at the top again as far as the document allows.
    void restore(const ScrollAnchor& anchor);
    /// Restores the held reading position once more when the event loop runs, if it moved.
    void settleLater();
    /// How often a reading position was restored after a new width or theme (instrumentation for
    /// tests, S033 review O3).
    [[nodiscard]] quint64 restores() const noexcept { return restores_; }

private:
    [[nodiscard]] ScrollAnchor captureAnchor() const;
    /// The scroll value that shows `anchor` at the top (lays its block out).
    [[nodiscard]] int anchorValue(const ScrollAnchor& anchor) const;
    /// Scrolls `anchor` to the top as far as the document allows. It may make a scroll range larger
    /// for heights not taken in yet, never a range that does not scroll: that would show the scroll
    /// bar for a document that fits the view.
    void restoreAnchor(const ScrollAnchor& anchor);
    /// Restores the held reading position again if it moved since a resize or theme change.
    void settleAnchor();
    void keepContentStill(qreal top, qreal oldHeight, qreal newHeight);
    void applyPendingShift();
    /// Whether the caret is wholly on screen.
    [[nodiscard]] bool caretOnScreen() const;
    /// Checks once the event loop runs whether the followed caret left the screen.
    void scheduleCaretCheck();
    /// Scrolls the followed caret back on screen (S033: heights change while blocks are restyled in
    /// idle slices).
    void keepCaretOnScreen();

    QTextEdit* editor_;
    BlockLayout* layout_;
    SmoothScroller* scroller_;
    const bool& insideEdit_;
    qreal pendingShift_ = 0;  ///< height change above the viewport not yet scrolled away
    bool applyingShift_ = false;
    bool resizing_ = false;
    ScrollAnchor heldAnchor_;        ///< reading position across resizes until the view scrolls
    bool rangeClamps_ = false;       ///< a new scroll range is about to clamp the value
    bool followCaret_ = false;       ///< the view follows the caret (no scroll left it off screen)
    bool followingCaret_ = false;    ///< keepCaretOnScreen is scrolling
    bool typing_ = false;            ///< inside an Input
    bool caretCheckQueued_ = false;  ///< a caret check waits for the event loop
    int settlePasses_ = 0;           ///< settleAnchor calls that moved the view since the last restore
    /// settleAnchor shows the held reading position again at most this often after a restore. In the
    /// tests, and in probes over several widths and fonts, it took at most one pass; the limit only
    /// ends a back and forth if the layout does not come to rest. After it, the position may be off
    /// by the last correction until the reader scrolls (review O10).
    static constexpr int kMaxSettlePasses = 4;
    quint64 restores_ = 0;  ///< reading positions restored by a resize or theme change
};

}  // namespace hitokoto::ui
