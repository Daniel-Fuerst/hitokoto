// SPDX-License-Identifier: GPL-3.0-or-later
// The work of the inline pass, counted in steps (S013, S015, S020, S022: a line renders in linear
// time). The inline elements and the registry's merge of their results count one step for each
// character they look at and each entry of a list they walk; `inlineSteps()` (markdown.hpp) reads
// the sum, and tests compare it with the length of the line. Internal to the core.
#pragma once

#include <cstddef>
#include <cstdint>

namespace hitokoto::core::detail {

/// Adds to the count of the calling thread.
void addInlineSteps(std::uint64_t steps) noexcept;

/// Collects the steps of one function in a local variable and adds them when the function ends,
/// so that counting costs an addition per loop pass and nothing more.
class InlineSteps {
public:
    InlineSteps() = default;
    InlineSteps(const InlineSteps&) = delete;
    InlineSteps& operator=(const InlineSteps&) = delete;
    ~InlineSteps() { addInlineSteps(count_); }

    void add(std::size_t steps = 1) noexcept { count_ += steps; }

private:
    std::uint64_t count_ = 0;
};

}  // namespace hitokoto::core::detail
