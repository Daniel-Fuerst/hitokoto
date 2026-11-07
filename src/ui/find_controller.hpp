// SPDX-License-Identifier: GPL-3.0-or-later
// S028: drives the search for the find bar: the units of the concealed view in slices on the UI
// thread (never blocking), the current match, the count, the highlights, and the source view of
// tables and properties that hold the current match (review O8). It never changes the document.
#pragma once

#include "hitokoto/core/find.hpp"
#include "hitokoto/core/theme.hpp"

#include <QObject>
#include <QString>
#include <QTextLayout>

#include <optional>
#include <vector>

namespace hitokoto::ui {

class EditorWidget;
class FindBar;

class FindController : public QObject {
    Q_OBJECT
public:
    FindController(EditorWidget* editor, FindBar* bar, const core::ElementRegistry& registry,
                   QObject* parent = nullptr);
    ~FindController() override;
    FindController(const FindController&) = delete;
    FindController& operator=(const FindController&) = delete;

    void applyTheme(const core::Theme& theme);

    /// Ctrl+F: opens the bar (or brings the focus back to its field) and selects the search text.
    void open();
    /// Escape: closes the bar; with a current match, selects it in the editor (decision 9).
    void close();
    void next();
    void previous();

    [[nodiscard]] bool isOpen() const;
    /// Whether the whole document has been searched for the current text.
    [[nodiscard]] bool isComplete() const;
    /// The current match, if any.
    [[nodiscard]] std::optional<core::FindMatch> currentMatch() const;
    /// 1-based index of the current match once the count is complete, else 0.
    [[nodiscard]] int currentIndex() const;
    /// Matches found so far (all of them once complete).
    [[nodiscard]] int matchCount() const;
    /// The text of the count: "n of m", "m+ matches" or "No matches" (empty for no search text).
    [[nodiscard]] QString countText() const;
    [[nodiscard]] bool matchCase() const;
    [[nodiscard]] bool highlightAll() const;
    /// The matches whose highlights are on screen (only the current one unless "Highlight all").
    [[nodiscard]] std::vector<core::FindMatch> visibleMatches() const;
    /// The highlight formats on a block (tests: their text colour must never cover concealed text).
    [[nodiscard]] QList<QTextLayout::FormatRange> highlightFormats(int blockNumber) const;
    /// The accessible announcement made for the current match last (review O11).
    [[nodiscard]] QString lastAnnouncement() const;

signals:
    /// The matches, the current match or the count changed.
    void resultsChanged();

private:
    EditorWidget* editor_;
    FindBar* bar_;
    const core::ElementRegistry& registry_;
};

}  // namespace hitokoto::ui
