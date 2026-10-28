// SPDX-License-Identifier: GPL-3.0-or-later
// S019 / ADR 0003: block heights and positions for the document layout. Typing Enter in a large
// file must not rebuild the positions of every block (S007 typing budget, benchmark
// typing_10mb_p99_ms).
#include "hitokoto/core/height_index.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <numeric>
#include <vector>

using namespace hitokoto::core;

namespace {
/// Compares every height and every prefix sum with a plain vector.
void expectSame(const HeightIndex& index, const std::vector<double>& reference) {
    ASSERT_EQ(index.size(), reference.size());
    double sum = 0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
        ASSERT_DOUBLE_EQ(index.height(i), reference[i]) << "at " << i;
        ASSERT_DOUBLE_EQ(index.sumBefore(i), sum) << "at " << i;
        sum += reference[i];
    }
    EXPECT_DOUBLE_EQ(index.sumBefore(reference.size()), sum);
    EXPECT_DOUBLE_EQ(index.total(), sum);
}

/// Deterministic pseudo-random numbers (no seed differences between platforms).
class Lcg {
public:
    std::uint32_t next(std::uint32_t bound) {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(state_ >> 33) % bound;
    }

private:
    std::uint64_t state_ = 42;
};
}  // namespace

TEST(S019_HeightIndex, SumsTotalAndHeights) {
    HeightIndex index;
    index.assign({10, 20, 0, 30});
    expectSame(index, {10, 20, 0, 30});
    index.set(1, 25);
    expectSame(index, {10, 25, 0, 30});
}

TEST(S019_HeightIndex, IndexAtFindsTheEntryCoveringYAndSkipsEmptyOnes) {
    HeightIndex index;
    index.assign({10, 20, 0, 30});
    EXPECT_EQ(index.indexAt(0), 0u);
    EXPECT_EQ(index.indexAt(9.9), 0u);
    EXPECT_EQ(index.indexAt(10), 1u);
    EXPECT_EQ(index.indexAt(29.9), 1u);
    EXPECT_EQ(index.indexAt(30), 3u);  // entry 2 has no height (a hidden line)
    EXPECT_EQ(index.indexAt(59.9), 3u);
    EXPECT_EQ(index.indexAt(60), 4u);
    EXPECT_EQ(index.indexAt(1000), 4u);
}

TEST(S019_HeightIndex, InsertAndEraseAnywhereMatchAPlainVector) {
    HeightIndex index;
    std::vector<double> reference(1000);
    std::iota(reference.begin(), reference.end(), 1.0);
    index.assign(reference);
    Lcg random;
    for (int step = 0; step < 3000; ++step) {
        const auto at = random.next(static_cast<std::uint32_t>(reference.size()) + 1);
        switch (random.next(4)) {
            case 0: {
                const auto count = 1 + random.next(step % 100 == 0 ? 3000 : 3);
                const auto height = static_cast<double>(random.next(50));
                index.insert(at, count, height);
                reference.insert(reference.begin() + at, count, height);
                break;
            }
            case 1: {
                const auto count =
                    std::min<std::size_t>(1 + random.next(step % 100 == 0 ? 2000 : 3), reference.size() - at);
                index.erase(at, count);
                reference.erase(reference.begin() + at, reference.begin() + static_cast<std::ptrdiff_t>(at + count));
                break;
            }
            default:
                if (at < reference.size()) {
                    const auto height = static_cast<double>(random.next(50));
                    index.set(at, height);
                    reference[at] = height;
                }
                break;
        }
        if (step % 250 == 0) {
            expectSame(index, reference);
            const double y = static_cast<double>(random.next(static_cast<std::uint32_t>(index.total()) + 1));
            const std::size_t found = index.indexAt(y);
            if (found < reference.size()) {
                EXPECT_LE(index.sumBefore(found), y);
                EXPECT_GT(index.sumBefore(found + 1), y);
            } else {
                EXPECT_GE(y, index.total());
            }
        }
    }
    expectSame(index, reference);
}

TEST(S019_HeightIndex, EditsAtOnePlaceDoNotRebuildTheSums) {
    HeightIndex index;
    index.assign(std::vector<double>(427'204, 20.0));
    // Typing in the middle of a 10 MB file: Enter splits a line (erase one, insert two), lines
    // change height, the caret moves a few lines up and down. The first Enter there may move the
    // gap from the end of the file once.
    std::size_t line = 213'602;
    index.erase(line, 1);
    index.insert(line, 2, 20.0);
    ++line;
    const std::size_t before = index.rebuilds();
    for (int i = 0; i < 200; ++i) {
        index.erase(line, 1);
        index.insert(line, 2, 20.0);
        index.set(line + 1, 40.0);
        line += (i % 3 == 0) ? 1U : 2U;
        if (i % 10 == 0) {
            index.set(line - 5, 60.0);
        }
    }
    EXPECT_EQ(index.rebuilds(), before);
    EXPECT_EQ(index.size(), 427'405u);
}

TEST(S019_HeightIndex, ManyNewLinesRebuildRarely) {
    HeightIndex index;
    index.assign(std::vector<double>(10'000, 20.0));
    index.insert(5'000, 1, 20.0);  // the first moves the gap
    const std::size_t before = index.rebuilds();
    for (std::size_t i = 1; i < 20'000; ++i) {
        index.insert(5'000 + i, 1, 20.0);  // typing Enter 20,000 times
    }
    EXPECT_LE(index.rebuilds() - before, 10u);
    EXPECT_DOUBLE_EQ(index.total(), 30'000 * 20.0);
}
