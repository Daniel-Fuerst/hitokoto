// SPDX-License-Identifier: GPL-3.0-or-later
// S045: laying out the tree of math_parse.cpp. The rules are TeX's (The TeXbook, appendix G),
// with the distances taken from the font's MATH table the way OpenType math and MathML Core
// define them. Boxes are measured in pixels with y upwards inside this file; the result is turned
// to y downwards for the UI at the end.
#include "math_parse.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace hitokoto::core {
namespace {

using math::AtomClass;
using math::Node;

/// TeX's styles: display, text, script, scriptscript; cramped styles raise superscripts less.
struct Style {
    int level = 1;
    bool cramped = false;
};

Style superscriptStyle(Style s) {
    return {s.level <= 1 ? 2 : 3, s.cramped};
}
Style subscriptStyle(Style s) {
    return {s.level <= 1 ? 2 : 3, true};
}
Style numeratorStyle(Style s) {
    return {s.level == 0 ? 1 : s.level == 1 ? 2 : 3, s.cramped};
}
Style denominatorStyle(Style s) {
    return {s.level == 0 ? 1 : s.level == 1 ? 2 : 3, true};
}

struct Box {
    double width = 0;
    double height = 0;
    double depth = 0;
    double italic = 0;
    std::vector<MathGlyph> glyphs;  // y upwards here
    std::vector<MathRule> rules;    // y = bottom edge, upwards here
};

void place(Box& into, const Box& part, double dx, double dy) {
    for (const MathGlyph& g : part.glyphs) {
        into.glyphs.push_back({g.glyph, g.x + dx, g.y + dy, g.size});
    }
    for (const MathRule& r : part.rules) {
        into.rules.push_back({r.x + dx, r.y + dy, r.width, r.height});
    }
}

/// The space between two atoms in TeX's table (The TeXbook, ch. 18): 0 none, 1 thin, 2 thin in
/// display and text style only, 3 medium in those styles, 4 thick in those styles.
constexpr std::array<std::array<std::uint8_t, 8>, 8> kSpacing{{
    {0, 1, 3, 4, 0, 0, 0, 2},  // Ord
    {1, 1, 0, 4, 0, 0, 0, 2},  // Op
    {3, 3, 0, 0, 3, 0, 0, 3},  // Bin
    {4, 4, 0, 0, 4, 0, 0, 4},  // Rel
    {0, 0, 0, 0, 0, 0, 0, 0},  // Open
    {0, 1, 3, 4, 0, 0, 0, 2},  // Close
    {2, 2, 0, 2, 2, 2, 2, 2},  // Punct
    {2, 1, 3, 4, 2, 0, 2, 2},  // Inner
}};

constexpr double kNullDelimiterSpace = 0.12;  // TeX's \nulldelimiterspace, 1.2pt at 10pt, in em
constexpr int kMaxRepeats = 256;              // extenders in one assembly

std::string hexOf(char32_t c) {
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "U+%04X", static_cast<unsigned>(c));
    return buffer.data();
}

class Layout {
public:
    Layout(const MathFont& font, const MathTable& table, double pixelSize)
        : font_(font), table_(table), c_(table.constants), pixelSize_(pixelSize) {
        const double upem = font.unitsPerEm();
        upem_ = upem > 0 ? upem : 1000;
        script_ = c_.scriptPercentScaleDown > 0 ? c_.scriptPercentScaleDown / 100 : 0.7;
        scriptScript_ = c_.scriptScriptPercentScaleDown > 0 ? c_.scriptScriptPercentScaleDown / 100 : 0.5;
    }

    Box node(const Node& n, Style s) {
        if (error_) {
            return {};
        }
        switch (n.kind) {
            case Node::Kind::Null:
                return {};
            case Node::Kind::Symbol:
                return symbol(n, s);
            case Node::Kind::Group:
                return list(n.children, s);
            case Node::Kind::Scripts:
                return scripts(n, s);
            case Node::Kind::Fraction:
                return fraction(n, s);
            case Node::Kind::Radical:
                return radical(n, s);
            case Node::Kind::Delimited:
                return delimited(n.left, node(n.children[0], s), n.right, s);
            case Node::Kind::LargeOp:
                return largeOp(n, s);
            case Node::Kind::Text:
                return text(n.text, s);
            case Node::Kind::Space: {
                Box b;
                b.width = n.amount * px(s);
                return b;
            }
            case Node::Kind::Array:
                return array(n, s);
            case Node::Kind::XArrow:
                return xarrow(n, s);
            case Node::Kind::Raise: {
                const Style inner = n.scale < 1 ? Style{std::max(s.level, 2), s.cramped} : s;
                Box b = node(n.children[0], inner);
                Box out;
                const double dy = n.amount * px(s);
                place(out, b, 0, dy);
                out.width = b.width;
                out.height = std::max(0.0, b.height + dy);
                out.depth = std::max(0.0, b.depth - dy);
                return out;
            }
        }
        return {};
    }

