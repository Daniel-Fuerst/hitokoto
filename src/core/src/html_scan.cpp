// SPDX-License-Identifier: GPL-3.0-or-later
// S031: one linear scan of a paste's HTML before Qt's import. It reads the HTML as Qt's HTML
// parser (QTextHtmlParser, the same in Qt 6.8.3 and 6.12.0) reads it, finds the tables, and
// estimates from what it counts how long the import would take; see kPasteHtmlBounds. The same
// reading takes what only formats out of the HTML before the import (htmlWithoutFormatting).
#include "hitokoto/core/find.hpp"
#include "hitokoto/core/paste.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace hitokoto::core {

namespace {

constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();

std::uint64_t add(std::uint64_t a, std::uint64_t b) noexcept {
    return a > kMax - b ? kMax : a + b;
}

std::uint64_t mul(std::uint64_t a, std::uint64_t b) noexcept {
    return a != 0 && b > kMax / a ? kMax : a * b;
}

/// QChar::isSpace, which Qt's parser uses between words.
bool isSpace(char16_t c) noexcept {
    return isUnicodeWhitespace(c);
}

char16_t asciiLower(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + (u'a' - u'A')) : c;
}

/// Whether `name` is `lower` (ASCII letters and digits) without regard to ASCII case. Qt lowers
/// names with Unicode rules; the only non-ASCII letter that lowers to an ASCII one is the Kelvin
/// sign, and no name compared here holds a `k` except "blockquote", for which a miss is safe.
bool is(std::u16string_view name, std::u16string_view lower) noexcept {
    if (name.size() != lower.size()) {
        return false;
    }
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (asciiLower(name[i]) != lower[i]) {
            return false;
        }
    }
    return true;
}

/// A hash of a name for finding the element an end tag closes (lower ASCII, FNV-1a).
std::uint64_t hashOf(std::u16string_view name) noexcept {
    std::uint64_t h = 14695981039346656037ULL;
    for (const char16_t c : name) {
        h = (h ^ asciiLower(c)) * 1099511628211ULL;
    }
    return h;
}

/// What an element is to Qt's parser and to the estimate.
enum class Kind : std::uint8_t {
    Other,      // inline and unknown elements
    Block,      // a block of its own (DisplayBlock in Qt's element table)
    Cell,       // td, th
    Row,        // tr
    Table,      // table
    Section,    // thead, tbody, tfoot (allowed only in a table)
    Caption,    // caption (allowed only in a table)
    Paragraph,  // p (closes a p it is directly in)
    ListItem,   // li (closes an li it is directly in)
    DefList,    // dl
    DefItem,    // dd, dt (allowed only in a dl)
    Body,       // body, qt (not in head)
    Head,       // head
    Void,       // img, br, hr, meta: Qt closes them at once
    Style,      // style: its text is a style sheet
};

struct Element {
    Kind kind = Kind::Other;
    bool image = false;  // img
    bool lineBreak = false;
    bool meta = false;  // meta: name and content mark Qt's own HTML, which Qt reads spaces in differently
};

Element classify(std::u16string_view n) noexcept {
    Element e;
    if (is(n, u"td") || is(n, u"th")) {
        e.kind = Kind::Cell;
    } else if (is(n, u"tr")) {
        e.kind = Kind::Row;
    } else if (is(n, u"table")) {
        e.kind = Kind::Table;
    } else if (is(n, u"thead") || is(n, u"tbody") || is(n, u"tfoot")) {
        e.kind = Kind::Section;
    } else if (is(n, u"caption")) {
        e.kind = Kind::Caption;
    } else if (is(n, u"p")) {
        e.kind = Kind::Paragraph;
    } else if (is(n, u"li")) {
        e.kind = Kind::ListItem;
    } else if (is(n, u"dl")) {
        e.kind = Kind::DefList;
    } else if (is(n, u"dd") || is(n, u"dt")) {
        e.kind = Kind::DefItem;
    } else if (is(n, u"body") || is(n, u"qt")) {
        e.kind = Kind::Body;
    } else if (is(n, u"head")) {
        e.kind = Kind::Head;
    } else if (is(n, u"img")) {
        e.kind = Kind::Void;
        e.image = true;
    } else if (is(n, u"br")) {
        e.kind = Kind::Void;
        e.lineBreak = true;
    } else if (is(n, u"hr") || is(n, u"meta")) {
        e.kind = Kind::Void;
        e.meta = is(n, u"meta");
    } else if (is(n, u"style")) {
        e.kind = Kind::Style;
    } else if (is(n, u"div") || is(n, u"blockquote") || is(n, u"center") || is(n, u"ul") || is(n, u"ol") ||
               is(n, u"pre") || is(n, u"h1") || is(n, u"h2") || is(n, u"h3") || is(n, u"h4") || is(n, u"h5") ||
               is(n, u"h6")) {
        e.kind = Kind::Block;
    }
    return e;
}

