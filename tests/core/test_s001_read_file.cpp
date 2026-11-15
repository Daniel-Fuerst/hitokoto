// SPDX-License-Identifier: GPL-3.0-or-later
// S001: reading a file for opening. Only regular files are read, up to a bound, and every failure
// is a message, never an exception.
#include "hitokoto/core/file_io.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace hitokoto::core;
namespace fs = std::filesystem;

namespace {

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

void spit(const fs::path& p, std::string_view bytes) {
    std::ofstream out(p, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/// The message of a refused read, or a failure if the read succeeded.
std::string refusal(const ReadResult& r) {
    EXPECT_FALSE(r.bytes) << "read " << (r.bytes ? r.bytes->size() : 0) << " bytes";
    if (!r.error) {
        ADD_FAILURE() << "no error reported";
        return {};
    }
    EXPECT_FALSE(r.error->message.empty());
    return r.error->message;
}

/// readFile() with exceptions turned into a failure, so that a thrown exception names the case.
ReadResult readCaught(const fs::path& p, std::uintmax_t maxBytes = kMaxReadBytes) {
    try {
        return readFile(p, maxBytes);
    } catch (const std::exception& e) {
        ADD_FAILURE() << "readFile threw: " << e.what();
    } catch (...) {
        ADD_FAILURE() << "readFile threw a non-standard exception";
    }
    return {};
}

}  // namespace

TEST(S001_ReadFile, RegularFileIsReadByteForByte) {
    TempDir dir;
    const auto file = dir.path() / "notes.md";
    spit(file, std::string_view("# a\r\nx\0y\n", 9));
    const auto r = readCaught(file);
    // Explicit branch for bugprone-unchecked-optional-access (it ignores ASSERT_TRUE and flags value()).
    if (!r.bytes) {
        FAIL() << "read failed: " << (r.error ? r.error->message : std::string("no error reported"));
    }
    EXPECT_EQ(*r.bytes, std::string_view("# a\r\nx\0y\n", 9));
    EXPECT_FALSE(r.error);
}

TEST(S001_ReadFile, EmptyFileIsReadAsNoBytes) {
    TempDir dir;
    const auto file = dir.path() / "empty.md";
    spit(file, "");
    const auto r = readCaught(file);
    if (!r.bytes) {
        FAIL() << "read failed: " << (r.error ? r.error->message : std::string("no error reported"));
    }
    EXPECT_TRUE(r.bytes->empty());
    EXPECT_FALSE(r.error);
}

TEST(S001_ReadFile, MissingFileIsReported) {
    TempDir dir;
    EXPECT_NE(refusal(readCaught(dir.path() / "missing.md")).find("does not exist"), std::string::npos);
}

TEST(S001_ReadFile, FolderIsRefused) {
    TempDir dir;
    const auto folder = dir.path() / "Documents";
    fs::create_directory(folder);
    EXPECT_NE(refusal(readCaught(folder)).find("folder"), std::string::npos);
}

TEST(S001_ReadFile, LinkToAFolderIsRefused) {
    TempDir dir;
    const auto folder = dir.path() / "Documents";
    const auto link = dir.path() / "link.md";
    fs::create_directory(folder);
    std::error_code ec;
    fs::create_directory_symlink(folder, link, ec);
    if (ec) {
        GTEST_SKIP() << "cannot create a symbolic link here: " << ec.message();
    }
    EXPECT_NE(refusal(readCaught(link)).find("folder"), std::string::npos);
}

TEST(S001_ReadFile, LinkToAFileIsRead) {
    TempDir dir;
    const auto file = dir.path() / "real.md";
    const auto link = dir.path() / "link.md";
    spit(file, "text");
    std::error_code ec;
    fs::create_symlink(file, link, ec);
    if (ec) {
        GTEST_SKIP() << "cannot create a symbolic link here: " << ec.message();
    }
    const auto r = readCaught(link);
    if (!r.bytes) {
        FAIL() << "read failed: " << (r.error ? r.error->message : std::string("no error reported"));
    }
    EXPECT_EQ(*r.bytes, "text");
}

TEST(S001_ReadFile, FileLargerThanTheBoundIsRefusedWithoutReadingIt) {
    TempDir dir;
    const auto file = dir.path() / "huge.md";
    spit(file, "");
    // Sparse where the file system allows it: no data is written.
    fs::resize_file(file, kMaxReadBytes + 1);
    const auto started = std::chrono::steady_clock::now();
    const std::string message = refusal(readCaught(file));
    EXPECT_NE(message.find("too large"), std::string::npos) << message;
    EXPECT_NE(message.find("128 MiB"), std::string::npos) << message;
    // Refused from its size, not after reading 128 MiB.
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
}

TEST(S001_ReadFile, FileAtTheBoundIsReadAndOneByteMoreIsRefused) {
    TempDir dir;
    const auto file = dir.path() / "notes.md";
    spit(file, std::string(1000, 'x'));
    const auto atBound = readCaught(file, 1000);
    if (!atBound.bytes) {
        FAIL() << "read failed: " << (atBound.error ? atBound.error->message : std::string("no error reported"));
    }
    EXPECT_EQ(atBound.bytes->size(), 1000u);
    EXPECT_NE(refusal(readCaught(file, 999)).find("too large"), std::string::npos);
}

#ifdef __linux__
// Files under /proc report a size of 0 and yield their content when read: like a file that grows
// between the size check and reading, the bound has to hold while reading.
TEST(S001_ReadFile, BoundHoldsWhileReading) {
    ASSERT_EQ(fs::file_size("/proc/self/status"), 0u);
    EXPECT_NE(refusal(readCaught("/proc/self/status", 16)).find("too large"), std::string::npos);
    const auto whole = readCaught("/proc/self/status");
    if (!whole.bytes) {
        FAIL() << "read failed: " << (whole.error ? whole.error->message : std::string("no error reported"));
    }
    EXPECT_GT(whole.bytes->size(), 16u);
}
#endif

#ifndef _WIN32
TEST(S001_ReadFile, PipeIsRefusedWithoutWaiting) {
    TempDir dir;
    const auto fifo = dir.path() / "pipe.md";
    ASSERT_EQ(mkfifo(fifo.c_str(), 0600), 0);
    auto read = std::async(std::launch::async, [&] { return readCaught(fifo); });
    if (read.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        // Release the blocked reader: a writer that opens and closes the pipe ends the read.
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            close(fd);
        }
        (void)read.get();
        FAIL() << "readFile waited on a pipe";
    }
    EXPECT_NE(refusal(read.get()).find("not a regular file"), std::string::npos);
}

TEST(S001_ReadFile, DeviceIsRefused) {
    EXPECT_NE(refusal(readCaught("/dev/null")).find("not a regular file"), std::string::npos);
}
#endif
