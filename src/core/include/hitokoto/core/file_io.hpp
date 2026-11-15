// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace hitokoto::core {

struct IoError {
    std::string message;
};

struct ReadResult {
    std::optional<std::string> bytes;
    std::optional<IoError> error;
};

/// The largest file readFile() reads unless told otherwise: 128 MiB. Opening a file takes about
/// 27 times its size in memory and about 0.3 s per MiB (S001, S007), so a file at this bound
/// already needs about 3.5 GB; larger files are refused with a message instead of reading them.
inline constexpr std::uintmax_t kMaxReadBytes = std::uintmax_t{128} * 1024 * 1024;

/// Reads a whole file as bytes.
///
/// Only regular files are read. What is not a regular file after following symbolic links (a
/// folder, a pipe, a device, a socket) is refused before it is opened, so that reading never
/// waits on a pipe. A file larger than `maxBytes` is refused, also when it grows while it is
/// read. Every failure, including running out of memory, is returned as an IoError with a message
/// for the user; nothing is thrown.
ReadResult readFile(const std::filesystem::path& path, std::uintmax_t maxBytes = kMaxReadBytes);

/// Test seam for the atomic writer.
struct AtomicWriteHooks {
    /// Called after the temporary file is complete and synced, before it replaces the target.
    /// Returning false aborts the write as if the process had died at that moment
    /// (the temporary file is left behind, the target is untouched).
    std::function<bool(const std::filesystem::path& tempFile)> beforeReplace;
};

/// Writes `bytes` to `target` atomically: a temporary file in the same directory is written
/// and synced, then replaces the target. If `target` is a symbolic link, the file it points to
/// is replaced. Permissions (POSIX) and attributes, ACL and streams (Windows) of an existing file
/// are kept. Files marked read-only are refused on every platform.
std::optional<IoError> writeFileAtomically(const std::filesystem::path& target, std::string_view bytes,
                                           const AtomicWriteHooks* hooks = nullptr);

}  // namespace hitokoto::core
