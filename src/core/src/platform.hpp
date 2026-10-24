// SPDX-License-Identifier: GPL-3.0-or-later
// Platform shim (justified in docs/architecture.md): the C++ standard library can neither flush a
// file to stable storage nor replace a file while keeping its metadata. This is the only
// OS-specific code in the core.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace hitokoto::core::platform {

/// Creates `path` exclusively (fails if it exists), writes `bytes` and flushes them to disk.
/// If `likeFile` is given (POSIX), the new file gets its permission bits and, where allowed, its
/// owner and group before any byte is written. Returns an error description on failure.
std::optional<std::string> writeAndSync(const std::filesystem::path& path, std::string_view bytes,
                                        const std::filesystem::path* likeFile);

/// Atomically replaces `target` with `temp` (rename on POSIX; ReplaceFileW on Windows, which keeps
/// the target's attributes, ACL and streams). Returns an error description on failure.
std::optional<std::string> replaceFile(const std::filesystem::path& temp, const std::filesystem::path& target);

/// True if the existing file is marked read-only (no owner write bit / read-only attribute).
bool isReadOnly(const std::filesystem::path& file) noexcept;

/// Best effort: makes a rename in `dir` durable (POSIX); no-op on Windows.
void syncDirectory(const std::filesystem::path& dir) noexcept;

}  // namespace hitokoto::core::platform
