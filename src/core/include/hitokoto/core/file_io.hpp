// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

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

/// Reads a whole file as bytes.
ReadResult readFile(const std::filesystem::path& path);

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
