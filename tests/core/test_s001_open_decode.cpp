// SPDX-License-Identifier: GPL-3.0-or-later
// S001: decoding files for display.
#include "hitokoto/core/text_codec.hpp"

#include <gtest/gtest.h>

using namespace hitokoto::core;

namespace {
DecodedText ok(std::string_view bytes) {
    auto r = decodeUtf8(bytes);
    EXPECT_FALSE(r.error) << r.error->message;
    return r.value.value_or(DecodedText{});
}
std::size_t errorAt(std::string_view bytes) {
    auto r = decodeUtf8(bytes);
    EXPECT_TRUE(r.error);
    EXPECT_FALSE(r.value);
    return r.error ? r.error->byteOffset : static_cast<std::size_t>(-1);
}
}  // namespace

TEST(S001_Open, PlainAsciiWithLf) {
    const auto d = ok("# Title\n- item\n");
    EXPECT_EQ(d.text, u"# Title\n- item\n");
    EXPECT_FALSE(d.hasBom);
    EXPECT_EQ(d.uniformEnding, LineEnding::LF);
    EXPECT_TRUE(d.perBreak.empty());
}

TEST(S001_Open, EmptyFile) {
    const auto d = ok("");
    EXPECT_TRUE(d.text.empty());
    EXPECT_EQ(d.uniformEnding, LineEnding::LF);
}

TEST(S001_Open, BomIsRecordedAndStripped) {
    const auto d = ok("\xEF\xBB\xBFhi");
    EXPECT_TRUE(d.hasBom);
    EXPECT_EQ(d.text, u"hi");
}

TEST(S001_Open, CrlfBecomesNewlineAndIsRecorded) {
    const auto d = ok("a\r\nb\r\n");
    EXPECT_EQ(d.text, u"a\nb\n");
    EXPECT_EQ(d.uniformEnding, LineEnding::CRLF);
    EXPECT_TRUE(d.perBreak.empty());
}

TEST(S001_Open, LoneCrIsALineEnding) {
    const auto d = ok("a\rb");
    EXPECT_EQ(d.text, u"a\nb");
    EXPECT_EQ(d.uniformEnding, LineEnding::CR);
}

TEST(S001_Open, MixedEndingsArePerBreakWithMajority) {
    const auto d = ok("a\r\nb\nc\r\nd");
    EXPECT_EQ(d.text, u"a\nb\nc\nd");
    EXPECT_EQ(d.uniformEnding, LineEnding::CRLF);
    ASSERT_EQ(d.perBreak.size(), 3u);
    EXPECT_EQ(d.perBreak[1], LineEnding::LF);
}

TEST(S001_Open, NoTrailingNewlineIsKept) {
    EXPECT_EQ(ok("last").text, u"last");
}

TEST(S001_Open, MultibyteAndSupplementaryCharacters) {
    // a-umlaut, CJK "hitokoto", emoji (surrogate pair)
    const auto d = ok("\xC3\xA4 \xE4\xB8\x80\xE8\xA8\x80 \xF0\x9F\x98\x80");
    EXPECT_EQ(d.text, u"\u00E4 \u4E00\u8A00 \U0001F600");
}

TEST(S001_Open, InvalidUtf8IsRejectedWithPosition) {
    EXPECT_EQ(errorAt("ab\xFF"), 2u);            // invalid lead byte
    EXPECT_EQ(errorAt("ab\xC3"), 2u);            // truncated
    EXPECT_EQ(errorAt("\xC3\x28"), 0u);          // bad continuation
    EXPECT_EQ(errorAt("x\xC0\xAF"), 1u);         // overlong (lead byte C0 never valid)
    EXPECT_EQ(errorAt("\xE0\x80\xAF"), 0u);      // overlong 3-byte
    EXPECT_EQ(errorAt("\xED\xA0\x80"), 0u);      // UTF-16 surrogate
    EXPECT_EQ(errorAt("\xF4\x90\x80\x80"), 0u);  // above U+10FFFF
}

TEST(S001_Open, ParagraphSeparatorIsRejected) {
    const auto r = decodeUtf8(
        "a\xE2\x80\xA9"
        "b");
    ASSERT_TRUE(r.error);
    EXPECT_EQ(r.error->byteOffset, 1u);
    EXPECT_NE(r.error->message.find("U+2029"), std::string::npos);
}

TEST(S001_Open, QtFrameMarkersAreRejected) {
    EXPECT_EQ(errorAt("a\xEF\xB7\x90"), 1u);   // U+FDD0
    EXPECT_EQ(errorAt("ab\xEF\xB7\x91"), 2u);  // U+FDD1
    EXPECT_TRUE(decodeUtf8("\xEF\xB7\x92").value) << "other noncharacters are plain text";
}

TEST(S001_Open, LineSeparatorAndNulArePlainCharacters) {
    EXPECT_EQ(ok("a\xE2\x80\xA8"
                 "b")
                  .text,
              u"a\u2028b");
    EXPECT_EQ(ok(std::string_view("a\0b", 3)).text, std::u16string(u"a\0b", 3));
}
