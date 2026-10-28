// SPDX-License-Identifier: GPL-3.0-or-later
#include "hitokoto/core/height_index.hpp"

#include <algorithm>
#include <iterator>

namespace hitokoto::core {
namespace {

/// Free slots kept at the edit position after a rebuild: enough for many new lines in a row.
std::size_t gapFor(std::size_t size) {
    return std::max<std::size_t>(1024, size / 32);
}

std::size_t lowestBit(std::size_t i) {
    return i & (~i + 1);
}

}  // namespace

void HeightIndex::assign(const std::vector<double>& heights) {
    build(heights, heights.size(), gapFor(heights.size()));
}

double HeightIndex::height(std::size_t index) const {
    return index < size_ ? slots_[slotOf(index)] : 0.0;
}

void HeightIndex::set(std::size_t index, double height) {
    if (index >= size_) {
        return;
    }
    const std::size_t slot = slotOf(index);
    const double delta = height - slots_[slot];
    if (delta == 0) {
        return;
    }
    slots_[slot] = height;
    add(slot, delta);
    total_ += delta;
}

void HeightIndex::insert(std::size_t index, std::size_t count, double height) {
    index = std::min(index, size_);
    if (count == 0) {
        return;
    }
    if (count > gapLength_) {
        // Grow by a share of the size, like a vector, so a run of new lines rebuilds rarely.
        rebuild(index, count + std::max(gapFor(size_ + count), (size_ + count) / 4));
    } else {
        moveGap(index);
    }
    for (std::size_t i = 0; i < count; ++i) {
        slots_[gapStart_] = height;
        if (height != 0) {
            add(gapStart_, height);
        }
        ++gapStart_;
        --gapLength_;
    }
    size_ += count;
    total_ += height * static_cast<double>(count);
}

void HeightIndex::erase(std::size_t index, std::size_t count) {
    index = std::min(index, size_);
    count = std::min(count, size_ - index);
    if (count == 0) {
        return;
    }
    if (count > slots_.size() / 16) {
        // A large range (replacing much of the document): build anew without it.
        std::vector<double> logical = values();
        logical.erase(logical.begin() + static_cast<std::ptrdiff_t>(index),
                      logical.begin() + static_cast<std::ptrdiff_t>(index + count));
        build(logical, index, gapFor(logical.size()));
        return;
    }
    moveGap(index);
    // The entries right after the gap become part of it.
    for (std::size_t slot = gapStart_ + gapLength_; slot < gapStart_ + gapLength_ + count; ++slot) {
        const double value = slots_[slot];
        if (value != 0) {
            add(slot, -value);
            total_ -= value;
        }
        slots_[slot] = 0;
    }
    gapLength_ += count;
    size_ -= count;
}

double HeightIndex::sumBefore(std::size_t index) const {
    double sum = 0;
    for (std::size_t i = slotOf(std::min(index, size_)); i > 0; i -= lowestBit(i)) {
        sum += sums_[i];
    }
    return sum;
}

std::size_t HeightIndex::indexAt(double y) const {
    if (size_ == 0 || y >= total_) {
        return size_;
    }
    // Fenwick descent: the most slots whose sum is <= y; the next slot covers y.
    const std::size_t n = slots_.size();
    std::size_t pos = 0;
    double remaining = y;
    std::size_t step = 1;
    while (step * 2 <= n) {
        step *= 2;
    }
    for (; step > 0; step /= 2) {
        if (pos + step <= n && sums_[pos + step] <= remaining) {
            pos += step;
            remaining -= sums_[pos];
        }
    }
    // Skip entries without height (and the gap); also covers y < 0 and rounding.
    while (pos < n && slots_[pos] <= 0) {
        ++pos;
    }
    if (pos >= n) {
        return size_;
    }
    return pos < gapStart_ ? pos : pos - gapLength_;
}

std::size_t HeightIndex::slotOf(std::size_t index) const noexcept {
    return index < gapStart_ ? index : index + gapLength_;
}

std::vector<double> HeightIndex::values() const {
    std::vector<double> logical;
    logical.reserve(size_);
    logical.insert(logical.end(), slots_.begin(), slots_.begin() + static_cast<std::ptrdiff_t>(gapStart_));
    logical.insert(logical.end(), slots_.begin() + static_cast<std::ptrdiff_t>(gapStart_ + gapLength_), slots_.end());
    return logical;
}

void HeightIndex::rebuild(std::size_t gapAt, std::size_t gapLength) {
    build(values(), gapAt, gapLength);
}

void HeightIndex::build(const std::vector<double>& logical, std::size_t gapAt, std::size_t gapLength) {
    gapAt = std::min(gapAt, logical.size());
    slots_.assign(logical.size() + gapLength, 0.0);
    std::copy(logical.begin(), logical.begin() + static_cast<std::ptrdiff_t>(gapAt), slots_.begin());
    std::copy(logical.begin() + static_cast<std::ptrdiff_t>(gapAt), logical.end(),
              slots_.begin() + static_cast<std::ptrdiff_t>(gapAt + gapLength));
    gapStart_ = gapAt;
    gapLength_ = gapLength;
    size_ = logical.size();
    buildSums();
}

void HeightIndex::buildSums() {
    const std::size_t n = slots_.size();
    sums_.assign(n + 1, 0.0);
    total_ = 0;
    for (std::size_t i = 1; i <= n; ++i) {
        sums_[i] += slots_[i - 1];
        total_ += slots_[i - 1];
        if (const std::size_t parent = i + lowestBit(i); parent <= n) {
            sums_[parent] += sums_[i];
        }
    }
    ++rebuilds_;
}

void HeightIndex::moveGap(std::size_t to) {
    if (to == gapStart_) {
        return;
    }
    if (gapLength_ == 0) {
        gapStart_ = to;  // nothing to move
        return;
    }
    const std::size_t distance = to < gapStart_ ? gapStart_ - to : to - gapStart_;
    if (distance > slots_.size() / 64) {
        // Cheaper than two tree updates per entry: shift the entries, then sum anew (O(n) once,
        // when editing jumps to another place in the file).
        const auto begin = slots_.begin();
        const auto gapStart = static_cast<std::ptrdiff_t>(gapStart_);
        const auto gapLength = static_cast<std::ptrdiff_t>(gapLength_);
        const auto target = static_cast<std::ptrdiff_t>(to);
        if (to < gapStart_) {
            std::move_backward(begin + target, begin + gapStart, begin + gapStart + gapLength);
            std::fill(begin + target, begin + target + gapLength, 0.0);
        } else {
            std::move(begin + gapStart + gapLength, begin + target + gapLength, begin + gapStart);
            std::fill(begin + target, begin + target + gapLength, 0.0);
        }
        gapStart_ = to;
        buildSums();
        return;
    }
    const auto move = [this](std::size_t from, std::size_t dest) {
        const double value = slots_[from];
        slots_[dest] = value;
        slots_[from] = 0;
        if (value != 0) {
            add(from, -value);
            add(dest, value);
        }
    };
    if (to < gapStart_) {
        // Entries [to, gapStart_) move behind the gap, last first.
        for (std::size_t slot = gapStart_; slot > to; --slot) {
            move(slot - 1, slot - 1 + gapLength_);
        }
    } else {
        // Entries after the gap, up to index `to`, move in front of it.
        for (std::size_t slot = gapStart_ + gapLength_; slot < to + gapLength_; ++slot) {
            move(slot, slot - gapLength_);
        }
    }
    gapStart_ = to;
}

void HeightIndex::add(std::size_t slot, double delta) {
    for (std::size_t i = slot + 1; i < sums_.size(); i += lowestBit(i)) {
        sums_[i] += delta;
    }
}

}  // namespace hitokoto::core
