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

}  // namespace hitokoto::core