/// Whether an element of `kind` starts a block in Qt's import, so that an image after it is in a
/// new block. Without style sheets and style attributes an element's display is its own.
bool startsBlock(Kind kind) noexcept {
    switch (kind) {
        case Kind::Cell:
        case Kind::Row:
        case Kind::Table:
        case Kind::Section:
        case Kind::Caption:
        case Kind::Block:
        case Kind::Paragraph:
        case Kind::ListItem:
        case Kind::DefList:
        case Kind::DefItem:
        case Kind::Body:
            return true;
        default:
            return false;
    }
}

/// Whether an attribute `key` of element `e` stays in the HTML Qt imports: the spans of a cell, and
/// the mark of Qt's own HTML. Qt lowers keys with Unicode rules; none of these holds a `k`, the only
/// letter outside ASCII that lowers to an ASCII one (the Kelvin sign).
bool keeps(const Element& e, std::u16string_view key) noexcept {
    return (e.kind == Kind::Cell && (is(key, u"colspan") || is(key, u"rowspan"))) ||
           (e.meta && (is(key, u"name") || is(key, u"content")));
}

/// One open element, as Qt's parser keeps it as the parent of what follows.
struct Open {
    std::uint64_t hash = 0;
    std::u16string_view name;  // empty for the table and row Qt inserts itself
    Kind kind = Kind::Other;
};

/// The grid Qt builds for one table: rows, and per row the columns its cells and the row spans of
/// the rows above take (QTextHtmlImporter::scanTable). An upper bound: every row span still open
/// adds its columns to a row, also where Qt does not need them.
class Grid {
public:
    void startRow() noexcept {
        endRow();
        open_ = true;
        const std::uint64_t row = rows_;
        rows_ = add(rows_, 1);
        carry_ = add(carry_, next_);
        next_ = 0;
        while (!spans_.empty() && spans_.front().first <= row) {
            std::pop_heap(spans_.begin(), spans_.end(), std::greater<>());
            carry_ -= spans_.back().second;
            spans_.pop_back();
        }
        width_ = carry_;
        covered_ = add(covered_, carry_);
    }

    /// A cell spanning `columns` (1 to 20,480, as Qt bounds it) and `rows` (at least 1).
    bool addCell(std::uint64_t columns, std::uint64_t rows) noexcept {
        if (!open_) {
            startRow();
        }
        cells_ = add(cells_, 1);
        width_ = add(width_, columns);
        covered_ = add(covered_, columns - 1);
        if (rows > 1) {
            next_ = add(next_, columns);
            try {
                spans_.emplace_back(add(rows_ - 1, rows), columns);
            } catch (const std::bad_alloc&) {
                return false;
            }
            std::push_heap(spans_.begin(), spans_.end(), std::greater<>());
        }
        return true;
    }

    /// The table ends: its positions and what spans merge, then empty for the next table.
    void finish(std::uint64_t& positionsWithoutCell, std::uint64_t& mergeWork) noexcept {
        endRow();
        const std::uint64_t positions = mul(rows_, columns_);
        positionsWithoutCell = add(positionsWithoutCell, positions > cells_ ? positions - cells_ : 0);
        mergeWork = add(mergeWork, mul(covered_, positions));
        *this = Grid{};
    }

private:
    void endRow() noexcept {
        if (open_) {
            columns_ = std::max(columns_, width_);
            open_ = false;
        }
    }

