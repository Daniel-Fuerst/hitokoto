// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/text_codec.hpp"

#include <algorithm>
#include <array>

namespace hitokoto::core {
namespace {

constexpr std::string_view kBom = "\xEF\xBB\xBF";

DecodeResult fail(std::size_t offset, std::string message) {
    DecodeResult r;
    r.error = DecodeError{offset, std::move(message)};
    return r;
}

void appendUtf16(std::u16string& out, char32_t cp) {
    if (cp < 0x10000) {
        out.push_back(static_cast<char16_t>(cp));
    } else {
        cp -= 0x10000;
        out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
        out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    }
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string_view endingBytes(LineEnding e) {
    switch (e) {
        case LineEnding::LF:
            return "\n";
        case LineEnding::CRLF:
            return "\r\n";
        case LineEnding::CR:
            return "\r";
    }
    return "\n";
}

}  // namespace

std::size_t countBreaks(std::u16string_view text) noexcept {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), u'\n'));
}

DecodeResult decodeUtf8(std::string_view bytes) {
    DecodedText out;
    std::size_t i = 0;
    if (bytes.substr(0, 3) == kBom) {
        out.hasBom = true;
        i = 3;
    }
    out.text.reserve(bytes.size());
    std::vector<LineEnding> endings;
    std::array<std::size_t, 3> counts{};

    const auto byteAt = [&](std::size_t k) { return static_cast<unsigned char>(bytes[k]); };

    while (i < bytes.size()) {
        const unsigned char b0 = byteAt(i);
        if (b0 < 0x80) {
            if (b0 == '\n' || b0 == '\r') {
                LineEnding e = LineEnding::LF;
                if (b0 == '\r') {
                    if (i + 1 < bytes.size() && bytes[i + 1] == '\n') {
                        e = LineEnding::CRLF;
                        ++i;
                    } else {
                        e = LineEnding::CR;
                    }
                }
                endings.push_back(e);
                ++counts[static_cast<std::size_t>(e)];
                out.text.push_back(u'\n');
            } else {
                out.text.push_back(static_cast<char16_t>(b0));
            }
            ++i;
            continue;
        }
        std::size_t len = 0;
        char32_t cp = 0;
        char32_t minimum = 0;
        if (b0 >= 0xC2 && b0 <= 0xDF) {
            len = 2;
            cp = b0 & 0x1Fu;
            minimum = 0x80;
        } else if (b0 >= 0xE0 && b0 <= 0xEF) {
            len = 3;
            cp = b0 & 0x0Fu;
            minimum = 0x800;
        } else if (b0 >= 0xF0 && b0 <= 0xF4) {
            len = 4;
            cp = b0 & 0x07u;
            minimum = 0x10000;
        } else {
            return fail(i, "invalid UTF-8 lead byte");
        }
        if (i + len > bytes.size()) {
            return fail(i, "truncated UTF-8 sequence");
        }
        for (std::size_t k = 1; k < len; ++k) {
            const unsigned char b = byteAt(i + k);
            if ((b & 0xC0u) != 0x80u) {
                return fail(i, "invalid UTF-8 continuation byte");
            }
            cp = (cp << 6) | (b & 0x3Fu);
        }
        if (cp < minimum) {
            return fail(i, "overlong UTF-8 sequence");
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return fail(i, "UTF-8 sequence encodes an invalid code point");
        }
        if (cp == 0x2029) {
            return fail(i, "U+2029 PARAGRAPH SEPARATOR is not supported");
        }
        if (cp == 0xFDD0 || cp == 0xFDD1) {
            // Qt uses these noncharacters as frame markers and would turn them into line breaks.
            return fail(i, "noncharacter U+FDD0/U+FDD1 is not supported");
        }
        appendUtf16(out.text, cp);
        i += len;
    }

    // Majority ending; ties go to the first ending seen.
    if (!endings.empty()) {
        LineEnding majority = endings.front();
        for (LineEnding e : {LineEnding::LF, LineEnding::CRLF, LineEnding::CR}) {
            if (counts[static_cast<std::size_t>(e)] > counts[static_cast<std::size_t>(majority)]) {
                majority = e;
            }
        }
        out.uniformEnding = majority;
        const bool mixed = std::any_of(endings.begin(), endings.end(), [&](LineEnding e) { return e != majority; });
        if (mixed) {
            out.perBreak = std::move(endings);
        }
    }

    DecodeResult r;
    r.value = std::move(out);
    return r;
}

std::optional<std::string> encodeUtf8(std::u16string_view text, bool withBom, LineEnding uniformEnding,
                                      std::span<const LineEnding> perBreak) {
    if (!perBreak.empty() && perBreak.size() != countBreaks(text)) {
        return std::nullopt;
    }
    std::string out;
    out.reserve(text.size() + text.size() / 4 + 3);
    if (withBom) {
        out.append(kBom);
    }
    std::size_t breakIndex = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char16_t c = text[i];
        if (c == u'\n') {
            out.append(endingBytes(perBreak.empty() ? uniformEnding : perBreak[breakIndex]));
            ++breakIndex;
            continue;
        }
        char32_t cp = c;
        if (c >= 0xD800 && c <= 0xDBFF) {
            if (i + 1 >= text.size() || text[i + 1] < 0xDC00 || text[i + 1] > 0xDFFF) {
                return std::nullopt;
            }
            cp = 0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10) + (static_cast<char32_t>(text[i + 1]) - 0xDC00);
            ++i;
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            return std::nullopt;
        }
        appendUtf8(out, cp);
    }
    return out;
}

}  // namespace hitokoto::core
