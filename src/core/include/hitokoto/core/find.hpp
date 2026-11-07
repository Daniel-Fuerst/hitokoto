// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find in the open file. The searched text is the concealed view of the document, split into
// units (lines, link destinations, table cells, properties of front matter) that map back to the
// source; the matcher compares with simple Unicode case folding (ADR 0008).
#pragma once

#include "hitokoto/core/markdown.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// Simple Unicode case folding: CaseFolding.txt status C and S, one code point to one (ADR 0008).
/// Code points without a mapping fold to themselves.
[[nodiscard]] char32_t foldCaseSimple(char32_t c) noexcept;
/// Version of the Unicode data the folding table was generated from, e.g. "18.0.0".
[[nodiscard]] std::string_view caseFoldingUnicodeVersion() noexcept;
/// The Unicode property White_Space (S028, "Whitespace").
[[nodiscard]] bool isUnicodeWhitespace(char32_t c) noexcept;

/// A position in the document: the index of a line and a UTF-16 column in it.
struct DocPos {
    std::size_t line = 0;
    std::uint32_t column = 0;
    friend auto operator<=>(const DocPos&, const DocPos&) = default;
};

/// What a unit of searched text is (S028, "Units").
enum class FindUnitKind : std::uint8_t { Line, Destination, Cell, Property };

/// How a unit is separated from the unit before it (S028, "Units"): a soft boundary counts as a
/// whitespace run and a line boundary, a hard one only as a line boundary; no match crosses it.
enum class FindBoundary : std::uint8_t { Soft, Hard };

/// A run of code units of a unit's text and the source range [source, sourceEnd) it came from. A
/// run is one to one if its source lies on one line and has its length; otherwise it is atomic (an
/// escape, `<br>`, a folded line break): a match that covers part of it covers all of its source.
struct FindSegment {
    std::uint32_t textStart = 0;
    std::uint32_t textLength = 0;
    DocPos source;     ///< where the run starts in the document
    DocPos sourceEnd;  ///< where it ends (exclusive)
    [[nodiscard]] bool oneToOne() const noexcept {
        return source.line == sourceEnd.line && sourceEnd.column - source.column == textLength;
    }
    friend bool operator==(const FindSegment&, const FindSegment&) = default;
};

/// One unit of the searched text with its map back to the source.
struct FindUnit {
    FindUnitKind kind = FindUnitKind::Line;
    FindBoundary before = FindBoundary::Soft;
    std::u16string text;
    std::vector<FindSegment> segments;  ///< cover `text` in order, without gaps
    /// Destination units: the link text, highlighted and selected for a match in the destination.
    DocPos linkStart, linkEnd;
    friend bool operator==(const FindUnit&, const FindUnit&) = default;
};

/// One match: the source range to highlight and select. For a match in a link destination it is
/// the link text (S028 decision 2); `inDestination` tells them apart.
struct FindMatch {
    DocPos start;
    DocPos end;  ///< exclusive
    bool inDestination = false;
    /// Where the match lies in the units it was found in: [unit, textStart] to [endUnit, textEnd).
    std::size_t unit = 0;
    std::uint32_t textStart = 0;
    std::size_t endUnit = 0;
    std::uint32_t textEnd = 0;
    friend bool operator==(const FindMatch&, const FindMatch&) = default;
};

struct FindOptions {
    bool matchCase = false;  ///< decision 6; off = compare after foldCaseSimple
};

/// Options that decide the units of a document.
struct FindDocumentOptions {
    bool frontMatterAsProperties = true;  ///< `Options::frontMatterAsProperties` of the UI (S024)
};

/// A compiled search text (S028, "Search text").
class FindQuery {
public:
    FindQuery() = default;
    FindQuery(std::u16string_view text, FindOptions options);

    /// Empty or only whitespace: finds nothing.
    [[nodiscard]] bool findsNothing() const noexcept { return words_.empty(); }
    [[nodiscard]] const FindOptions& options() const noexcept { return options_; }
    /// The words between whitespace runs, as code points (folded unless matchCase).
    [[nodiscard]] const std::vector<std::u32string>& words() const noexcept { return words_; }
    [[nodiscard]] bool leadingWhitespace() const noexcept { return leading_; }
    [[nodiscard]] bool trailingWhitespace() const noexcept { return trailing_; }

private:
    FindOptions options_;
    std::vector<std::u32string> words_;
    bool leading_ = false;
    bool trailing_ = false;
};

/// The concealed text of one line outside tables and properties: the line with the hidden ranges
/// of `render` removed (revealed = false), followed by one Destination unit per link.
[[nodiscard]] std::vector<FindUnit> lineUnits(std::u16string_view line, std::size_t lineIndex,
                                              const LineRender& render);

/// The units of one table row (S028, "Table cells"): one Cell unit per cell up to `columns`
/// (the header's cell count), each followed by Destination units for its links. Only the inline
/// elements of `registry` run on a cell; `\|` reads as `|`, `<br>` as a line break.
[[nodiscard]] std::vector<FindUnit> tableRowUnits(const ElementRegistry& registry, std::u16string_view line,
                                                  std::size_t lineIndex, std::size_t columns);

/// The units of closed front matter shown as properties (S028, "Front matter"): one Property unit
/// per key, value, list or tags item and per key and scalar inside an unsupported value, mapped
/// back to the YAML lines. `yamlLines` are the lines between the delimiters; the first of them has
/// the index `firstLine` in the document. Empty if the YAML has an error.
[[nodiscard]] std::vector<FindUnit> propertyUnits(std::span<const std::u16string> yamlLines, std::size_t firstLine);

/// All units of a document in order, with their boundaries (the reference for the UI).
[[nodiscard]] std::vector<FindUnit> documentUnits(const ElementRegistry& registry,
                                                  std::span<const std::u16string> lines,
                                                  const FindDocumentOptions& options = {});

/// Every match of `query` in `units`, in document order (a destination match stands at the start
/// of its link text), without overlaps.
[[nodiscard]] std::vector<FindMatch> findInUnits(std::span<const FindUnit> units, const FindQuery& query);

/// `findInUnits(documentUnits(...), FindQuery(text, options))`.
[[nodiscard]] std::vector<FindMatch> findInDocument(const ElementRegistry& registry,
                                                    std::span<const std::u16string> lines, std::u16string_view text,
                                                    FindOptions options = {},
                                                    const FindDocumentOptions& documentOptions = {});

/// The concealed text around a match for a screen reader (S028 review O11): at most `maxLength`
/// code units of the unit text around it, with "…" where text was cut.
[[nodiscard]] std::u16string matchContext(std::span<const FindUnit> units, const FindMatch& match,
                                          std::size_t maxLength = 80);

}  // namespace hitokoto::core