    std::uint64_t rows_ = 0;
    std::uint64_t columns_ = 0;
    std::uint64_t cells_ = 0;
    std::uint64_t width_ = 0;
    std::uint64_t carry_ = 0;    // columns of row spans from the rows above
    std::uint64_t next_ = 0;     // columns of row spans that start in this row
    std::uint64_t covered_ = 0;  // positions covered by a span
    bool open_ = false;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans_;  // (row it ends before, columns)
};

/// The value of a span attribute as a count: Qt reads it with QString::toInt and ignores it if
/// that fails. Read here as the first digits, which is never less than what Qt takes; a value
/// with an entity counts as `most`.
std::uint64_t spanValue(std::u16string_view value, std::uint64_t most) noexcept {
    if (value.find(u'&') != std::u16string_view::npos) {
        return most;
    }
    std::size_t i = 0;
    while (i < value.size() && (isSpace(value[i]) || value[i] == u'+')) {
        ++i;
    }
    if (i < value.size() && value[i] == u'-') {
        return 1;
    }
    std::uint64_t v = 0;
    bool any = false;
    while (i < value.size() && value[i] >= u'0' && value[i] <= u'9') {
        v = std::min(most, v * 10 + static_cast<std::uint64_t>(value[i] - u'0'));
        any = true;
        ++i;
    }
    return any ? std::clamp<std::uint64_t>(v, 1, most) : 1;
}

class Scanner {
public:
    /// With `out`, the scan also writes the HTML without what only formats there, which is never
    /// longer than `html` (htmlWithoutFormatting); without it, what only formats is beyond the
    /// estimate.
    explicit Scanner(std::u16string_view html, char16_t* out = nullptr) : h_(html), out_(out) {}

    /// Runs the scan and writes what stays; how many characters that is.
    std::size_t rewrite() noexcept {
        run();
        cut(h_.size(), h_.size());
        return written_;
    }

    HtmlScan run() noexcept {
        const PasteHtmlCosts& c = kPasteHtmlBounds.costs;
        while (pos_ < h_.size()) {
            const std::size_t lt = h_.find(u'<', pos_);
            const std::size_t end = lt == std::u16string_view::npos ? h_.size() : lt;
            text(pos_, end);
            if (lt == std::u16string_view::npos) {
                break;
            }
            pos_ = lt + 1;
            eatSpace();
            if (at(u'!')) {
                exclamation();
                charge(c.endTag);
            } else if (at(u'/')) {
                ++pos_;
                endTag();
            } else if (!startTag()) {
                break;
            }
            if (result_.tables == HtmlTables::Nested || beyond_) {
                break;
            }
        }
        if (!beyond_ && result_.tables != HtmlTables::Nested) {
            grid_.finish(positionsWithoutCell_, mergeWork_);
            charge(mul(positionsWithoutCell_, c.positionWithoutCell));
            charge(mul(mergeWork_, c.mergeStep));
        }
        result_.allowedPs = std::max(kPasteHtmlBounds.floorPs, mul(result_.cells, kPasteHtmlBounds.perCellPs));
        const bool over = beyond_ || result_.importPs > result_.allowedPs;
        if ((result_.tables == HtmlTables::Flat && over) || (result_.tables == HtmlTables::None && beyond_)) {
            result_.tables = HtmlTables::OverBudget;
        }
        return result_;
    }

private:
    bool at(char16_t c) const noexcept { return pos_ < h_.size() && h_[pos_] == c; }

    void eatSpace() noexcept {
        while (pos_ < h_.size() && isSpace(h_[pos_]) && h_[pos_] != u'\u2029') {
            ++pos_;
        }
    }

    void charge(std::uint64_t ps) noexcept { result_.importPs = add(result_.importPs, ps); }

    /// Leaves `html[from, to)` out of what is written; `from` is never before an earlier cut's end.
    void cut(std::size_t from, std::size_t to) noexcept {
        if (out_ == nullptr) {
            return;
        }
        const auto first = h_.begin() + static_cast<std::ptrdiff_t>(copied_);
        std::copy(first, h_.begin() + static_cast<std::ptrdiff_t>(from), out_ + written_);
        written_ += from - copied_;
        copied_ = to;
    }

