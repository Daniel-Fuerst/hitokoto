// SPDX-License-Identifier: GPL-3.0-or-later
// S019 / ADR 0003: the heights of the editor's blocks and the y position of each, for the
// document layout. Prefix sums in a Fenwick tree give block <-> y in O(log n). Inserting and
// removing blocks works like a gap buffer: the array keeps a gap of unused slots at the last edit,
// so typing Enter costs O(log n) instead of rebuilding the sums of every block (10 MB, 427k
// blocks: 2.5-3.5 ms per Enter before).
#pragma once

#include <cstddef>
#include <vector>

namespace hitokoto::core {

class HeightIndex {
public:
    /// Replaces all heights; rebuilds the sums (O(n)).
    void assign(const std::vector<double>& heights);

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] double height(std::size_t index) const;
    /// Changes one height, O(log n).
    void set(std::size_t index, double height);
    /// Inserts `count` entries of `height` before `index` (`index` == size() appends).
    void insert(std::size_t index, std::size_t count, double height);
    /// Removes the entries [index, index + count).
    void erase(std::size_t index, std::size_t count);

    /// Sum of the heights of the entries before `index`.
    [[nodiscard]] double sumBefore(std::size_t index) const;
    [[nodiscard]] double total() const noexcept { return total_; }
    /// The entry with a positive height that covers `y` (sumBefore(i) <= y < sumBefore(i + 1));
    /// size() if y is at or below the total.
    [[nodiscard]] std::size_t indexAt(double y) const;

    /// How often the sums were rebuilt from scratch; edits at one place should not need it.
    [[nodiscard]] std::size_t rebuilds() const noexcept { return rebuilds_; }

private:
    [[nodiscard]] std::size_t slotOf(std::size_t index) const noexcept;
    /// The heights in index order, without the gap.
    [[nodiscard]] std::vector<double> values() const;
    void rebuild(std::size_t gapAt, std::size_t gapLength);
    void build(const std::vector<double>& logical, std::size_t gapAt, std::size_t gapLength);
    void buildSums();
    void moveGap(std::size_t to);
    void add(std::size_t slot, double delta);

    std::vector<double> slots_;  ///< heights by slot; the gap's slots hold 0
    std::vector<double> sums_;   ///< Fenwick tree over slots_, 1-based
    std::size_t gapStart_ = 0;
    std::size_t gapLength_ = 0;
    std::size_t size_ = 0;
    double total_ = 0;
    std::size_t rebuilds_ = 0;
};

}  // namespace hitokoto::core
