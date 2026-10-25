// SPDX-License-Identifier: GPL-3.0-or-later
// S004: saving writes exactly the Markdown text, byte for byte, atomically.
#include "hitokoto/core/file_io.hpp"
#include "hitokoto/core/text_codec.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>

using namespace hitokoto::core;
namespace fs = std::filesystem;

namespace {

std::string roundTrip(std::string_view bytes) {
    const auto d = decodeUtf8(bytes);
    if (!d.value) {
        // Guarded for bugprone-unchecked-optional-access; decodeUtf8 sets the error whenever the
        // value is empty, but the checker cannot know that.
        ADD_FAILURE() << "decode failed: " << (d.error ? d.error->message : std::string("no error reported"));
        return {};
    }
    const auto e = encodeUtf8(d.value->text, d.value->hasBom, d.value->uniformEnding, d.value->perBreak);
    if (!e) {
        ADD_FAILURE() << "encode failed";
        return {};
    }
    return *e;
}

class TempDir {
public:
    TempDir() {
        std::random_device rd;
        path_ = fs::temp_directory_path() / ("hitokoto-test-" + std::to_string(rd()) + std::to_string(rd()));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

void spit(const fs::path& p, std::string_view bytes) {
    std::ofstream out(p, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::size_t countEntries(const fs::path& dir) {
    return static_cast<std::size_t>(std::distance(fs::directory_iterator(dir), fs::directory_iterator()));
}

}  // namespace

TEST(S004_Save, UnchangedFilesRoundTripByteForByte) {
    for (std::string_view sample : {
             std::string_view(""),
             std::string_view("plain"),
             std::string_view("# T\n- a\n"),
             std::string_view("a\r\nb\r\n"),
             std::string_view("\xEF\xBB\xBF# BOM\r\n\r\n- x"),
             std::string_view("cr\rmac\r"),
             std::string_view("mixed\r\nlf\ncr\rend"),
             std::string_view("\xF0\x9F\x98\x80 emoji\n"),
             std::string_view("\n\n\n"),
         }) {
        EXPECT_EQ(roundTrip(sample), sample);
    }
}

TEST(S004_Save, EditedFileKeepsBomAndCrlf) {
    const auto decoded = decodeUtf8("\xEF\xBB\xBF# T\r\nx\r\n");
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!decoded.value) {
        FAIL() << "decode failed";
    }
    const auto& d = *decoded.value;
    const std::u16string edited = d.text + u"added\n";
    const auto encoded = encodeUtf8(edited, d.hasBom, d.uniformEnding, {});
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!encoded) {
        FAIL() << "encode failed";
    }
    EXPECT_EQ(*encoded, "\xEF\xBB\xBF# T\r\nx\r\nadded\r\n");
}

TEST(S004_Save, PerBreakSizeMismatchIsRejected) {
    const LineEnding one[] = {LineEnding::LF};
    EXPECT_FALSE(encodeUtf8(u"a\nb\nc", false, LineEnding::LF, one));
}

TEST(S004_Save, UnpairedSurrogateIsRejected) {
    const char16_t bad[] = {u'a', static_cast<char16_t>(0xD800), u'b'};
    EXPECT_FALSE(encodeUtf8(std::u16string_view(bad, 3), false, LineEnding::LF));
}

// Property: every valid UTF-8 byte string decodes and encodes back unchanged.
TEST(S004_Save, PropertyRandomValidUtf8RoundTrips) {
    std::mt19937 rng(20261024);
    const char32_t pool[] = {U'a',  U'#',      U'-',      U' ',      U'\t',         U'\n',
                             U'\r', U'\u00E4', U'\u2028', U'\u4E00', U'\U0001F600', 0};
    for (int iteration = 0; iteration < 3000; ++iteration) {
        std::u32string cps;
        const int len = static_cast<int>(rng() % 40);
        for (int k = 0; k < len; ++k) {
            cps.push_back(pool[rng() % std::size(pool)]);
        }
        std::string bytes = (rng() % 4 == 0) ? "\xEF\xBB\xBF" : "";
        for (char32_t cp : cps) {
            if (cp < 0x80) {
                bytes.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                bytes.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                bytes.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp < 0x10000) {
                bytes.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                bytes.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                bytes.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                bytes.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                bytes.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                bytes.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                bytes.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }
        ASSERT_EQ(roundTrip(bytes), bytes) << "iteration " << iteration;
    }
}

// Property: arbitrary bytes either decode and round-trip, or are rejected; never anything else.
TEST(S004_Save, PropertyArbitraryBytesNeverCorrupt) {
    std::mt19937 rng(7);
    for (int iteration = 0; iteration < 20000; ++iteration) {
        std::string bytes(rng() % 12, '\0');
        for (auto& b : bytes) {
            b = static_cast<char>(rng() % 256);
        }
        const auto d = decodeUtf8(bytes);
        ASSERT_NE(d.value.has_value(), d.error.has_value());
        if (d.value) {
            const auto encoded = encodeUtf8(d.value->text, d.value->hasBom, d.value->uniformEnding, d.value->perBreak);
            // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
            if (!encoded) {
                FAIL() << "encode failed";
            }
            ASSERT_EQ(*encoded, bytes);
        }
    }
}

TEST(S004_Save, AtomicWriteCreatesAndReplaces) {
    TempDir dir;
    const auto file = dir.path() / "notes.md";
    ASSERT_FALSE(writeFileAtomically(file, "first"));
    EXPECT_EQ(slurp(file), "first");
    ASSERT_FALSE(writeFileAtomically(file, "second"));
    EXPECT_EQ(slurp(file), "second");
    EXPECT_EQ(countEntries(dir.path()), 1u) << "no temporary file may be left behind";
}

TEST(S004_Save, InterruptedWriteLeavesOriginalIntact) {
    TempDir dir;
    const auto file = dir.path() / "notes.md";
    spit(file, "original");
    fs::path seenTemp;
    AtomicWriteHooks hooks;
    hooks.beforeReplace = [&](const fs::path& temp) {
        seenTemp = temp;
        return false;  // simulate a crash between write and rename
    };
    EXPECT_TRUE(writeFileAtomically(file, "new content", &hooks));
    EXPECT_EQ(slurp(file), "original");
    EXPECT_EQ(seenTemp.parent_path(), file.parent_path()) << "temp file must be in the same directory";
    EXPECT_EQ(slurp(seenTemp), "new content");
}

TEST(S004_Save, ReadOnlyFilesAreRefusedAndUnchanged) {
    TempDir dir;
    const auto file = dir.path() / "locked.md";
    spit(file, "original");
    fs::permissions(file, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
                    fs::perm_options::replace);
    const auto err = writeFileAtomically(file, "new");
    fs::permissions(file, fs::perms::owner_all, fs::perm_options::add);  // allow cleanup
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!err) {
        FAIL() << "expected an error";
    }
    EXPECT_NE(err->message.find("read-only"), std::string::npos);
    EXPECT_EQ(slurp(file), "original");
    EXPECT_EQ(countEntries(dir.path()), 1u);
}

TEST(S004_Save, ReadFileReturnsBytesOrError) {
    TempDir dir;
    const auto file = dir.path() / "a.md";
    spit(file, std::string_view("x\0y\r\n", 5));
    const auto read = readFile(file);
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!read.bytes) {
        FAIL() << "read failed";
    }
    EXPECT_EQ(*read.bytes, std::string_view("x\0y\r\n", 5));
    const auto missing = readFile(dir.path() / "missing.md");
    EXPECT_FALSE(missing.bytes);
    EXPECT_TRUE(missing.error);
}

#ifndef _WIN32
TEST(S004_Save, PermissionsAndSymlinkTargetArePreserved) {
    TempDir dir;
    const auto real = dir.path() / "real.md";
    const auto link = dir.path() / "link.md";
    spit(real, "old");
    fs::permissions(real, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
    fs::create_symlink(real, link);
    ASSERT_FALSE(writeFileAtomically(link, "new"));
    EXPECT_TRUE(fs::is_symlink(link));
    EXPECT_EQ(slurp(real), "new");
    EXPECT_EQ(fs::status(real).permissions() & fs::perms::all, fs::perms::owner_read | fs::perms::owner_write);
}
#endif