    /// Text between tags: it adds to the block an image lands in. In a style element, up to the
    /// next tag (comments do not end it), it is the style sheet Qt reads at the next end tag: it
    /// leaves, and a sheet the scan meets is beyond the estimate.
    void text(std::size_t from, std::size_t to) noexcept {
        block_ = add(block_, to - from);
        if (inStyle_ && from < to) {
            if (out_ != nullptr) {
                cut(from, to);
            } else {
                beyond_ = true;
            }
        }
    }

    /// `<!`: a comment to `-->` or the end, or a declaration to `>` (parseExclamationTag).
    void exclamation() noexcept {
        ++pos_;
        if (h_.substr(pos_, 2) == u"--") {
            const std::size_t end = h_.find(u"-->", pos_ + 2);
            pos_ = end == std::u16string_view::npos ? h_.size() : end + 3;
        } else {
            const std::size_t end = h_.find(u'>', pos_);
            pos_ = end == std::u16string_view::npos ? h_.size() : end + 1;
        }
    }

    /// A word as Qt's parseWord reads it: quoted with `"` or `'` (a `'` after a backslash does not
    /// end it), or up to `>`, `/>`, `<`, `=` or a space. Entities never move its end: what an entity
    /// reads that Qt would decode is letters, digits, `#` and `;`.
    std::u16string_view word() noexcept {
        if (at(u'"')) {
            const std::size_t start = ++pos_;
            const std::size_t end = h_.find(u'"', start);
            pos_ = end == std::u16string_view::npos ? h_.size() : end + 1;
            return h_.substr(start, (end == std::u16string_view::npos ? h_.size() : end) - start);
        }
        if (at(u'\'')) {
            const std::size_t start = ++pos_;
            while (pos_ < h_.size()) {
                const char16_t ch = h_[pos_++];
                if (ch == u'\'' && h_.size() > 1 && h_[pos_ - 2] != u'\\') {
                    return h_.substr(start, pos_ - 1 - start);
                }
            }
            return h_.substr(start);
        }
        const std::size_t start = pos_;
        while (pos_ < h_.size()) {
            const char16_t ch = h_[pos_];
            if (ch == u'>' || (ch == u'/' && pos_ + 1 < h_.size() && h_[pos_ + 1] == u'>') || ch == u'<' ||
                ch == u'=' || isSpace(ch)) {
                break;
            }
            ++pos_;
        }
        return h_.substr(start, pos_ - start);
    }

    /// Past the `>` that ends a tag; whether a `/` came before it (Qt closes the element at once).
    bool finishTag() noexcept {
        bool closed = false;
        while (pos_ < h_.size() && h_[pos_] != u'>') {
            closed = closed || h_[pos_] == u'/';
            ++pos_;
        }
        if (pos_ < h_.size()) {
            ++pos_;
        }
        return closed;
    }

    std::size_t depth() const noexcept { return depth_; }

    void push(std::u16string_view name, Kind kind) noexcept {
        if (depth_ == stack_.size()) {
            beyond_ = true;  // deeper than kMaxDepth
            return;
        }
        stack_[depth_++] = Open{name.empty() ? 0 : hashOf(name), name, kind};
        if (kind == Kind::Table) {
            ++tables_;
        } else if (kind == Kind::Row) {
            ++rows_;
        } else if (kind == Kind::DefList) {
            ++defLists_;
        }
    }

    void pop() noexcept {
        const Open& top = stack_[--depth_];
        if (top.kind == Kind::Table) {
            --tables_;
            grid_.finish(positionsWithoutCell_, mergeWork_);
        } else if (top.kind == Kind::Row) {
            --rows_;
        } else if (top.kind == Kind::DefList) {
            --defLists_;
        }
    }

    /// Pops the elements above the nearest one `allowed` accepts; all of them if none does
    /// (resolveParent attaches the element to the document's root then).
    template <typename Allowed>
    void popTo(Allowed allowed) noexcept {
        while (depth_ > 0 && !allowed(stack_[depth_ - 1].kind)) {
            pop();
        }
    }

