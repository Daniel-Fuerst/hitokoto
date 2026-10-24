// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/file_io.hpp"

#include "platform.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
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

}  // namespace

ReadResult readFile(const fs::path& path) {
    ReadResult r;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::error_code ec;
        const bool exists = fs::exists(path, ec);
        r.error = IoError{exists ? "cannot read file" : "file does not exist"};
        return r;
    }
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    std::string bytes;
    if (!ec) {
        bytes.resize(static_cast<std::size_t>(size));
        in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        bytes.resize(static_cast<std::size_t>(in.gcount()));
        // The file may have grown since file_size(); read whatever is left.
        char extra[4096];
        while (in.read(extra, sizeof extra) || in.gcount() > 0) {
            bytes.append(extra, static_cast<std::size_t>(in.gcount()));
        }
    } else {
        bytes.assign(std::istreambuf_iterator<char>(in), {});
    }
    if (in.bad()) {
        r.error = IoError{"error while reading file"};
        return r;
    }
    r.bytes = std::move(bytes);
    return r;
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
