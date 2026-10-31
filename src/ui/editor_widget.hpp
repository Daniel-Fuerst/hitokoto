// SPDX-License-Identifier: GPL-3.0-or-later
// The editing surface: a QTextEdit whose document is the only copy of the text (ADR 0002) and is
// laid out by BlockLayout (ADR 0003). Rendering comes from the core through BlockStyler; this
// class adds the cursor-aware reveal, bullet drawing and click handling.
#pragma once

#include "block_layout.hpp"
#include "block_styler.hpp"
#include "hitokoto/core/theme.hpp"

#include <QPointF>
#include <QTextBlock>
#include <QTextEdit>

#include <memory>
#include <optional>

class QMimeData;
class QPainter;

class QTableView;

namespace hitokoto::ui {

class PropertiesEmbedder;
class TableEmbedder;

class EditorWidget : public QTextEdit {
    Q_OBJECT
public:
    explicit EditorWidget(const core::ElementRegistry& registry, QWidget* parent = nullptr);
    ~EditorWidget() override;

    void applyTheme(const core::Theme& theme);

    [[nodiscard]] BlockStyler& styler() noexcept { return *styler_; }
    /// The table view showing the table whose lines include `blockNumber`; null if the line is not
    /// in a table or the table shows its source (S018).
    [[nodiscard]] QTableView* tableViewAt(int blockNumber) const;
    /// Whether the table whose lines include `blockNumber` shows its Markdown lines (S018).
    [[nodiscard]] bool isTableSourceShown(int blockNumber) const;
    /// The document layout (ADR 0003).
    [[nodiscard]] BlockLayout* blockLayout() const noexcept { return layout_; }
    /// Counts real text edits (not formatting); used to decide whether a save is current.
    [[nodiscard]] quint64 editCount() const noexcept { return editCount_; }
    /// First and last block number on screen.
    [[nodiscard]] std::pair<int, int> visibleBlockRange() const;
    /// The language badge painted on `block` (an opening fence while concealed), else empty (S021).
    [[nodiscard]] QString codeBlockBadge(const QTextBlock& block);
    /// S024: the properties block shown in place of the front matter; null while there is none, the
    /// YAML shows, or the toggle is off.
    [[nodiscard]] QWidget* propertiesView() const;
    /// S024: the badge after the first line of front matter that is not shown as properties
    /// ("Invalid YAML (line n): reason"), else empty.
    [[nodiscard]] QString frontMatterBadge() const;
    /// S024 review O2: front matter as a properties block (on) or as plain text, as in S023 (off).
    void setFrontMatterAsProperties(bool on);

signals:
    void painted();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    friend class TableEmbedder;
    friend class PropertiesEmbedder;

    // Block geometry as QPlainTextEdit offered it: document coordinates, and the offset that maps
    // them into the viewport.
    [[nodiscard]] QRectF blockBoundingGeometry(const QTextBlock& block) const;
    [[nodiscard]] QPointF contentOffset() const;
    [[nodiscard]] QTextBlock firstVisibleBlock() const;
    /// The line at the top of the viewport and how far into it the view starts.
    struct ScrollAnchor {
        QTextBlock block;
        qreal fraction = 0;
    };
    [[nodiscard]] ScrollAnchor captureAnchor() const;
    void restoreAnchor(const ScrollAnchor& anchor);
    void keepContentStill(qreal top, qreal oldHeight, qreal newHeight);
    void applyPendingShift();

    void updateReveal();
    void updateMargins();
    [[nodiscard]] int availableTableWidth() const;
    void paintCodeBlockPanels();
    void paintInlineCodeBoxes(QPainter& painter, const QTextBlock& block, const core::LineRender& render,
                              QPointF origin, const core::CodeStyle& style);
    void paintDecorations();
    /// S023: the line of a concealed thematic break (`rect`: the block in viewport coordinates).
    void paintThematicBreak(QPainter& painter, const QTextBlock& block, const QRectF& rect);

    BlockLayout* layout_ = nullptr;
    qreal pendingShift_ = 0;  ///< height change above the viewport not yet scrolled away
    bool applyingShift_ = false;
    BlockStyler* styler_ = nullptr;
    std::unique_ptr<TableEmbedder> tables_;
    std::unique_ptr<PropertiesEmbedder> properties_;
    double padding_ = 0;
    double maxWidth_ = 0;
    quint64 editCount_ = 0;
};

}  // namespace hitokoto::ui