    void endTag() noexcept {
        const PasteHtmlCosts& c = kPasteHtmlBounds.costs;
        inStyle_ = false;
        std::u16string_view name = word();
        finishTag();
        charge(add(c.endTag, mul(depth(), c.endDepthStep)));
        while (!name.empty() && isSpace(name.front())) {
            name.remove_prefix(1);
        }
        while (!name.empty() && isSpace(name.back())) {
            name.remove_suffix(1);
        }
        if (name.empty() || name.find(u'&') != std::u16string_view::npos) {
            return;  // matches nothing that is sure: nothing closes, which overstates the depth
        }
        // The nearest open element of that name closes with all inside it (parseCloseTag). Only
        // the first element whose hash matches is compared; a different name there closes nothing.
        const std::uint64_t hash = hashOf(name);
        for (std::size_t i = depth_; i > 0; --i) {
            const Open& open = stack_[i - 1];
            const std::u16string_view openName =
                !open.name.empty() ? open.name : (open.kind == Kind::Table ? u"table" : u"tr");
            if ((open.name.empty() ? hashOf(openName) : open.hash) != hash) {
                continue;
            }
            if (is(openName, lowerAscii(name))) {
                while (depth_ >= i) {
                    pop();
                }
            }
            return;
        }
    }

    /// `name` as lower ASCII for `is`; names longer than the buffer never match.
    std::u16string_view lowerAscii(std::u16string_view name) noexcept {
        if (name.size() > lower_.size()) {
            return u"\x1f";
        }
        for (std::size_t i = 0; i < name.size(); ++i) {
            lower_[i] = asciiLower(name[i]);
        }
        return {lower_.data(), name.size()};
    }

    bool startTag() noexcept {
        const PasteHtmlCosts& c = kPasteHtmlBounds.costs;
        inStyle_ = false;
        const std::u16string_view name = word();
        if (name.find(u'&') != std::u16string_view::npos) {
            beyond_ = true;  // an entity in a tag name: no HTML that a program writes
            return false;
        }
        const Element e = classify(name);

        // The element's own cost and its depth.
        switch (e.kind) {
            case Kind::Cell:
                charge(c.cellTag);
                break;
            case Kind::Row:
                charge(c.rowTag);
                break;
            case Kind::Table:
                charge(c.tableTag);
                break;
            case Kind::Other:
            case Kind::Style:
            case Kind::Head:
                charge(name.empty() ? c.endTag : c.otherTag);
                break;
            default:
                charge(c.blockTag);
                break;
        }
        charge(mul(depth(), c.depthStep));

        // Attributes as Qt's parseAttributes reads them, after a space only.
        std::uint64_t columns = 1;
        std::uint64_t rows = 1;
        if (pos_ < h_.size() && isSpace(h_[pos_])) {
            if (!attributes(e, columns, rows)) {
                return false;
            }
        }
        const bool closed = finishTag();
        if (e.kind == Kind::Style && !closed) {
            inStyle_ = true;
        }

        // Where Qt attaches the element (resolveParent), and the table it belongs to.
        switch (e.kind) {
            case Kind::Cell:
                if (rows_ == 0) {
                    if (tables_ > 0) {
                        result_.tables = HtmlTables::Nested;  // Qt inserts a table inside the table
                        return true;
                    }
                    push({}, Kind::Table);
                    push({}, Kind::Row);
                    grid_.startRow();
                } else {
                    popTo([](Kind k) { return k == Kind::Row; });
                }
                result_.cells = add(result_.cells, 1);
                if (!grid_.addCell(columns, rows)) {
                    beyond_ = true;
                    return false;
                }
                break;
            case Kind::Row:
                if (tables_ == 0) {
                    push({}, Kind::Table);
                }
                popTo([](Kind k) { return k == Kind::Table || k == Kind::Section; });
                grid_.startRow();
                break;
            case Kind::Table:
                if (tables_ > 0) {
                    result_.tables = HtmlTables::Nested;
                    return true;
                }
                if (result_.tables == HtmlTables::None) {
                    result_.tables = HtmlTables::Flat;
                }
                break;
            case Kind::Section:
            case Kind::Caption:
                popTo([](Kind k) { return k == Kind::Table; });
                break;
            case Kind::DefItem:
                popTo([](Kind k) { return k == Kind::DefList; });
                break;
            case Kind::Body:
                while (depth_ > 0 && stack_[depth_ - 1].kind == Kind::Head) {
                    pop();
                }
                break;
            case Kind::Paragraph:
            case Kind::ListItem:
                if (depth_ > 0 && stack_[depth_ - 1].kind == e.kind) {
                    pop();  // not nested in itself
                }
                break;
            default:
                break;
        }
        if (startsBlock(e.kind)) {
            block_ = 0;
        }
        if (e.image) {
            charge(mul(block_, c.imageStep));
        }
        if (e.image || e.lineBreak) {
            block_ = add(block_, 1);
        }
        if (e.kind != Kind::Void && !closed && !name.empty()) {
            push(name, e.kind);
        } else if (e.kind == Kind::Table) {
            grid_.finish(positionsWithoutCell_, mergeWork_);  // `<table/>`
        }
        return !beyond_;
    }

