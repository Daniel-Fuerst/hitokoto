// SPDX-License-Identifier: GPL-3.0-or-later
// S024: shows a note's front matter as a properties block (PropertiesView) while the caret is
// elsewhere, and as its YAML while the caret and the anchor of the selection are both in it. The
// first line reserves the block's height in BlockLayout (ADR 0003), the other lines are hidden,
// as a table's are (S018); nothing is ever written from the block (ADR 0002). Reading the YAML and
// building the block take at most 4 ms on a key and go on in idle slices (review O7, O8).
#pragma once

#include "block_layout.hpp"
#include "hitokoto/core/properties.hpp"
#include "hitokoto/core/theme.hpp"

#include <QFont>
#include <QObject>
#include <QTimer>

#include <optional>

class QKeyEvent;

namespace hitokoto::ui {

class BlockStyler;
class EditorWidget;
class PropertiesView;

class PropertiesEmbedder : public QObject, public BlockLayout::Claimant {
    Q_OBJECT
public:
    PropertiesEmbedder(EditorWidget* editor, BlockStyler* styler, BlockLayout* layout);
    ~PropertiesEmbedder() override;
    PropertiesEmbedder(const PropertiesEmbedder&) = delete;
    PropertiesEmbedder& operator=(const PropertiesEmbedder&) = delete;

    /// The lines of the front matter while the properties block stands for them.
    [[nodiscard]] bool claims(const QTextBlock& block) const override;

    /// S024 review O2: off shows front matter as S023 does (plain text, no block, no badge).
    void setEnabled(bool enabled);
    void setLook(const core::PropertiesStyle& style, const QFont& font, const QFont& badgeFont);
    void setAvailableWidth(int width);

    /// The block while it is shown, else null.
    [[nodiscard]] QWidget* view() const;
    /// "Invalid YAML (line n): reason" while the YAML is not read as properties and the caret is
    /// outside the block, else empty.
    [[nodiscard]] QString badge() const;
    /// Reading or building goes on in idle slices.
    [[nodiscard]] bool busy() const noexcept { return work_.isActive(); }

    /// The caret or the selection moved: show the YAML or the block.
    void caretMoved();
    /// A mouse button went down in the editor or on the block (true) or up (false): while one is
    /// held, the block keeps its form (review O6).
    void setMouseHeld(bool held);
    /// A change to the document; lines of the front matter are read again.
    void documentChanged(int position, int charsRemoved, int charsAdded);
    /// The document's blocks are gone (a replaced document): forget them without touching them.
    void forgetAll();
    /// Reading and building again (front matter confirmed, styling complete).
    void schedule();
    /// Moves the block and the badge to the first line (after scrolling or layout changes).
    void reposition();
    /// Up from the first line below the block goes to the closing line. True if handled.
    bool handleEditorKey(QKeyEvent* event);
    /// After opening (Armin, 2026-10-31): the caret at the start of the first line below closed
    /// front matter with valid YAML, unless that line is a table, else at the start of the
    /// document. `loaded`: the whole file is in; front matter still being confirmed then places the
    /// caret when it is, unless it moved.
    void placeCaretAfterOpening(bool loaded);

private:
    /// The closing line the styler confirmed, or -1 (none, still confirming, toggle off).
    [[nodiscard]] int confirmedClose() const;
    /// The caret below the front matter if its YAML is valid and a line that is no table follows
    /// it; false if not.
    bool placeCaretBelow();
    /// Reads the YAML and builds the block for at most `nanoseconds`, then shows what the caret
    /// asks for; true when nothing is left to do.
    bool work(qint64 nanoseconds);
    void slice();
    /// The caret and the anchor are both on lines of the front matter.
    [[nodiscard]] bool caretInside() const;
    /// Shows the block or the YAML, as the caret asks; nothing while a mouse button is held.
    void apply();
    void conceal();
    void reveal();
    void read();
    void activate(int row, bool extend);
    void updateBadge();

    EditorWidget* editor_;
    BlockStyler* styler_;
    BlockLayout* layout_;
    PropertiesView* view_ = nullptr;  ///< owned by the editor's viewport
    QWidget* badge_ = nullptr;        ///< the badge after the first line, an accessible element
    core::PropertiesStyle style_;
    QFont font_;
    QFont badgeFont_;
    bool hasLook_ = false;
    int width_ = 400;
    bool enabled_ = true;
    int close_ = -1;             ///< closing line of the front matter read last, -1 = none
    bool dirty_ = true;          ///< the lines changed since they were read
    bool shown_ = false;         ///< the block stands for the lines (they are hidden)
    int hiddenTo_ = -1;          ///< last line hidden by conceal()
    bool held_ = false;          ///< a mouse button is held
    bool forgotten_ = false;     ///< blocks gone: touch nothing until the next slice
    bool afterOpening_ = false;  ///< opened while the front matter was being confirmed
    std::optional<core::PropertiesError> error_;
    QTimer work_;
};

}  // namespace hitokoto::ui