    [[nodiscard]] const std::optional<MathError>& error() const noexcept { return error_; }

private:
    const MathFont& font_;
    const MathTable& table_;
    const MathConstants& c_;
    double pixelSize_;
    double upem_ = 1000;
    double script_ = 0.7;
    double scriptScript_ = 0.5;
    std::optional<MathError> error_;

    /// Pixels per em in a style.
    [[nodiscard]] double px(Style s) const {
        return pixelSize_ * (s.level <= 1 ? 1.0 : s.level == 2 ? script_ : scriptScript_);
    }
    /// Pixels per design unit in a style.
    [[nodiscard]] double unit(Style s) const { return px(s) / upem_; }
    [[nodiscard]] double k(double value, Style s) const { return value * unit(s); }

    void fail(std::string message) {
        if (!error_) {
            error_ = MathError{0, std::move(message)};
        }
    }

    Box glyphBox(std::uint16_t glyph, Style s) {
        Box b;
        const double u = unit(s);
        const GlyphBounds bounds = font_.bounds(glyph);
        b.width = font_.advance(glyph) * u;
        b.height = std::max(0.0, bounds.yMax * u);
        b.depth = std::max(0.0, -bounds.yMin * u);
        if (const auto it = table_.italicCorrections.find(glyph); it != table_.italicCorrections.end()) {
            b.italic = it->second * u;
        }
        b.glyphs.push_back({glyph, 0, 0, px(s)});
        return b;
    }

    std::uint16_t glyphFor(char32_t c, char32_t plain = 0) {
        std::uint16_t g = font_.glyph(c);
        if (g == 0 && plain != 0) {
            g = font_.glyph(plain);
        }
        if (g == 0) {
            fail("the math font has no glyph for " + hexOf(c));
        }
        return g;
    }

    Box symbol(const Node& n, Style s) {
        const std::uint16_t g = glyphFor(n.ch, n.plain);
        return g == 0 ? Box{} : glyphBox(g, s);
    }

    Box text(const std::u32string& text, Style s) {
        Box b;
        for (const char32_t c : text) {
            if (c == U' ') {
                const std::uint16_t space = font_.glyph(U' ');
                b.width += space != 0 ? font_.advance(space) * unit(s) : px(s) / 4;
                continue;
            }
            const std::uint16_t g = glyphFor(c);
            if (g == 0) {
                return {};
            }
            Box gb = glyphBox(g, s);
            place(b, gb, b.width, 0);
            b.width += gb.width;
            b.height = std::max(b.height, gb.height);
            b.depth = std::max(b.depth, gb.depth);
        }
        return b;
    }

    static AtomClass classOf(const Node& n) {
        switch (n.kind) {
            case Node::Kind::Symbol:
            case Node::Kind::Text:
            case Node::Kind::LargeOp:
            case Node::Kind::Scripts:
            case Node::Kind::XArrow:
            case Node::Kind::Delimited:
                return n.cls;
            case Node::Kind::Fraction:
                return AtomClass::Inner;
            default:
                return AtomClass::Ord;
        }
    }

    double spaceBetween(AtomClass left, AtomClass right, Style s) const {
        const std::uint8_t code = kSpacing[static_cast<std::size_t>(left)][static_cast<std::size_t>(right)];
        const double mu = px(s) / 18;
        const bool nonScript = s.level <= 1;
        switch (code) {
            case 1:
                return 3 * mu;
            case 2:
                return nonScript ? 3 * mu : 0;
            case 3:
                return nonScript ? 4 * mu : 0;
            case 4:
                return nonScript ? 5 * mu : 0;
            default:
                return 0;
        }
    }

