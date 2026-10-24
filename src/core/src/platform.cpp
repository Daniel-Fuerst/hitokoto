// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace hitokoto::core::platform {

#ifdef _WIN32

namespace {
std::string lastError(const char* what) {
    return std::string(what) + " failed (Windows error " + std::to_string(GetLastError()) + ")";
}
}  // namespace

std::optional<std::string> writeAndSync(const std::filesystem::path& path, std::string_view bytes,
                                        const std::filesystem::path* /*likeFile*/) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return lastError("CreateFileW");
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - done, std::size_t{1} << 30));
        DWORD written = 0;
        if (!WriteFile(h, bytes.data() + done, chunk, &written, nullptr)) {
            auto err = lastError("WriteFile");
            CloseHandle(h);
            return err;
        }
        done += written;
    }
    if (!FlushFileBuffers(h)) {
        auto err = lastError("FlushFileBuffers");
        CloseHandle(h);
        return err;
    }
    CloseHandle(h);
    return std::nullopt;
}

std::optional<std::string> replaceFile(const std::filesystem::path& temp, const std::filesystem::path& target) {
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (ReplaceFileW(target.c_str(), temp.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
            return std::nullopt;
        }
        return lastError("ReplaceFileW");
    }
    if (MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
        return std::nullopt;
    }
    return lastError("MoveFileExW");
}

bool isReadOnly(const std::filesystem::path& file) noexcept {
    const DWORD attributes = GetFileAttributesW(file.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0;
}

void syncDirectory(const std::filesystem::path&) noexcept {}

#else

namespace {
std::string errnoText(const char* what, int err) {
    return std::string(what) + " failed: " + std::strerror(err);
}

int syncToDisk(int fd) {
#ifdef __APPLE__
    // fsync on macOS does not force the drive to flush its cache.
    if (::fcntl(fd, F_FULLFSYNC) == 0) {
        return 0;
    }
#endif
    return ::fsync(fd);
}
}  // namespace

std::optional<std::string> writeAndSync(const std::filesystem::path& path, std::string_view bytes,
                                        const std::filesystem::path* likeFile) {
    struct stat original {};
    const bool haveOriginal = likeFile != nullptr && ::stat(likeFile->c_str(), &original) == 0;
    // Private until the original's permissions are applied, so contents never leak.
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, haveOriginal ? 0600 : 0666);
    if (fd < 0) {
        return errnoText("open", errno);
    }
    if (haveOriginal) {
        // Owner and group only succeed with sufficient rights; keeping the mode matters most.
        if (::fchown(fd, original.st_uid, original.st_gid) != 0) {
            // Not fatal: changing the owner needs privileges; the mode below is what matters.
        }
        if (::fchmod(fd, original.st_mode & 07777) != 0) {
            const int err = errno;
            ::close(fd);
            return errnoText("fchmod", err);
        }
    }
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t n = ::write(fd, bytes.data() + done, bytes.size() - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int err = errno;
            ::close(fd);
            return errnoText("write", err);
        }
        done += static_cast<std::size_t>(n);
    }
    if (syncToDisk(fd) != 0) {
        const int err = errno;
        ::close(fd);
        return errnoText("fsync", err);
    }
    if (::close(fd) != 0) {
        return errnoText("close", errno);
    }
    return std::nullopt;
}

std::optional<std::string> replaceFile(const std::filesystem::path& temp, const std::filesystem::path& target) {
    if (::rename(temp.c_str(), target.c_str()) != 0) {
        return errnoText("rename", errno);
    }
    return std::nullopt;
}

bool isReadOnly(const std::filesystem::path& file) noexcept {
    struct stat st {};
    return ::stat(file.c_str(), &st) == 0 && (st.st_mode & S_IWUSR) == 0;
}

void syncDirectory(const std::filesystem::path& dir) noexcept {
    const std::filesystem::path d = dir.empty() ? std::filesystem::path(".") : dir;
    const int fd = ::open(d.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        (void)syncToDisk(fd);
        ::close(fd);
    }
}

#endif

}  // namespace hitokoto::core::platform
