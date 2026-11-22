// SPDX-License-Identifier: GPL-3.0-or-later
// The bits of QTextBlock::userState(): what BlockStyler and BlockLayout know about a block.
#pragma once

#include <array>
#include <bit>

namespace hitokoto::ui {

// QTextBlock::userState() is an int; Qt gives a new block -1, which counts as nothing known. Bit 31
// (the sign) stays clear, so bits 0 to 30 are all there is. A flag that does not fit goes into
// BlockData (block_data.hpp) or needs a second word.
//
// Bits 0-1    style: what the block was styled for (kUnstyled, kStyledConcealed, kStyledRevealed,
//             kStyledParagraphRevealed)
// Bit 2       the block state after the line is known
// Bit 3       that state is active; only then does the block carry a BlockData holding it, so
//             plain lines need no allocation
// Bits 4-19   the paragraph of that state (core::BlockState::paragraph; S023, ADR 0006)
// Bit 20      the setext level of the line's paragraph is known
// Bits 21-22  that level, or the one the line was last shown with
// Bit 23      the line's link to the paragraph above is known
// Bits 24-25  that link (none, continues, underline of level 1 or 2)
// Bits 26-27  columns of the hanging indent of continuation lines (S003); 0 = none, 3 = more,
//             kept in BlockData::wrapIndent
// Bit 28      the line flows into the next one (soft line break, S033)
// Bit 29      a hard line break ends the line (S033)
// Bit 30      the line is laid out on the line of the one before it (S033)
//
// BlockStyler sets bits 0 to 25, BlockLayout bits 26 to 30 (and reads the style). Caching a new
// block state replaces kCachedStateBits and keeps every other bit.
constexpr int kStyleMask = 0x3;
constexpr int kUnstyled = 0;
constexpr int kStyledConcealed = 1;
constexpr int kStyledRevealed = 2;
constexpr int kStyledParagraphRevealed = 3;
constexpr int kStateKnown = 1 << 2;
constexpr int kStateActive = 1 << 3;
constexpr int kParagraphShift = 4;
constexpr int kParagraphMask = 0xFFFF << kParagraphShift;
constexpr int kLevelKnown = 1 << 20;
constexpr int kLevelShift = 21;
constexpr int kLevelMask = 0x3 << kLevelShift;
constexpr int kLinkKnown = 1 << 23;
constexpr int kLinkShift = 24;
constexpr int kLinkMask = 0x3 << kLinkShift;
constexpr int kLinkContinues = 1;
constexpr int kWrapIndentShift = 26;
constexpr int kWrapIndentMax = 3;
constexpr int kWrapIndentMask = kWrapIndentMax << kWrapIndentShift;
constexpr int kFlowsToNextBit = 1 << 28;
constexpr int kHardBreakBit = 1 << 29;
constexpr int kSharesPrevLineBit = 1 << 30;

/// The bits that hold the block state after the line.
constexpr int kCachedStateBits = kStateKnown | kStateActive | kParagraphMask;

/// Every field above; a new one is added here, so that the checks below cover it.
constexpr std::array kUserStateFields{kStyleMask,      kStateKnown,     kStateActive,  kParagraphMask,
                                      kLevelKnown,     kLevelMask,      kLinkKnown,    kLinkMask,
                                      kWrapIndentMask, kFlowsToNextBit, kHardBreakBit, kSharesPrevLineBit};

namespace detail {
constexpr bool userStateFieldsAreDisjointAndPositive() {
    unsigned all = 0;
    int bits = 0;
    for (const int field : kUserStateFields) {
        if (field <= 0) {
            return false;
        }
        all |= static_cast<unsigned>(field);
        bits += std::popcount(static_cast<unsigned>(field));
    }
    return std::popcount(all) == bits && (all & 0x80000000U) == 0;
}
}  // namespace detail

static_assert(detail::userStateFieldsAreDisjointAndPositive(),
              "the fields of QTextBlock::userState() overlap or reach the sign bit");

}  // namespace hitokoto::ui