    Box list(const std::vector<Node>& items, Style s) {
        // TeX's rules 5 and 6: a binary operator where no operand can stand before or after it
        // is an ordinary atom.
        std::vector<AtomClass> classes(items.size(), AtomClass::Ord);
        std::ptrdiff_t previous = -1;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (items[i].kind == Node::Kind::Space || items[i].kind == Node::Kind::Null) {
                continue;
            }
            AtomClass c = classOf(items[i]);
            const AtomClass before = previous >= 0 ? classes[static_cast<std::size_t>(previous)] : AtomClass::Open;
            if (c == AtomClass::Bin &&
                (previous < 0 || before == AtomClass::Bin || before == AtomClass::Op || before == AtomClass::Rel ||
                 before == AtomClass::Open || before == AtomClass::Punct)) {
                c = AtomClass::Ord;
            }
            if ((c == AtomClass::Rel || c == AtomClass::Close || c == AtomClass::Punct) && previous >= 0 &&
                before == AtomClass::Bin) {
                classes[static_cast<std::size_t>(previous)] = AtomClass::Ord;
            }
            classes[i] = c;
            previous = static_cast<std::ptrdiff_t>(i);
        }
        if (previous >= 0 && classes[static_cast<std::size_t>(previous)] == AtomClass::Bin) {
            classes[static_cast<std::size_t>(previous)] = AtomClass::Ord;
        }

