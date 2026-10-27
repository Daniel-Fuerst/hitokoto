// SPDX-License-Identifier: GPL-3.0-or-later
// S018 / ADR 0005: finds the tables in the editor's document and shows each as a TableWidget over
// the space its header line reserves in BlockLayout (ADR 0003); the other lines are hidden. Changes
// rows and columns in the Markdown lines and moves the caret into and out of tables.
#pragma once

#include <QObject>
#include <QPoint>
#include <QTextBlock>

#include <vector>

class QKeyEvent;
class QTableView;

namespace hitokoto::ui {

class BlockLayout;
class BlockStyler;
class EditorFormats;
class EditorWidget;
class TableWidget;

class TableEmbedder : public QObject {
    Q_OBJECT
public:
    TableEmbedder(EditorWidget* editor, BlockStyler* styler, BlockLayout* layout);
    ~TableEmbedder() override;

    [[nodiscard]] QTableView* viewAt(int blockNumber) const;
    [[nodiscard]] bool isSourceShown(int blockNumber) const;

    void setFormats(const EditorFormats* formats);
    void setAvailableWidth(int width);
    /// Finds the tables on and near the screen and brings their widgets up to date.
    void refresh();
    /// refresh() once the event loop runs again (after document changes).
    void scheduleRefresh();
    /// A change to the document: typing in the editor keeps a table that the typed line belongs
    /// to as Markdown lines until the caret leaves that line (S018, "typing a table").
    void documentChanged(int position, int charsRemoved, int charsAdded);

    /// Before text goes into the blank line directly below a table (keys, input method, paste):
    /// adds a blank line, so the text becomes a paragraph and not a row of the table (S018,
    /// "writing below a table").
    void beforeWritingBelow();

    /// Marks changes as typed in the editor (keys, input method, paste) while it exists; loading
    /// a file or the table view's own changes are not typing.
    class Typing {
    public:
        explicit Typing(TableEmbedder* embedder) : embedder_(embedder) { ++embedder_->typingDepth_; }
        ~Typing() { --embedder_->typingDepth_; }
        Typing(const Typing&) = delete;
        Typing& operator=(const Typing&) = delete;

    private:
        TableEmbedder* embedder_;
    };
    /// Moves the widgets to their lines (after scrolling or layout changes).
    void reposition();
    /// Up/Down from the lines next to a table enter it; Ctrl+Shift+M on a table line in the source
    /// view shows the table again. Returns true if the key was handled.
    bool handleEditorKey(QKeyEvent* event);
    /// A collapsed caret that lands on hidden table lines moves into the table view.
    void caretMoved();
    /// A click in the editor above a table that starts the document or below one that ends it
    /// adds a line there for the caret. Returns true if the click was handled.
    bool handleEditorClick(QPoint position);

private:
    struct Table {
        QTextBlock header;
        QTextBlock last;
        bool source = false;
        bool held = false;              ///< shows its lines while the caret is in a line typed in the editor
        TableWidget* widget = nullptr;  ///< owned by the table; deleted in release()
        quint64 signature = 0;          ///< of the lines and width the widget was fitted to
        /// A change removed one of its lines (or the line break before it): its QTextBlocks may
        /// no longer exist, so nothing touches them again; refresh() drops the table.
        bool stale = false;
        /// Positions of the line breaks before and in the table, kept up to date with each
        /// change: removing one of them makes the table stale.
        std::vector<int> breaks;
    };

    [[nodiscard]] Table* tableAt(int blockNumber);
    [[nodiscard]] const Table* tableAt(int blockNumber) const;
    [[nodiscard]] Table* tableOf(const TableWidget* widget);
    [[nodiscard]] QTextBlock lastLineOf(const QTextBlock& header) const;
    [[nodiscard]] quint64 signatureOf(const Table& table) const;
    void apply(Table& table);
    void release(Table& table);
    /// Forgets a stale table without touching its lines.
    void drop(Table& table);
    /// Records where the table's line breaks are (after it was found or changed).
    void markBreaks(Table& table) const;
    /// Shows lines in [from, to] that are hidden or reserve height but belong to no table.
    void sweep(int from, int to);
    TableWidget* createWidget();
    void focusCell(Table& table, int row, int column, bool atEnd = false);

    // Changes to the Markdown lines; each is one undo step.
    void appendRow(Table& table, int column);
    void deleteRow(Table& table, int row, bool preferPrevious);
    void addColumn(Table& table);
    void deleteColumn(Table& table, int column);
    void removeLastRowAndLeave(Table& table);
    /// To the line above (`atEnd`: its end), adding one if the table starts the document.
    void leaveAbove(Table& table, bool atEnd);
    /// To the line below, if any.
    void leaveDown(Table& table);
    /// Below the table, adding lines at the end of the document (see the .cpp).
    void leaveBelow(Table& table);
    /// From the table view: `rowOrBlock`/`columnOrPosition` are the current cell; from the source
    /// view: the caret's block number and position in its line.
    void toggleSource(Table& table, int rowOrBlock, int columnOrPosition);
    /// Re-reads one table right after its lines changed, so the view can go on at once.
    void update(Table& table);

    EditorWidget* editor_;
    BlockStyler* styler_;
    BlockLayout* layout_;
    const EditorFormats* formats_ = nullptr;
    int availableWidth_ = 400;
    std::vector<Table> tables_;
    int typingDepth_ = 0;
    bool leaving_ = false;  ///< moving the caret out of a table: caretMoved() lets it
    QTextBlock typing_;     ///< the line the editor's caret types in; invalid once it leaves
    bool refreshPending_ = false;
    bool refreshing_ = false;
};

}  // namespace hitokoto::ui
