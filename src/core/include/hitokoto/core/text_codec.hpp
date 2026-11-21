// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hitokoto::core {

/// How a line ends on disk.
enum class LineEnding : std::uint8_t { LF, CRLF, CR };

/// Decoded file content plus everything needed to write it back byte for byte.
///
/// `text` holds the lines joined with u'\n' (the editor's representation). The line
/// endings of the file are kept separately: one ending for all line breaks, or, for files
/// with mixed endings, one entry per line break.
struct DecodedText {
    std::u16string text;
    bool hasBom = false;
    LineEnding uniformEnding = LineEnding::LF;  ///< used for every break when perBreak is empty;
                                                ///< for mixed files: the majority ending
    std::vector<LineEnding> perBreak;           ///< non-empty only for mixed files
};

struct DecodeError {
    std::size_t byteOffset = 0;
    std::string message;
};

struct DecodeResult {
    std::optional<DecodedText> value;
    std::optional<DecodeError> error;
};

/// Decodes UTF-8 (with or without BOM). Invalid UTF-8 is rejected, never repaired.
/// U+2029 (PARAGRAPH SEPARATOR) and the noncharacters U+FDD0/U+FDD1 are rejected as well,
/// because the editor would turn them into line breaks and the file could not be saved back
/// unchanged.
DecodeResult decodeUtf8(std::string_view bytes);

/// Encodes text whose lines are joined with u'\n'. `perBreak`, if not empty, must hold one
/// ending per u'\n' in `text`; otherwise `uniformEnding` is used for every break.
/// Returns std::nullopt if the input is not valid UTF-16 or `perBreak` has the wrong size.
std::optional<std::string> encodeUtf8(std::u16string_view text, bool withBom, LineEnding uniformEnding,
                                      std::span<const LineEnding> perBreak = {});

/// Number of u'\n' characters in `text`.
std::size_t countBreaks(std::u16string_view text) noexcept;

/// U+FFFD REPLACEMENT CHARACTER: what text entering the editor holds in place of a lone
/// surrogate.
inline constexpr char16_t kReplacementCharacter = u'�';

/// Offset of the first lone surrogate in `text`, or std::u16string_view::npos. A lone surrogate
/// is a high surrogate not followed by a low one, or a low surrogate not preceded by a high one:
/// half of a character, which UTF-8 cannot encode (encodeUtf8 refuses it).
std::size_t findLoneSurrogate(std::u16string_view text) noexcept;

/// `text` with each lone surrogate replaced by U+FFFD; pairs and every other code unit stay, so
/// the length stays the same. Every way into the editor's text passes what it inserts through
/// this, so that the text can always be saved (S004).
///
/// The text is judged on its own: a half at its start or end is replaced, also when the text it
/// goes next to ends or starts with the other half. Next to an insertion there is no half to
/// complete: the document holds none (decodeUtf8 refuses one, and everything inserted passes
/// here), and the caret and a selection never stand inside a pair (Qt moves and hits by whole
/// characters, find matches whole code points).
std::u16string replaceLoneSurrogates(std::u16string_view text);

/// A place in a text, both counted from 1: the line (lines end at u'\n') and the column in code
/// points (a surrogate pair is one column, a lone surrogate one).
struct TextPlace {
    std::size_t line = 1;
    std::size_t column = 1;
};

/// The line and column of `offset` in `text`; an offset past the end counts as the end.
TextPlace placeOf(std::u16string_view text, std::size_t offset) noexcept;

}  // namespace hitokoto::core