        Box out;
        bool hasLast = false;  // an atom stood before (spaces and nulls are no atoms)
        AtomClass last = AtomClass::Ord;
        for (std::size_t i = 0; i < items.size() && !error_; ++i) {
            const Node& item = items[i];
            if (item.kind == Node::Kind::Null) {
                continue;
            }
            if (item.kind == Node::Kind::Space) {
                out.width += item.amount * px(s);
                continue;
            }
            if (hasLast) {
                out.width += spaceBetween(last, classes[i], s);
            }
            Box b = node(item, s);
            place(out, b, out.width, 0);
            out.width += b.width;
            out.height = std::max(out.height, b.height);
            out.depth = std::max(out.depth, b.depth);
            out.italic = b.italic;
            last = classes[i];
            hasLast = true;
        }
        return out;
    }

    /// A glyph grown to at least `target` along one direction: the glyph itself, the first larger
    /// variant that reaches the target, an assembly of parts, or the largest variant.
    Box stretch(char32_t c, double target, Style s, bool vertical) {
        const std::uint16_t g = glyphFor(c);
        if (g == 0) {
            return {};
        }
        Box base = glyphBox(g, s);
        const double size = vertical ? base.height + base.depth : base.width;
        if (size >= target) {
            return base;
        }
        const auto& constructions = vertical ? table_.vertical : table_.horizontal;
        const auto it = constructions.find(g);
        if (it == constructions.end()) {
            return base;
        }
        const GlyphConstruction& construction = it->second;
        for (const GlyphVariant& v : construction.variants) {
            if (v.advance * unit(s) >= target) {
                return glyphBox(v.glyph, s);
            }
        }
        if (!construction.assembly.empty()) {
            return assemble(construction.assembly, target, s, vertical);
        }
        return construction.variants.empty() ? base : glyphBox(construction.variants.back().glyph, s);
    }

    Box assemble(const std::vector<GlyphPart>& parts, double target, Style s, bool vertical) {
        const double u = unit(s);
        const double minOverlap = table_.minConnectorOverlap * u;
        double fixedAdvance = 0;
        double extenderAdvance = 0;
        std::size_t fixedCount = 0;
        std::size_t extenderCount = 0;
        for (const GlyphPart& p : parts) {
            (p.extender ? extenderAdvance : fixedAdvance) += p.fullAdvance * u;
            ++(p.extender ? extenderCount : fixedCount);
        }
        // The fewest repeats of the extenders whose longest assembly reaches the target.
        int repeats = extenderCount > 0 ? 1 : 0;
        const auto partsWith = [&](int r) { return fixedCount + extenderCount * static_cast<std::size_t>(r); };
        const auto longest = [&](int r) {
            const std::size_t n = partsWith(r);
            return fixedAdvance + extenderAdvance * r - (n > 0 ? static_cast<double>(n - 1) * minOverlap : 0);
        };
        while (extenderCount > 0 && longest(repeats) < target && repeats < kMaxRepeats) {
            ++repeats;
        }
        std::vector<const GlyphPart*> sequence;
        for (const GlyphPart& p : parts) {
            const int times = p.extender ? repeats : 1;
            for (int t = 0; t < times; ++t) {
                sequence.push_back(&p);
            }
        }
        double total = 0;
        for (const GlyphPart* p : sequence) {
            total += p->fullAdvance * u;
        }
        double overlap = minOverlap;
        if (sequence.size() > 1) {
            double maxOverlap = std::numeric_limits<double>::max();
            for (std::size_t i = 0; i + 1 < sequence.size(); ++i) {
                maxOverlap =
                    std::min(maxOverlap, std::min(sequence[i]->endConnector, sequence[i + 1]->startConnector) * u);
            }
            const double wanted = (total - target) / static_cast<double>(sequence.size() - 1);
            overlap = std::clamp(wanted, minOverlap, std::max(minOverlap, maxOverlap));
        }
        Box out;
        double run = 0;
        for (const GlyphPart* p : sequence) {
            const GlyphBounds b = font_.bounds(p->glyph);
            const double advance = font_.advance(p->glyph) * u;
            if (vertical) {
                out.glyphs.push_back({p->glyph, 0, run - b.yMin * u, px(s)});
                out.width = std::max(out.width, advance);
            } else {
                out.glyphs.push_back({p->glyph, run - b.xMin * u, 0, px(s)});
                out.height = std::max(out.height, b.yMax * u);
                out.depth = std::max(out.depth, -b.yMin * u);
            }
            run += p->fullAdvance * u - overlap;
        }
        const double length = run + overlap;
        if (vertical) {
            out.height = length;
        } else {
            out.width = length;
        }
        return out;
    }

    /// A delimiter tall enough for content reaching `height` above and `depth` below the
    /// baseline, centred on the math axis (TeX: \delimiterfactor 901, \delimitershortfall 5pt).
    Box delimiter(char32_t c, double height, double depth, Style s) {
        if (c == 0) {
            Box b;
            b.width = kNullDelimiterSpace * px(s);
            return b;
        }
        const double axis = k(c_.axisHeight, s);
        const double reach = std::max(height - axis, depth + axis);
        const double target = std::max(reach * 2 * 0.901, reach * 2 - 0.5 * px(s));
        Box d = stretch(c, target, s, true);
        Box out;
        const double dy = axis - (d.height - d.depth) / 2;
        place(out, d, 0, dy);
        out.width = d.width;
        out.height = d.height + dy;
        out.depth = d.depth - dy;
        return out;
    }

    Box delimited(char32_t left, const Box& body, char32_t right, Style s) {
        Box l = delimiter(left, body.height, body.depth, s);
        Box r = delimiter(right, body.height, body.depth, s);
        Box out;
        place(out, l, 0, 0);
        place(out, body, l.width, 0);
        place(out, r, l.width + body.width, 0);
        out.width = l.width + body.width + r.width;
        out.height = std::max({l.height, body.height, r.height});
        out.depth = std::max({l.depth, body.depth, r.depth});
        return out;
    }

    Box scripts(const Node& n, Style s) {
        const Node& baseNode = n.children[0];
        const Node& supNode = n.children[1];
        const Node& subNode = n.children[2];
        Box base = node(baseNode, s);
        const bool op = baseNode.kind == Node::Kind::LargeOp ||
                        (baseNode.kind == Node::Kind::Text && baseNode.cls == AtomClass::Op);
        if (op && baseNode.limits && s.level == 0) {
            return limits(base, supNode, subNode, s);
        }
        const bool hasSup = supNode.kind != Node::Kind::Null;
        const bool hasSub = subNode.kind != Node::Kind::Null;
        Box sup = hasSup ? node(supNode, superscriptStyle(s)) : Box{};
        Box sub = hasSub ? node(subNode, subscriptStyle(s)) : Box{};
        // TeX (appendix G, rule 18a): a single character as base has no baseline drop.
        const bool character = baseNode.kind == Node::Kind::Symbol;
        const double baseHeight = character ? 0 : base.height;
        const double baseDepth = character ? 0 : base.depth;
        double subShift = 0;
        double supShift = 0;
        if (hasSub) {
            subShift = std::max({k(c_.subscriptShiftDown, s), baseDepth + k(c_.subscriptBaselineDropMin, s),
                                 sub.height - k(c_.subscriptTopMax, s)});
        }
        if (hasSup) {
            supShift =
                std::max({k(s.cramped ? c_.superscriptShiftUpCramped : c_.superscriptShiftUp, s),
                          baseHeight - k(c_.superscriptBaselineDropMax, s), sup.depth + k(c_.superscriptBottomMin, s)});
        }
        if (hasSup && hasSub) {
            const double gap = (supShift - sup.depth) - (sub.height - subShift);
            const double gapMin = k(c_.subSuperscriptGapMin, s);
            if (gap < gapMin) {
                subShift += gapMin - gap;
                const double lift = k(c_.superscriptBottomMaxWithSubscript, s) - (supShift - sup.depth);
                if (lift > 0) {
                    supShift += lift;
                    subShift -= lift;
                }
            }
        }
        // A large operator's italic correction moves the subscript left; another base's moves
        // the superscript right.
        const double supX = base.width + (baseNode.kind == Node::Kind::LargeOp ? 0 : base.italic);
        const double subX = base.width - (baseNode.kind == Node::Kind::LargeOp ? base.italic : 0);
        Box out;
        place(out, base, 0, 0);
        out.height = base.height;
        out.depth = base.depth;
        double right = base.width;
        if (hasSup) {
            place(out, sup, supX, supShift);
            out.height = std::max(out.height, supShift + sup.height);
            right = std::max(right, supX + sup.width);
        }
        if (hasSub) {
            place(out, sub, subX, -subShift);
            out.depth = std::max(out.depth, subShift + sub.depth);
            right = std::max(right, subX + sub.width);
        }
        out.width = right + k(c_.spaceAfterScript, s);
        return out;
    }

    Box limits(const Box& base, const Node& overNode, const Node& underNode, Style s) {
        const bool hasOver = overNode.kind != Node::Kind::Null;
        const bool hasUnder = underNode.kind != Node::Kind::Null;
        Box over = hasOver ? node(overNode, superscriptStyle(s)) : Box{};
        Box under = hasUnder ? node(underNode, subscriptStyle(s)) : Box{};
        const double width = std::max({base.width, over.width, under.width});
        Box out;
        place(out, base, (width - base.width) / 2, 0);
        out.width = width;
        out.height = base.height;
        out.depth = base.depth;
        if (hasOver) {
            const double rise = std::max(k(c_.upperLimitBaselineRiseMin, s), k(c_.upperLimitGapMin, s) + over.depth);
            const double y = base.height + rise;
            place(out, over, (width - over.width) / 2 + base.italic / 2, y);
            out.height = y + over.height;
        }
        if (hasUnder) {
            const double drop = std::max(k(c_.lowerLimitBaselineDropMin, s), k(c_.lowerLimitGapMin, s) + under.height);
            const double y = base.depth + drop;
            place(out, under, (width - under.width) / 2 - base.italic / 2, -y);
            out.depth = y + under.depth;
        }
        return out;
    }

    Box largeOp(const Node& n, Style s) {
        const std::uint16_t g = glyphFor(n.ch);
        if (g == 0) {
            return {};
        }
        std::uint16_t chosen = g;
        if (s.level == 0) {
            // Display style: the first variant at least DisplayOperatorMinHeight tall.
            if (const auto it = table_.vertical.find(g); it != table_.vertical.end()) {
                const double minHeight = c_.displayOperatorMinHeight;
                for (const GlyphVariant& v : it->second.variants) {
                    chosen = v.glyph;
                    if (v.advance >= minHeight) {
                        break;
                    }
                }
            }
        }
        Box glyph = glyphBox(chosen, s);
        const double axis = k(c_.axisHeight, s);
        const double dy = axis - (glyph.height - glyph.depth) / 2;
        Box out;
        place(out, glyph, 0, dy);
        out.width = glyph.width;
        out.height = glyph.height + dy;
        out.depth = glyph.depth - dy;
        out.italic = glyph.italic;
        return out;
    }

    Box fraction(const Node& n, Style outer) {
        // A forced style (\dfrac, \tfrac) applies to the whole fraction, rule and delimiters too.
        const Style s = n.forcedStyle >= 0 ? Style{n.forcedStyle, outer.cramped} : outer;
        const bool display = s.level == 0;
        Box num = node(n.children[0], numeratorStyle(s));
        Box den = node(n.children[1], denominatorStyle(s));
        const double axis = k(c_.axisHeight, s);
        double numShift = 0;
        double denShift = 0;
        Box stack;
        const double pad = n.rule ? kNullDelimiterSpace * px(s) : 0;
        const double inner = std::max(num.width, den.width);
        stack.width = inner + 2 * pad;
        if (n.rule) {
            const double t = k(c_.fractionRuleThickness, s);
            numShift =
                std::max(k(display ? c_.fractionNumeratorDisplayStyleShiftUp : c_.fractionNumeratorShiftUp, s),
                         axis + t / 2 + k(display ? c_.fractionNumDisplayStyleGapMin : c_.fractionNumeratorGapMin, s) +
                             num.depth);
            denShift = std::max(
                k(display ? c_.fractionDenominatorDisplayStyleShiftDown : c_.fractionDenominatorShiftDown, s),
                den.height + k(display ? c_.fractionDenomDisplayStyleGapMin : c_.fractionDenominatorGapMin, s) - axis +
                    t / 2);
            stack.rules.push_back({pad, axis - t / 2, inner, t});
        } else {
            numShift = k(display ? c_.stackTopDisplayStyleShiftUp : c_.stackTopShiftUp, s);
            denShift = k(display ? c_.stackBottomDisplayStyleShiftDown : c_.stackBottomShiftDown, s);
            const double gap = (numShift - num.depth) - (den.height - denShift);
            const double gapMin = k(display ? c_.stackDisplayStyleGapMin : c_.stackGapMin, s);
            if (gap < gapMin) {
                numShift += (gapMin - gap) / 2;
                denShift += (gapMin - gap) / 2;
            }
        }
        place(stack, num, pad + (inner - num.width) / 2, numShift);
        place(stack, den, pad + (inner - den.width) / 2, -denShift);
        stack.height = numShift + num.height;
        stack.depth = denShift + den.depth;
        if (n.left == 0 && n.right == 0) {
            return stack;
        }
        return delimited(n.left, stack, n.right, s);
    }

    Box radical(const Node& n, Style s) {
        const bool display = s.level == 0;
        Box radicand = node(n.children[0], {s.level, true});
        double gap = k(display ? c_.radicalDisplayStyleVerticalGap : c_.radicalVerticalGap, s);
        const double rule = k(c_.radicalRuleThickness, s);
        const double target = radicand.height + radicand.depth + gap + rule;
        Box surd = stretch(U'√', target, s, true);
        const double surdSize = surd.height + surd.depth;
        if (surdSize > target) {
            gap += (surdSize - target) / 2;  // TeX centres the extra space of a larger sign
        }
        const double top = radicand.height + gap + rule;
        const double dy = top - surd.height;
        double x = 0;
        Box out;
        if (n.children[1].kind != Node::Kind::Null) {
            Box degree = node(n.children[1], {3, false});
            const double before = k(c_.radicalKernBeforeDegree, s);
            const double after = k(c_.radicalKernAfterDegree, s);
            const double bottom = dy - surd.depth;
            const double raise = c_.radicalDegreeBottomRaisePercent / 100 * surdSize;
            const double y = bottom + raise + degree.depth;
            place(out, degree, before, y);
            out.height = y + degree.height;
            x = std::max(0.0, before + degree.width + after);
        }
        place(out, surd, x, dy);
        const double bodyX = x + surd.width;
        place(out, radicand, bodyX, 0);
        out.rules.push_back({bodyX, radicand.height + gap, radicand.width, rule});
        out.width = bodyX + radicand.width;
        out.height = std::max(out.height, top + k(c_.radicalExtraAscender, s));
        out.depth = std::max(radicand.depth, surd.depth - dy);
        return out;
    }

    Box array(const Node& n, Style s) {
        const Style cell{n.displayCells ? 0 : 1, false};
        const std::size_t rows = n.rows;
        const std::size_t columns = n.columns;
        if (rows == 0 || columns == 0 || n.children.size() != rows * columns) {
            return {};
        }
        std::vector<Box> boxes;
        boxes.reserve(n.children.size());
        for (const Node& c : n.children) {
            boxes.push_back(node(c, cell));
            if (error_) {
                return {};
            }
        }
        const double em = px(cell);
        const double strutHeight = 0.7 * 1.2 * em;  // \arraystretch strut: 0.7 and 0.3 \baselineskip
        const double strutDepth = 0.3 * 1.2 * em;
        std::vector<double> widths(columns, 0);
        std::vector<double> heights(rows, strutHeight);
        std::vector<double> depths(rows, strutDepth);
        for (std::size_t r = 0; r < rows; ++r) {
            for (std::size_t c = 0; c < columns; ++c) {
                const Box& b = boxes[r * columns + c];
                widths[c] = std::max(widths[c], b.width);
                heights[r] = std::max(heights[r], b.height);
                depths[r] = std::max(depths[r], b.depth);
            }
        }
        const bool pairs = !n.align.empty() && n.align[0] == 'r' && columns > 1 && n.align[1] == 'l';
        std::vector<double> xs(columns, 0);
        double x = 0;
        for (std::size_t c = 0; c < columns; ++c) {
            if (c > 0 && (!pairs || c % 2 == 0)) {
                x += n.columnGap * em;
            }
            xs[c] = x;
            x += widths[c];
        }
        const double rowGap = n.rowGap * em;
        double total = 0;
        for (std::size_t r = 0; r < rows; ++r) {
            total += heights[r] + depths[r];
        }
        total += rowGap * static_cast<double>(rows - 1);
        const double top = k(c_.axisHeight, s) + total / 2;
        Box out;
        double y = top;
        for (std::size_t r = 0; r < rows; ++r) {
            const double baseline = y - heights[r];
            for (std::size_t c = 0; c < columns; ++c) {
                const Box& b = boxes[r * columns + c];
                const char align = c < n.align.size() ? n.align[c] : 'c';
                const double free = widths[c] - b.width;
                const double dx = align == 'l' ? 0 : align == 'r' ? free : free / 2;
                place(out, b, xs[c] + dx, baseline);
            }
            y = baseline - depths[r] - rowGap;
        }
        out.width = x;
        out.height = top;
        out.depth = total - top;
        return out;
    }

    Box xarrow(const Node& n, Style s) {
        const bool hasAbove = n.children[0].kind != Node::Kind::Null &&
                              !(n.children[0].kind == Node::Kind::Group && n.children[0].children.empty());
        const bool hasBelow = n.children[1].kind != Node::Kind::Null &&
                              !(n.children[1].kind == Node::Kind::Group && n.children[1].children.empty());
        Box above = hasAbove ? node(n.children[0], superscriptStyle(s)) : Box{};
        Box below = hasBelow ? node(n.children[1], subscriptStyle(s)) : Box{};
        const double padding = 5.0 / 18 * px(s);  // a thick space on each side of the labels
        const double target = std::max({above.width, below.width}) + 2 * padding;
        Box arrow = stretch(n.ch, target, s, false);
        const double width = std::max(arrow.width, target);
        Box out;
        place(out, arrow, (width - arrow.width) / 2, 0);
        out.width = width;
        out.height = arrow.height;
        out.depth = arrow.depth;
        if (hasAbove) {
            const double y = std::max(k(c_.stretchStackTopShiftUp, s),
                                      arrow.height + k(c_.stretchStackGapAboveMin, s) + above.depth);
            place(out, above, (width - above.width) / 2, y);
            out.height = y + above.height;
        }
        if (hasBelow) {
            const double y = std::max(k(c_.stretchStackBottomShiftDown, s),
                                      arrow.depth + k(c_.stretchStackGapBelowMin, s) + below.height);
            place(out, below, (width - below.width) / 2, -y);
            out.depth = y + below.depth;
        }
        return out;
    }
};

}  // namespace

MathLayout layoutMath(std::u16string_view source, const MathFont& font, const MathTable& table, double pixelSize,
                      bool display) {
    MathLayout result;
    math::ParseResult parsed = math::parse(source);
    if (parsed.error) {
        result.error = parsed.error;
        return result;
    }
    Layout layout(font, table, pixelSize > 0 ? pixelSize : 1);
    Box box = layout.node(parsed.root, {display ? 0 : 1, false});
    if (layout.error()) {
        result.error = layout.error();
        return result;
    }
    result.width = box.width;
    result.ascent = box.height;
    result.descent = box.depth;
    result.glyphs.reserve(box.glyphs.size());
    for (const MathGlyph& g : box.glyphs) {
        result.glyphs.push_back({g.glyph, g.x, -g.y, g.size});
    }
    for (const MathRule& r : box.rules) {
        result.rules.push_back({r.x, -(r.y + r.height), r.width, r.height});
    }
    return result;
}

}  // namespace hitokoto::core
