// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/file_io.hpp"

#include "platform.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <new>
#include <random>
#include <string>
#include <system_error>
#include <thread>

namespace hitokoto::core {
namespace fs = std::filesystem;

namespace {

fs::path resolveTarget(const fs::path& target) {
    std::error_code ec;
    if (fs::is_symlink(fs::symlink_status(target, ec))) {
        const fs::path resolved = fs::weakly_canonical(target, ec);
        if (!ec) {
            return resolved;
        }
    }
    return target;
}

fs::path temporarySibling(const fs::path& target) {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    const auto token = rng();
    char buf[24];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(token));
    fs::path name = target.filename();
    name += ".hitokoto-";
    name += buf;
    name += ".tmp";
    return target.parent_path() / (fs::path(".") += name);
}

ReadResult failure(const char* message) {
    ReadResult r;
    r.error = IoError{message};
    return r;
}

ReadResult tooLarge(std::uintmax_t maxBytes) {
    ReadResult r;
    constexpr std::uintmax_t kMiB = std::uintmax_t{1024} * 1024;
    const std::uintmax_t mib = maxBytes / kMiB;
    r.error = IoError{mib > 0 && mib * kMiB == maxBytes
                          ? "the file is too large to open (more than " + std::to_string(mib) + " MiB)"
                          : "the file is too large to open (more than " + std::to_string(maxBytes) + " bytes)"};
    return r;
}

ReadResult readRegularFile(const fs::path& path, std::uintmax_t maxBytes) {
    // What the path names after following symbolic links decides, before anything is opened:
    // opening a pipe blocks until a writer comes, reading a folder fails deep inside the stream
    // and a device may never end.
    std::error_code ec;
    const fs::file_status status = fs::status(path, ec);
    if (status.type() == fs::file_type::not_found) {
        return failure("file does not exist");
    }
    if (ec) {
        return failure("cannot read file");
    }
    if (fs::is_directory(status)) {
        return failure("this is a folder, not a file");
    }
    if (!fs::is_regular_file(status)) {
        return failure("this is not a regular file (it is a device, a pipe or a socket)");
    }
    const std::uintmax_t size = fs::file_size(path, ec);
    if (!ec && size > maxBytes) {
        return tooLarge(maxBytes);
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return failure("cannot read file");
    }
    std::string bytes;
    if (!ec) {
        bytes.resize(static_cast<std::size_t>(size));
        in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        bytes.resize(static_cast<std::size_t>(in.gcount()));
    }
    // The file may have grown since file_size(), or reported no size at all (files under /proc):
    // read whatever is left, but never more than the bound.
    char extra[4096];
    while (in.read(extra, sizeof extra) || in.gcount() > 0) {
        if (static_cast<std::uintmax_t>(in.gcount()) > maxBytes - bytes.size()) {
            return tooLarge(maxBytes);
        }
        bytes.append(extra, static_cast<std::size_t>(in.gcount()));
    }
    if (in.bad()) {
        return failure("error while reading file");
    }
    ReadResult r;
    r.bytes = std::move(bytes);
    return r;
}

}  // namespace

ReadResult readFile(const fs::path& path, std::uintmax_t maxBytes) {
    // Nothing thrown here may reach the caller: opening runs on a worker thread, where an
    // exception ends the program (Qt's thread pool does not catch).
    try {
        return readRegularFile(path, maxBytes);
    } catch (const std::bad_alloc&) {
        return failure("not enough memory to read the file");
    } catch (...) {
        return failure("error while reading file");
    }
}

std::optional<IoError> writeFileAtomically(const fs::path& requestedTarget, std::string_view bytes,
                                           const AtomicWriteHooks* hooks) {
    const fs::path target = resolveTarget(requestedTarget);
    std::error_code ec;
    const bool exists = fs::exists(target, ec);
    if (exists && platform::isReadOnly(target)) {
        return IoError{"the file is read-only"};
    }
    const fs::path temp = temporarySibling(target);

    if (auto err = platform::writeAndSync(temp, bytes, exists ? &target : nullptr)) {
        std::error_code ignore;
        fs::remove(temp, ignore);
        return IoError{"cannot write temporary file: " + *err};
    }

    if (hooks && hooks->beforeReplace && !hooks->beforeReplace(temp)) {
        return IoError{"write aborted before replacing the target"};
    }

    // Windows: virus scanners, indexers or sync clients may hold the target briefly.
    std::optional<std::string> err;
    std::chrono::milliseconds wait{50};
    for (int attempt = 0; attempt < 5; ++attempt) {
        err = platform::replaceFile(temp, target);
        if (!err) {
            break;
        }
        std::this_thread::sleep_for(wait);
        wait *= 2;
    }
    if (err) {
        std::error_code ignore;
        fs::remove(temp, ignore);
        return IoError{"cannot replace file: " + *err};
    }
    platform::syncDirectory(target.parent_path());
    return std::nullopt;
}

}  // namespace hitokoto::core