    /// The attributes of a start tag (parseAttributes): their characters and the span of a cell.
    /// When writing, an attribute that only formats leaves with the blanks before it, up to one
    /// where a word follows, so that Qt reads the rest of the tag as before.
    bool attributes(const Element& e, std::uint64_t& columns, std::uint64_t& rows) noexcept {
        const PasteHtmlCosts& c = kPasteHtmlBounds.costs;
        while (pos_ < h_.size()) {
            const std::size_t blank = pos_;
            eatSpace();
            if (at(u'>') || at(u'/')) {
                break;
            }
            const std::size_t keyStart = pos_;
            const std::u16string_view key = word();
            if (key.empty()) {
                break;
            }
            if (key.find(u'&') != std::u16string_view::npos) {
                beyond_ = true;  // an entity in an attribute name: no HTML that a program writes
                return false;
            }
            std::size_t end = pos_;
            eatSpace();
            std::u16string_view value = u"1";
            if (at(u'=')) {
                ++pos_;
                eatSpace();
                value = word();
                end = pos_;
            }
            if (!keeps(e, key)) {
                const bool wordFollows = end < h_.size() && !isSpace(h_[end]) && h_[end] != u'>';
                cut(wordFollows && keyStart > blank ? blank + 1 : blank, end);
            }
            charge(mul(end - keyStart, c.attributeChar));
            if (value.empty()) {
                continue;
            }
            if (e.kind == Kind::Cell) {
                if (is(key, u"colspan")) {
                    columns = std::max(columns, spanValue(value, 20480));
                } else if (is(key, u"rowspan")) {
                    rows = std::max(rows, spanValue(value, std::numeric_limits<std::uint32_t>::max()));
                }
            }
        }
        return true;
    }

    std::u16string_view h_;
    char16_t* out_ = nullptr;
    std::size_t copied_ = 0;   // h_[0, copied_) is handled: written or cut
    std::size_t written_ = 0;  // characters written to out_
    std::size_t pos_ = 0;
    HtmlScan result_;
    bool beyond_ = false;
    std::array<Open, kPasteHtmlBounds.maxDepth> stack_{};
    std::size_t depth_ = 0;
    std::size_t tables_ = 0;  // open tables (at most one, a second one is nested)
    std::size_t rows_ = 0;
    std::size_t defLists_ = 0;
    std::array<char16_t, 64> lower_{};
    Grid grid_;
    std::uint64_t positionsWithoutCell_ = 0;
    std::uint64_t mergeWork_ = 0;
    std::uint64_t block_ = 0;  // characters in the block an image would land in
    bool inStyle_ = false;     // in the text of a style element
};

}  // namespace

HtmlScan scanHtml(std::u16string_view html) noexcept {
    return Scanner(html).run();
}

HtmlTables scanHtmlTables(std::u16string_view html) noexcept {
    return scanHtml(html).tables;
}

std::u16string htmlWithoutFormatting(std::u16string_view html) {
    std::u16string out(html.size(), u'\0');
    const std::size_t written = Scanner(html, out.data()).rewrite();
    out.resize(written);
    return out;
}

}  // namespace hitokoto::core
