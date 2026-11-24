// SPDX-License-Identifier: GPL-3.0-or-later
// S045: reading the LaTeX math subset (S045, decision 4) into a tree. One pass over the source;
// each node consumes at least one token, so the tree is linear in the source, and nesting stops at
// 64 levels with an error instead of a deep recursion.
#include "math_parse.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace hitokoto::core::math {
namespace {

constexpr int kMaxDepth = 64;

enum class Variant : std::uint8_t { Normal, Bold, Roman, Italic };

struct SymbolEntry {
    std::u16string_view name;
    char32_t ch;
    AtomClass cls;
};

// Commands that stand for one character. Greek letters go through mapGreek() for their italic or
// bold forms; the others are used as they are.
constexpr std::array kSymbols{
    // Greek, lower case (TeX's \epsilon and \phi are the lunate and closed forms).
    SymbolEntry{u"alpha", U'α', AtomClass::Ord},
    SymbolEntry{u"beta", U'β', AtomClass::Ord},
    SymbolEntry{u"gamma", U'γ', AtomClass::Ord},
    SymbolEntry{u"delta", U'δ', AtomClass::Ord},
    SymbolEntry{u"epsilon", U'ϵ', AtomClass::Ord},
    SymbolEntry{u"varepsilon", U'ε', AtomClass::Ord},
    SymbolEntry{u"zeta", U'ζ', AtomClass::Ord},
    SymbolEntry{u"eta", U'η', AtomClass::Ord},
    SymbolEntry{u"theta", U'θ', AtomClass::Ord},
    SymbolEntry{u"vartheta", U'ϑ', AtomClass::Ord},
    SymbolEntry{u"iota", U'ι', AtomClass::Ord},
    SymbolEntry{u"kappa", U'κ', AtomClass::Ord},
    SymbolEntry{u"varkappa", U'ϰ', AtomClass::Ord},
    SymbolEntry{u"lambda", U'λ', AtomClass::Ord},
    SymbolEntry{u"mu", U'μ', AtomClass::Ord},
    SymbolEntry{u"nu", U'ν', AtomClass::Ord},
    SymbolEntry{u"xi", U'ξ', AtomClass::Ord},
    SymbolEntry{u"omicron", U'ο', AtomClass::Ord},
    SymbolEntry{u"pi", U'π', AtomClass::Ord},
    SymbolEntry{u"varpi", U'ϖ', AtomClass::Ord},
    SymbolEntry{u"rho", U'ρ', AtomClass::Ord},
    SymbolEntry{u"varrho", U'ϱ', AtomClass::Ord},
    SymbolEntry{u"sigma", U'σ', AtomClass::Ord},
    SymbolEntry{u"varsigma", U'ς', AtomClass::Ord},
    SymbolEntry{u"tau", U'τ', AtomClass::Ord},
    SymbolEntry{u"upsilon", U'υ', AtomClass::Ord},
    SymbolEntry{u"phi", U'ϕ', AtomClass::Ord},
    SymbolEntry{u"varphi", U'φ', AtomClass::Ord},
    SymbolEntry{u"chi", U'χ', AtomClass::Ord},
    SymbolEntry{u"psi", U'ψ', AtomClass::Ord},
    SymbolEntry{u"omega", U'ω', AtomClass::Ord},
    // Greek, upper case (upright, as TeX sets them).
    SymbolEntry{u"Gamma", U'Γ', AtomClass::Ord},
    SymbolEntry{u"Delta", U'Δ', AtomClass::Ord},
    SymbolEntry{u"Theta", U'Θ', AtomClass::Ord},
    SymbolEntry{u"Lambda", U'Λ', AtomClass::Ord},
    SymbolEntry{u"Xi", U'Ξ', AtomClass::Ord},
    SymbolEntry{u"Pi", U'Π', AtomClass::Ord},
    SymbolEntry{u"Sigma", U'Σ', AtomClass::Ord},
    SymbolEntry{u"Upsilon", U'Υ', AtomClass::Ord},
    SymbolEntry{u"Phi", U'Φ', AtomClass::Ord},
    SymbolEntry{u"Psi", U'Ψ', AtomClass::Ord},
    SymbolEntry{u"Omega", U'Ω', AtomClass::Ord},
    // Ordinary symbols.
    SymbolEntry{u"partial", U'∂', AtomClass::Ord},
    SymbolEntry{u"nabla", U'∇', AtomClass::Ord},
    SymbolEntry{u"infty", U'∞', AtomClass::Ord},
    SymbolEntry{u"hbar", U'ℏ', AtomClass::Ord},
    SymbolEntry{u"ell", U'ℓ', AtomClass::Ord},
    SymbolEntry{u"emptyset", U'∅', AtomClass::Ord},
    SymbolEntry{u"varnothing", U'∅', AtomClass::Ord},
    SymbolEntry{u"forall", U'∀', AtomClass::Ord},
    SymbolEntry{u"exists", U'∃', AtomClass::Ord},
    SymbolEntry{u"neg", U'¬', AtomClass::Ord},
    SymbolEntry{u"lnot", U'¬', AtomClass::Ord},
    SymbolEntry{u"prime", U'′', AtomClass::Ord},
    SymbolEntry{u"angle", U'∠', AtomClass::Ord},
    SymbolEntry{u"triangle", U'△', AtomClass::Ord},
    SymbolEntry{u"vdots", U'⋮', AtomClass::Ord},
    SymbolEntry{u"ldots", U'…', AtomClass::Inner},
    SymbolEntry{u"dots", U'…', AtomClass::Inner},
    SymbolEntry{u"cdots", U'⋯', AtomClass::Inner},
    SymbolEntry{u"ddots", U'⋱', AtomClass::Inner},
    SymbolEntry{u"aleph", U'ℵ', AtomClass::Ord},
    SymbolEntry{u"Re", U'ℜ', AtomClass::Ord},
    SymbolEntry{u"Im", U'ℑ', AtomClass::Ord},
    SymbolEntry{u"degree", U'°', AtomClass::Ord},
    SymbolEntry{u"vert", U'|', AtomClass::Ord},
    SymbolEntry{u"Vert", U'‖', AtomClass::Ord},
    SymbolEntry{u"|", U'‖', AtomClass::Ord},
    SymbolEntry{u"$", U'$', AtomClass::Ord},
    SymbolEntry{u"%", U'%', AtomClass::Ord},
    SymbolEntry{u"&", U'&', AtomClass::Ord},
    SymbolEntry{u"#", U'#', AtomClass::Ord},
    SymbolEntry{u"_", U'_', AtomClass::Ord},
    // Binary operators.
    SymbolEntry{u"pm", U'±', AtomClass::Bin},
    SymbolEntry{u"mp", U'∓', AtomClass::Bin},
    SymbolEntry{u"times", U'×', AtomClass::Bin},
    SymbolEntry{u"div", U'÷', AtomClass::Bin},
    SymbolEntry{u"cdot", U'⋅', AtomClass::Bin},
    SymbolEntry{u"ast", U'∗', AtomClass::Bin},
    SymbolEntry{u"star", U'⋆', AtomClass::Bin},
    SymbolEntry{u"circ", U'∘', AtomClass::Bin},
    SymbolEntry{u"bullet", U'∙', AtomClass::Bin},
    SymbolEntry{u"cup", U'∪', AtomClass::Bin},
    SymbolEntry{u"cap", U'∩', AtomClass::Bin},
    SymbolEntry{u"wedge", U'∧', AtomClass::Bin},
    SymbolEntry{u"land", U'∧', AtomClass::Bin},
    SymbolEntry{u"vee", U'∨', AtomClass::Bin},
    SymbolEntry{u"lor", U'∨', AtomClass::Bin},
    SymbolEntry{u"oplus", U'⊕', AtomClass::Bin},
    SymbolEntry{u"ominus", U'⊖', AtomClass::Bin},
    SymbolEntry{u"otimes", U'⊗', AtomClass::Bin},
    SymbolEntry{u"setminus", U'∖', AtomClass::Bin},
    // Relations.
    SymbolEntry{u"leq", U'≤', AtomClass::Rel},
    SymbolEntry{u"le", U'≤', AtomClass::Rel},
    SymbolEntry{u"geq", U'≥', AtomClass::Rel},
    SymbolEntry{u"ge", U'≥', AtomClass::Rel},
    SymbolEntry{u"neq", U'≠', AtomClass::Rel},
    SymbolEntry{u"ne", U'≠', AtomClass::Rel},
    SymbolEntry{u"ll", U'≪', AtomClass::Rel},
    SymbolEntry{u"gg", U'≫', AtomClass::Rel},
    SymbolEntry{u"equiv", U'≡', AtomClass::Rel},
    SymbolEntry{u"approx", U'≈', AtomClass::Rel},
    SymbolEntry{u"sim", U'∼', AtomClass::Rel},
    SymbolEntry{u"simeq", U'≃', AtomClass::Rel},
    SymbolEntry{u"cong", U'≅', AtomClass::Rel},
    SymbolEntry{u"propto", U'∝', AtomClass::Rel},
    SymbolEntry{u"in", U'∈', AtomClass::Rel},
    SymbolEntry{u"notin", U'∉', AtomClass::Rel},
    SymbolEntry{u"ni", U'∋', AtomClass::Rel},
    SymbolEntry{u"subset", U'⊂', AtomClass::Rel},
    SymbolEntry{u"supset", U'⊃', AtomClass::Rel},
    SymbolEntry{u"subseteq", U'⊆', AtomClass::Rel},
    SymbolEntry{u"supseteq", U'⊇', AtomClass::Rel},
    SymbolEntry{u"perp", U'⟂', AtomClass::Rel},
    SymbolEntry{u"parallel", U'∥', AtomClass::Rel},
    SymbolEntry{u"mid", U'∣', AtomClass::Rel},
    SymbolEntry{u"to", U'→', AtomClass::Rel},
    SymbolEntry{u"rightarrow", U'→', AtomClass::Rel},
    SymbolEntry{u"leftarrow", U'←', AtomClass::Rel},
    SymbolEntry{u"gets", U'←', AtomClass::Rel},
    SymbolEntry{u"leftrightarrow", U'↔', AtomClass::Rel},
    SymbolEntry{u"Rightarrow", U'⇒', AtomClass::Rel},
    SymbolEntry{u"Leftarrow", U'⇐', AtomClass::Rel},
    SymbolEntry{u"Leftrightarrow", U'⇔', AtomClass::Rel},
    SymbolEntry{u"implies", U'⟹', AtomClass::Rel},
    SymbolEntry{u"iff", U'⟺', AtomClass::Rel},
    SymbolEntry{u"mapsto", U'↦', AtomClass::Rel},
    SymbolEntry{u"uparrow", U'↑', AtomClass::Rel},
    SymbolEntry{u"downarrow", U'↓', AtomClass::Rel},
    SymbolEntry{u"updownarrow", U'↕', AtomClass::Rel},
    SymbolEntry{u"rightleftharpoons", U'⇌', AtomClass::Rel},
    // Punctuation.
    SymbolEntry{u"colon", U':', AtomClass::Punct},
    // Delimiters.
    SymbolEntry{u"{", U'{', AtomClass::Open},
    SymbolEntry{u"}", U'}', AtomClass::Close},
    SymbolEntry{u"lbrace", U'{', AtomClass::Open},
    SymbolEntry{u"rbrace", U'}', AtomClass::Close},
    SymbolEntry{u"langle", U'⟨', AtomClass::Open},
    SymbolEntry{u"rangle", U'⟩', AtomClass::Close},
    SymbolEntry{u"lfloor", U'⌊', AtomClass::Open},
    SymbolEntry{u"rfloor", U'⌋', AtomClass::Close},
    SymbolEntry{u"lceil", U'⌈', AtomClass::Open},
    SymbolEntry{u"rceil", U'⌉', AtomClass::Close},
    SymbolEntry{u"lvert", U'|', AtomClass::Open},
    SymbolEntry{u"rvert", U'|', AtomClass::Close},
    SymbolEntry{u"lVert", U'‖', AtomClass::Open},
    SymbolEntry{u"rVert", U'‖', AtomClass::Close},
};

struct LargeOpEntry {
    std::u16string_view name;
    char32_t ch;
    bool limits;
};

constexpr std::array kLargeOps{
    LargeOpEntry{u"sum", U'∑', true},       LargeOpEntry{u"prod", U'∏', true},   LargeOpEntry{u"coprod", U'∐', true},
    LargeOpEntry{u"bigcup", U'⋃', true},    LargeOpEntry{u"bigcap", U'⋂', true}, LargeOpEntry{u"bigoplus", U'⨁', true},
    LargeOpEntry{u"bigotimes", U'⨂', true}, LargeOpEntry{u"int", U'∫', false},   LargeOpEntry{u"iint", U'∬', false},
    LargeOpEntry{u"iiint", U'∭', false},    LargeOpEntry{u"oint", U'∮', false},
};

struct OperatorName {
    std::u16string_view name;
    bool limits;
};

constexpr std::array kOperatorNames{
    OperatorName{u"sin", false},    OperatorName{u"cos", false},    OperatorName{u"tan", false},
    OperatorName{u"cot", false},    OperatorName{u"sec", false},    OperatorName{u"csc", false},
    OperatorName{u"sinh", false},   OperatorName{u"cosh", false},   OperatorName{u"tanh", false},
    OperatorName{u"coth", false},   OperatorName{u"arcsin", false}, OperatorName{u"arccos", false},
    OperatorName{u"arctan", false}, OperatorName{u"log", false},    OperatorName{u"ln", false},
    OperatorName{u"lg", false},     OperatorName{u"exp", false},    OperatorName{u"arg", false},
    OperatorName{u"deg", false},    OperatorName{u"dim", false},    OperatorName{u"hom", false},
    OperatorName{u"ker", false},    OperatorName{u"lim", true},     OperatorName{u"liminf", true},
    OperatorName{u"limsup", true},  OperatorName{u"max", true},     OperatorName{u"min", true},
    OperatorName{u"sup", true},     OperatorName{u"inf", true},     OperatorName{u"det", true},
    OperatorName{u"gcd", true},     OperatorName{u"Pr", true},
};

struct SpaceEntry {
    std::u16string_view name;
    double em;
};

constexpr double kMu = 1.0 / 18.0;
constexpr std::array kSpaces{
    SpaceEntry{u",", 3 * kMu},          SpaceEntry{u"thinspace", 3 * kMu}, SpaceEntry{u":", 4 * kMu},
    SpaceEntry{u">", 4 * kMu},          SpaceEntry{u"medspace", 4 * kMu},  SpaceEntry{u";", 5 * kMu},
    SpaceEntry{u"thickspace", 5 * kMu}, SpaceEntry{u"!", -3 * kMu},        SpaceEntry{u"negthinspace", -3 * kMu},
    SpaceEntry{u"quad", 1.0},           SpaceEntry{u"qquad", 2.0},         SpaceEntry{u" ", 1.0 / 3.0},
};

/// Commands that change nothing in this subset (style switches TeX applies to the rest of a group).
constexpr std::array<std::u16string_view, 6> kIgnored{u"displaystyle",      u"textstyle", u"scriptstyle",
                                                      u"scriptscriptstyle", u"limits",    u"nolimits"};

char32_t mapLatin(char32_t c, Variant v) {
    const bool lower = c >= U'a' && c <= U'z';
    const bool upper = c >= U'A' && c <= U'Z';
    if (!lower && !upper) {
        return c;
    }
    switch (v) {
        case Variant::Roman:
            return c;
        case Variant::Bold:
            return lower ? U'\U0001D41A' + (c - U'a') : U'\U0001D400' + (c - U'A');
        case Variant::Normal:
        case Variant::Italic:
            if (c == U'h') {
                return U'ℎ';  // Planck's constant stands in for the italic h in Unicode
            }
            return lower ? U'\U0001D44E' + (c - U'a') : U'\U0001D434' + (c - U'A');
    }
    return c;
}

char32_t mapDigit(char32_t c, Variant v) {
    return v == Variant::Bold && c >= U'0' && c <= U'9' ? U'\U0001D7CE' + (c - U'0') : c;
}

char32_t mapGreek(char32_t c, Variant v) {
    // The symbol forms after omega in the Mathematical Alphanumeric Symbols block.
    struct Special {
        char32_t plain, italic, bold;
    };
    static constexpr std::array kSpecial{
        Special{U'∂', U'\U0001D715', U'\U0001D6DB'}, Special{U'ϵ', U'\U0001D716', U'\U0001D6DC'},
        Special{U'ϑ', U'\U0001D717', U'\U0001D6DD'}, Special{U'ϰ', U'\U0001D718', U'\U0001D6DE'},
        Special{U'ϕ', U'\U0001D719', U'\U0001D6DF'}, Special{U'ϱ', U'\U0001D71A', U'\U0001D6E0'},
        Special{U'ϖ', U'\U0001D71B', U'\U0001D6E1'},
    };
    for (const Special& s : kSpecial) {
        if (s.plain == c) {
            return v == Variant::Bold ? s.bold : v == Variant::Roman ? c : s.italic;
        }
    }
    if (c >= U'α' && c <= U'ω') {
        return v == Variant::Bold ? U'\U0001D6C2' + (c - U'α') : v == Variant::Roman ? c : U'\U0001D6FC' + (c - U'α');
    }
    if (c >= U'Α' && c <= U'Ω') {
        return v == Variant::Bold ? U'\U0001D6A8' + (c - U'Α') : v == Variant::Italic ? U'\U0001D6E2' + (c - U'Α') : c;
    }
    return c;
}

bool isLetter(char16_t c) noexcept {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

bool isSpace(char16_t c) noexcept {
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
}

struct Token {
    enum class Kind : std::uint8_t { End, Char, Command, Open, Close, Sup, Sub, Amp, Prime, Tilde };
    Kind kind = Kind::End;
    char32_t ch = 0;
    std::u16string_view name;
    std::uint32_t position = 0;
};

Node symbol(char32_t ch, AtomClass cls, char32_t plain = 0) {
    Node n;
    n.kind = Node::Kind::Symbol;
    n.ch = ch;
    n.cls = cls;
    n.plain = plain != 0 && plain != ch ? plain : 0;
    return n;
}

Node nullNode() {
    return {};
}

class Parser {
public:
    explicit Parser(std::u16string_view source) : s_(source) {}

    ParseResult run() {
        ParseResult result;
        result.root = list();
        if (!error_) {
            const Token t = peek();
            if (t.kind == Token::Kind::Close) {
                fail(t.position, "unexpected }");
            } else if (t.kind == Token::Kind::Amp) {
                fail(t.position, "& outside an environment");
            } else if (t.kind == Token::Kind::Command && t.name == u"\\") {
                fail(t.position, "\\\\ outside an environment");
            } else if (t.kind == Token::Kind::Command && t.name == u"right") {
                fail(t.position, "\\right without \\left");
            } else if (t.kind == Token::Kind::Command && t.name == u"end") {
                fail(t.position, "\\end without \\begin");
            } else if (t.kind != Token::Kind::End) {
                fail(t.position, "unexpected input");
            }
        }
        result.error = error_;
        if (error_) {
            result.root = Node{};
        }
        return result;
    }

private:
    std::u16string_view s_;
    std::size_t pos_ = 0;
    int depth_ = 0;
    Variant variant_ = Variant::Normal;
    std::optional<MathError> error_;

    void fail(std::uint32_t position, std::string message) {
        if (!error_) {
            error_ = MathError{position, std::move(message)};
        }
    }

    void skipSpaces() {
        while (pos_ < s_.size() && isSpace(s_[pos_])) {
            ++pos_;
        }
    }

    /// The token at the current position (after spaces), without consuming it.
    Token peek() {
        skipSpaces();
        const std::size_t save = pos_;
        Token t = next();
        pos_ = save;
        return t;
    }

    Token next() {
        skipSpaces();
        Token t;
        t.position = static_cast<std::uint32_t>(pos_);
        if (pos_ >= s_.size()) {
            return t;
        }
        const char16_t c = s_[pos_++];
        switch (c) {
            case u'{':
                t.kind = Token::Kind::Open;
                return t;
            case u'}':
                t.kind = Token::Kind::Close;
                return t;
            case u'^':
                t.kind = Token::Kind::Sup;
                return t;
            case u'_':
                t.kind = Token::Kind::Sub;
                return t;
            case u'&':
                t.kind = Token::Kind::Amp;
                return t;
            case u'\'':
                t.kind = Token::Kind::Prime;
                return t;
            case u'~':
                t.kind = Token::Kind::Tilde;
                return t;
            case u'\\': {
                t.kind = Token::Kind::Command;
                const std::size_t start = pos_;
                if (pos_ < s_.size() && isLetter(s_[pos_])) {
                    while (pos_ < s_.size() && isLetter(s_[pos_])) {
                        ++pos_;
                    }
                } else if (pos_ < s_.size()) {
                    ++pos_;  // a command of one non-letter character: \, \{ \\ ...
                }
                t.name = s_.substr(start, pos_ - start);
                return t;
            }
            default:
                break;
        }
        t.kind = Token::Kind::Char;
        t.ch = c;
        if (c >= 0xD800 && c <= 0xDBFF && pos_ < s_.size() && s_[pos_] >= 0xDC00 && s_[pos_] <= 0xDFFF) {
            t.ch = 0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10U) + (s_[pos_] - 0xDC00);
            ++pos_;
        }
        return t;
    }

    bool enter(std::uint32_t position) {
        if (++depth_ > kMaxDepth) {
            fail(position, "too deeply nested");
            return false;
        }
        return true;
    }
    void leave() { --depth_; }

    /// Atoms until the end, `}`, `&`, `\\`, `\right` or `\end`; the caller decides what may end it.
    Node list() {
        Node group;
        group.kind = Node::Kind::Group;
        while (!error_) {
            const Token t = peek();
            if (t.kind == Token::Kind::End || t.kind == Token::Kind::Close || t.kind == Token::Kind::Amp ||
                (t.kind == Token::Kind::Command && (t.name == u"\\" || t.name == u"right" || t.name == u"end"))) {
                break;
            }
            Node item = atomWithScripts();
            if (item.kind != Node::Kind::Null) {
                group.children.push_back(std::move(item));
            }
        }
        return group;
    }

    /// `{ list }` after the opening brace was read.
    Node groupBody(std::uint32_t open) {
        if (!enter(open)) {
            return nullNode();
        }
        Node body = list();
        leave();
        const Token t = next();
        if (!error_ && t.kind != Token::Kind::Close) {
            fail(open, "missing }");
        }
        return body;
    }

    /// A required argument: a group, or one token.
    Node argument(std::uint32_t at) {
        const Token t = peek();
        if (t.kind == Token::Kind::Open) {
            (void)next();
            return groupBody(t.position);
        }
        if (t.kind == Token::Kind::End || t.kind == Token::Kind::Close || t.kind == Token::Kind::Amp ||
            t.kind == Token::Kind::Sup || t.kind == Token::Kind::Sub) {
            fail(at, "missing argument");
            return nullNode();
        }
        if (!enter(t.position)) {
            return nullNode();
        }
        Node n = atom();
        leave();
        return n;
    }

    /// `[ ... ]` if present, read as a list up to the closing bracket.
    std::optional<Node> optionalArgument() {
        skipSpaces();
        if (pos_ >= s_.size() || s_[pos_] != u'[') {
            return std::nullopt;
        }
        const auto open = static_cast<std::uint32_t>(pos_);
        ++pos_;
        if (!enter(open)) {
            return nullNode();
        }
        Node group;
        group.kind = Node::Kind::Group;
        while (!error_) {
            skipSpaces();
            if (pos_ >= s_.size()) {
                fail(open, "missing ]");
                break;
            }
            if (s_[pos_] == u']') {
                ++pos_;
                break;
            }
            const Token t = peek();
            if (t.kind == Token::Kind::Close || t.kind == Token::Kind::Amp) {
                fail(open, "missing ]");
                break;
            }
            Node item = atomWithScripts();
            if (item.kind != Node::Kind::Null) {
                group.children.push_back(std::move(item));
            }
        }
        leave();
        return group;
    }

    Node atomWithScripts() {
        const std::uint32_t start = peek().position;
        Node base = atom();
        Node sup;
        Node sub;
        bool haveSup = false;
        bool haveSub = false;
        while (!error_) {
            const Token t = peek();
            if (t.kind == Token::Kind::Command && (t.name == u"limits" || t.name == u"nolimits")) {
                (void)next();
                if (base.kind == Node::Kind::LargeOp || (base.kind == Node::Kind::Text && base.cls == AtomClass::Op)) {
                    base.limits = t.name == u"limits";
                } else {
                    fail(t.position, "\\limits must follow an operator");
                }
                continue;
            }
            if (t.kind == Token::Kind::Prime) {
                if (haveSup) {
                    fail(t.position, "double superscript");
                    break;
                }
                (void)next();
                Node primes;
                primes.kind = Node::Kind::Group;
                primes.children.push_back(symbol(U'′', AtomClass::Ord));
                while (peek().kind == Token::Kind::Prime) {
                    (void)next();
                    primes.children.push_back(symbol(U'′', AtomClass::Ord));
                }
                if (peek().kind == Token::Kind::Sup) {
                    const Token caret = next();
                    Node more = argument(caret.position);
                    primes.children.push_back(std::move(more));
                }
                sup = std::move(primes);
                haveSup = true;
                continue;
            }
            if (t.kind != Token::Kind::Sup && t.kind != Token::Kind::Sub) {
                break;
            }
            (void)next();
            const bool isSup = t.kind == Token::Kind::Sup;
            if ((isSup && haveSup) || (!isSup && haveSub)) {
                fail(t.position, isSup ? "double superscript" : "double subscript");
                break;
            }
            Node arg = argument(t.position);
            if (isSup) {
                sup = std::move(arg);
                haveSup = true;
            } else {
                sub = std::move(arg);
                haveSub = true;
            }
        }
        if (!haveSup && !haveSub) {
            return base;
        }
        if (base.kind == Node::Kind::Null) {
            base.kind = Node::Kind::Group;  // `^2` at the start: an empty base, as TeX allows
        }
        (void)start;
        Node scripts;
        scripts.kind = Node::Kind::Scripts;
        scripts.cls = base.cls;
        scripts.children.push_back(std::move(base));
        scripts.children.push_back(std::move(sup));
        scripts.children.push_back(std::move(sub));
        return scripts;
    }

    Node character(char32_t c) {
        if ((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z')) {
            return symbol(mapLatin(c, variant_), AtomClass::Ord, c);
        }
        if (c >= U'0' && c <= U'9') {
            return symbol(mapDigit(c, variant_), AtomClass::Ord, c);
        }
        switch (c) {
            case U'+':
                return symbol(c, AtomClass::Bin);
            case U'-':
                return symbol(U'−', AtomClass::Bin, c);
            case U'*':
                return symbol(U'∗', AtomClass::Bin, c);
            case U'=':
            case U'<':
            case U'>':
            case U':':
                return symbol(c, AtomClass::Rel);
            case U',':
            case U';':
                return symbol(c, AtomClass::Punct);
            case U'(':
            case U'[':
                return symbol(c, AtomClass::Open);
            case U')':
            case U']':
            case U'!':
            case U'?':
                return symbol(c, AtomClass::Close);
            default:
                break;
        }
        if (c >= U'α' && c <= U'ω') {
            return symbol(mapGreek(c, variant_), AtomClass::Ord, c);
        }
        return symbol(c, AtomClass::Ord);
    }

    Node atom() {
        const Token t = next();
        switch (t.kind) {
            case Token::Kind::Char:
                return character(t.ch);
            case Token::Kind::Open: {
                Node g = groupBody(t.position);
                return g;
            }
            case Token::Kind::Tilde: {
                Node n;
                n.kind = Node::Kind::Space;
                n.amount = 1.0 / 3.0;
                return n;
            }
            case Token::Kind::Command:
                return command(t);
            case Token::Kind::Sup:
            case Token::Kind::Sub:
            case Token::Kind::Prime: {
                // A script with no base before it: put the token back and let the caller attach
                // it to an empty base.
                pos_ = t.position;
                Node empty;
                empty.kind = Node::Kind::Group;
                return empty;
            }
            case Token::Kind::End:
            case Token::Kind::Close:
            case Token::Kind::Amp:
                fail(t.position, "unexpected input");
                return nullNode();
        }
        return nullNode();
    }

    /// The characters of a `{...}` argument as they are, braces balanced, `\` before one
    /// character keeping that character (\text, \operatorname).
    std::optional<std::u32string> rawArgument(std::uint32_t at) {
        skipSpaces();
        if (pos_ >= s_.size() || s_[pos_] != u'{') {
            fail(at, "missing argument");
            return std::nullopt;
        }
        const auto open = static_cast<std::uint32_t>(pos_);
        ++pos_;
        std::u32string text;
        int level = 0;
        while (pos_ < s_.size()) {
            const char16_t c = s_[pos_++];
            if (c == u'\\' && pos_ < s_.size()) {
                const char16_t e = s_[pos_];
                if (isLetter(e)) {
                    fail(static_cast<std::uint32_t>(pos_ - 1), "commands are not supported inside \\text");
                    return std::nullopt;
                }
                text.push_back(e);
                ++pos_;
                continue;
            }
            if (c == u'{') {
                ++level;
                continue;
            }
            if (c == u'}') {
                if (level == 0) {
                    return text;
                }
                --level;
                continue;
            }
            char32_t ch = c;
            if (c >= 0xD800 && c <= 0xDBFF && pos_ < s_.size() && s_[pos_] >= 0xDC00 && s_[pos_] <= 0xDFFF) {
                ch = 0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10U) + (s_[pos_] - 0xDC00);
                ++pos_;
            }
            text.push_back(ch == U'\n' || ch == U'\t' || ch == U'\r' ? U' ' : ch);
        }
        fail(open, "missing }");
        return std::nullopt;
    }

    /// The delimiter after \left or \right: a character, a delimiter command or `.`.
    std::optional<char32_t> delimiter(std::uint32_t at) {
        const Token t = next();
        if (t.kind == Token::Kind::Char) {
            switch (t.ch) {
                case U'.':
                    return char32_t{0};
                case U'(':
                case U')':
                case U'[':
                case U']':
                case U'|':
                case U'/':
                case U'<':
                case U'>':
                    return t.ch == U'<' ? U'⟨' : t.ch == U'>' ? U'⟩' : t.ch;
                default:
                    break;
            }
        } else if (t.kind == Token::Kind::Command) {
            for (const SymbolEntry& e : kSymbols) {
                if (e.name == t.name && (e.cls == AtomClass::Open || e.cls == AtomClass::Close || e.ch == U'|' ||
                                         e.ch == U'‖' || e.ch == U'↑' || e.ch == U'↓' || e.ch == U'↕')) {
                    return e.ch;
                }
            }
        }
        fail(at, "missing delimiter");
        return std::nullopt;
    }

    Node textNode(std::u32string text, AtomClass cls, bool limits) {
        Node n;
        n.kind = Node::Kind::Text;
        n.text = std::move(text);
        n.cls = cls;
        n.limits = limits;
        return n;
    }

    Node withVariant(Variant v, std::uint32_t at) {
        const Variant saved = variant_;
        variant_ = v;
        Node n = argument(at);
        variant_ = saved;
        return n;
    }

    Node logo(bool la) {
        // \TeX and \LaTeX as TeX draws them: kerns and a raised A, a lowered E.
        Node g;
        g.kind = Node::Kind::Group;
        const auto letter = [](char32_t c) { return symbol(c, AtomClass::Ord); };
        const auto kern = [](double em) {
            Node n;
            n.kind = Node::Kind::Space;
            n.amount = em;
            return n;
        };
        const auto raise = [](Node inner, double em, double scale) {
            Node n;
            n.kind = Node::Kind::Raise;
            n.amount = em;
            n.scale = scale;
            n.children.push_back(std::move(inner));
            return n;
        };
        if (la) {
            g.children.push_back(letter(U'L'));
            g.children.push_back(kern(-0.36));
            g.children.push_back(raise(letter(U'A'), 0.21, 0.7));
            g.children.push_back(kern(-0.15));
        }
        g.children.push_back(letter(U'T'));
        g.children.push_back(kern(-0.1667));
        g.children.push_back(raise(letter(U'E'), -0.2155, 1.0));
        g.children.push_back(kern(-0.125));
        g.children.push_back(letter(U'X'));
        return g;
    }

    /// What an environment looks like: its delimiters, column alignment and cell style.
    struct EnvironmentKind {
        char32_t left = 0;
        char32_t right = 0;
        char align = 'c';
        bool alternate = false;  ///< aligned: columns right, left, right, left ...
        bool displayCells = false;
        double rowGap = 0;
    };

    static std::optional<EnvironmentKind> environmentKind(const std::u32string& name) {
        struct Entry {
            std::u32string_view name;
            EnvironmentKind kind;
        };
        static constexpr double kJot = 0.3;  // amsmath's \jot (3pt at 10pt)
        static const std::array kEnvironments{
            Entry{U"matrix", {}},
            Entry{U"smallmatrix", {}},
            Entry{U"pmatrix", {U'(', U')'}},
            Entry{U"bmatrix", {U'[', U']'}},
            Entry{U"Bmatrix", {U'{', U'}'}},
            Entry{U"vmatrix", {U'|', U'|'}},
            Entry{U"Vmatrix", {U'‖', U'‖'}},
            Entry{U"cases", {U'{', 0, 'l'}},
            Entry{U"aligned", {0, 0, 'c', true, true, kJot}},
            Entry{U"align", {0, 0, 'c', true, true, kJot}},
            Entry{U"align*", {0, 0, 'c', true, true, kJot}},
            Entry{U"split", {0, 0, 'c', true, true, kJot}},
            Entry{U"gathered", {0, 0, 'c', false, true, kJot}},
            Entry{U"gather", {0, 0, 'c', false, true, kJot}},
            Entry{U"gather*", {0, 0, 'c', false, true, kJot}},
        };
        for (const Entry& e : kEnvironments) {
            if (e.name == name) {
                return e.kind;
            }
        }
        return std::nullopt;
    }

    /// The cell of an aligned block right of an `&`: amsmath puts an empty atom before it, so
    /// that `&=` keeps the space around the relation.
    static Node afterAlignment(Node cell) {
        Node withEmpty;
        withEmpty.kind = Node::Kind::Group;
        Node empty;
        empty.kind = Node::Kind::Group;
        withEmpty.children.push_back(std::move(empty));
        for (Node& c : cell.children) {
            withEmpty.children.push_back(std::move(c));
        }
        return withEmpty;
    }

    /// The cells up to `\end{name}`, row by row; empty if an error stopped it.
    std::vector<std::vector<Node>> rows(const std::u32string& name, const EnvironmentKind& kind, std::uint32_t begin) {
        std::vector<std::vector<Node>> out(1);
        while (!error_) {
            Node cell = list();
            if (kind.alternate && out.back().size() % 2 == 1) {
                cell = afterAlignment(std::move(cell));
            }
            out.back().push_back(std::move(cell));
            const Token t = next();
            if (t.kind == Token::Kind::Amp) {
                continue;
            }
            if (t.kind == Token::Kind::Command && t.name == u"\\") {
                (void)optionalArgument();  // \\[2pt]: extra space, not supported, read and dropped
                out.emplace_back();
                continue;
            }
            if (t.kind == Token::Kind::Command && t.name == u"end") {
                const std::optional<std::u32string> endName = rawArgument(t.position);
                if (endName && *endName != name) {
                    fail(t.position, "\\end does not match \\begin");
                }
                break;
            }
            fail(begin, "missing \\end");
        }
        // A last row that is empty (a `\\` before \end) is no row.
        if (out.size() > 1 && out.back().size() == 1 && out.back()[0].children.empty()) {
            out.pop_back();
        }
        return out;
    }

    Node environment(const Token& begin) {
        const std::optional<std::u32string> name = rawArgument(begin.position);
        if (!name) {
            return nullNode();
        }
        const std::optional<EnvironmentKind> kind = environmentKind(*name);
        if (!kind) {
            fail(begin.position, "unknown environment");
            return nullNode();
        }
        if (!enter(begin.position)) {
            return nullNode();
        }
        std::vector<std::vector<Node>> cells = rows(*name, *kind, begin.position);
        leave();
        if (error_) {
            return nullNode();
        }
        std::size_t columns = 0;
        for (const auto& r : cells) {
            columns = std::max(columns, r.size());
        }
        // Short rows are padded to the widest one. Every written cell takes at least one character
        // of the source, so a grid larger than the source is mostly padding: one wide row and many
        // short ones would grow quadratically (S044 code review O2). Real matrices stay far below.
        if (cells.size() > 0xFFFF || columns > 0xFFFF || cells.size() * columns > std::max<std::size_t>(s_.size(), 1)) {
            fail(begin.position, "matrix too large");
            return nullNode();
        }
        Node array;
        array.kind = Node::Kind::Array;
        array.displayCells = kind->displayCells;
        array.rowGap = kind->rowGap;
        array.rows = static_cast<std::uint16_t>(cells.size());
        array.columns = static_cast<std::uint16_t>(columns);
        for (std::size_t c = 0; c < columns; ++c) {
            array.align.push_back(kind->alternate ? (c % 2 == 0 ? 'r' : 'l') : kind->align);
        }
        array.columnGap = kind->alternate ? 2.0 : 1.0;  // \qquad between aligned pairs; 2 x \arraycolsep
        for (auto& r : cells) {
            r.resize(columns);
            for (Node& cell : r) {
                if (cell.kind == Node::Kind::Null) {
                    cell.kind = Node::Kind::Group;
                }
                array.children.push_back(std::move(cell));
            }
        }
        if (kind->left == 0 && kind->right == 0) {
            return array;
        }
        Node delimited;
        delimited.kind = Node::Kind::Delimited;
        delimited.cls = AtomClass::Inner;
        delimited.left = kind->left;
        delimited.right = kind->right;
        delimited.children.push_back(std::move(array));
        return delimited;
    }

    /// Commands that stand for one thing without arguments: symbols, large operators, operator
    /// names, spaces and ignored style switches (a Null node).
    std::optional<Node> simpleCommand(std::u16string_view name) {
        for (const SymbolEntry& e : kSymbols) {
            if (e.name == name) {
                return symbol(mapGreek(e.ch, variant_), e.cls, e.ch);
            }
        }
        for (const LargeOpEntry& e : kLargeOps) {
            if (e.name == name) {
                Node n;
                n.kind = Node::Kind::LargeOp;
                n.cls = AtomClass::Op;
                n.ch = e.ch;
                n.limits = e.limits;
                return n;
            }
        }
        for (const OperatorName& e : kOperatorNames) {
            if (e.name == name) {
                return textNode(std::u32string(e.name.begin(), e.name.end()), AtomClass::Op, e.limits);
            }
        }
        for (const SpaceEntry& e : kSpaces) {
            if (e.name == name) {
                Node n;
                n.kind = Node::Kind::Space;
                n.amount = e.em;
                return n;
            }
        }
        if (std::find(kIgnored.begin(), kIgnored.end(), name) != kIgnored.end()) {
            return nullNode();
        }
        return std::nullopt;
    }

    Node fraction(const Token& t) {
        if (!enter(t.position)) {
            return nullNode();
        }
        Node n;
        n.kind = Node::Kind::Fraction;
        n.children.push_back(argument(t.position));
        n.children.push_back(argument(t.position));
        leave();
        // \dfrac and \dbinom set the fraction in display style, \tfrac and \tbinom in text style,
        // wherever they stand (S044 code review O3).
        if (t.name.starts_with(u'd')) {
            n.forcedStyle = 0;
        } else if (t.name.starts_with(u't')) {
            n.forcedStyle = 1;
        }
        if (t.name.ends_with(u"binom")) {
            n.rule = false;
            n.left = U'(';
            n.right = U')';
        }
        return n;
    }

    Node radical(const Token& t) {
        if (!enter(t.position)) {
            return nullNode();
        }
        Node n;
        n.kind = Node::Kind::Radical;
        std::optional<Node> degree = optionalArgument();
        n.children.push_back(argument(t.position));
        n.children.push_back(degree ? std::move(*degree) : nullNode());
        leave();
        return n;
    }

    Node leftRight(const Token& t) {
        const std::optional<char32_t> left = delimiter(t.position);
        if (!left || !enter(t.position)) {
            return nullNode();
        }
        Node body = list();
        leave();
        const Token r = next();
        if (error_) {
            return nullNode();
        }
        if (r.kind != Token::Kind::Command || r.name != u"right") {
            fail(t.position, "\\left without \\right");
            return nullNode();
        }
        const std::optional<char32_t> right = delimiter(r.position);
        if (!right) {
            return nullNode();
        }
        Node n;
        n.kind = Node::Kind::Delimited;
        n.cls = AtomClass::Inner;
        n.left = *left;
        n.right = *right;
        n.children.push_back(std::move(body));
        return n;
    }

    Node xarrow(const Token& t) {
        if (!enter(t.position)) {
            return nullNode();
        }
        Node n;
        n.kind = Node::Kind::XArrow;
        n.cls = AtomClass::Rel;
        n.ch = t.name == u"xrightarrow" ? U'→' : U'←';
        std::optional<Node> below = optionalArgument();
        n.children.push_back(argument(t.position));
        n.children.push_back(below ? std::move(*below) : nullNode());
        leave();
        return n;
    }

    Node textCommand(const Token& t) {
        std::optional<std::u32string> text = rawArgument(t.position);
        if (!text) {
            return nullNode();
        }
        const bool op = t.name == u"operatorname";
        return textNode(std::move(*text), op ? AtomClass::Op : AtomClass::Ord, false);
    }

    Node logoCommand(const Token& t) {
        const Variant saved = variant_;
        variant_ = Variant::Roman;
        Node n = logo(t.name == u"LaTeX");
        variant_ = saved;
        return n;
    }

    Node command(const Token& t) {
        const std::u16string_view name = t.name;
        if (name.empty()) {
            fail(t.position, "\\ at the end");
            return nullNode();
        }
        if (std::optional<Node> simple = simpleCommand(name)) {
            return std::move(*simple);
        }
        using Handler = Node (Parser::*)(const Token&);
        struct Entry {
            std::u16string_view name;
            Handler handler;
        };
        static const std::array kCommands{
            Entry{u"frac", &Parser::fraction},
            Entry{u"dfrac", &Parser::fraction},
            Entry{u"tfrac", &Parser::fraction},
            Entry{u"binom", &Parser::fraction},
            Entry{u"dbinom", &Parser::fraction},
            Entry{u"tbinom", &Parser::fraction},
            Entry{u"sqrt", &Parser::radical},
            Entry{u"left", &Parser::leftRight},
            Entry{u"text", &Parser::textCommand},
            Entry{u"textrm", &Parser::textCommand},
            Entry{u"textnormal", &Parser::textCommand},
            Entry{u"mbox", &Parser::textCommand},
            Entry{u"operatorname", &Parser::textCommand},
            Entry{u"xrightarrow", &Parser::xarrow},
            Entry{u"xleftarrow", &Parser::xarrow},
            Entry{u"begin", &Parser::environment},
            Entry{u"LaTeX", &Parser::logoCommand},
            Entry{u"TeX", &Parser::logoCommand},
        };
        for (const Entry& e : kCommands) {
            if (e.name == name) {
                return (this->*e.handler)(t);
            }
        }
        struct VariantEntry {
            std::u16string_view name;
            Variant variant;
        };
        static constexpr std::array kVariants{
            VariantEntry{u"mathrm", Variant::Roman},    VariantEntry{u"rm", Variant::Roman},
            VariantEntry{u"mathup", Variant::Roman},    VariantEntry{u"mathbf", Variant::Bold},
            VariantEntry{u"boldsymbol", Variant::Bold}, VariantEntry{u"bm", Variant::Bold},
            VariantEntry{u"mathit", Variant::Italic},
        };
        for (const VariantEntry& e : kVariants) {
            if (e.name == name) {
                return withVariant(e.variant, t.position);
            }
        }
        std::string message = "unknown command \\";
        for (const char16_t c : name) {
            message.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        }
        fail(t.position, std::move(message));
        return nullNode();
    }
};

}  // namespace

ParseResult parse(std::u16string_view source) {
    return Parser(source).run();
}

}  // namespace hitokoto::core::math
