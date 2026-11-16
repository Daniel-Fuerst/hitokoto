// SPDX-License-Identifier: GPL-3.0-or-later
// For the tests that a line renders in linear time (S013, S015, S020, S022). They count the steps
// of the inline pass (core::inlineSteps) for a short and a long line of the same pattern and
// compare the two: steps, not time, so the result is the same on every machine and in every build.
#pragma once

#include "hitokoto/core/markdown.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace hitokoto::test {

/// The built-in elements without the bounds of S007, so that they run on a line of any length.
inline const core::ElementRegistry& unboundedRegistry() {
    static const core::ElementRegistry registry = [] {
        core::ElementRegistry r = core::ElementRegistry::withBuiltins();
        r.setInlineBounds({static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)});
        return r;
    }();
    return registry;
}

/// The steps of three renders of `line`: concealed, revealed, and concealed as a line of a
/// paragraph that the next line continues (where the end of the line is read for a hard break).
inline std::uint64_t renderSteps(std::u16string_view line) {
    const std::uint64_t before = core::inlineSteps();
    (void)unboundedRegistry().render(line, {false});
    (void)unboundedRegistry().render(line, {true});
    (void)unboundedRegistry().render(line, {false, core::BlockState{0, 0, 1}, u"next line"});
    return core::inlineSteps() - before;
}

/// A line of a given length, made of one pattern.
struct LinePattern {
    const char* name;
    std::function<std::u16string(std::size_t)> make;
};

/// `unit` repeated until the line has `length` characters or a few more.
inline std::u16string repeated(std::u16string_view unit, std::size_t length) {
    std::u16string line;
    line.reserve(length + unit.size());
    while (line.size() < length) {
        line += unit;
    }
    return line;
}

/// A line of `unit` repeated.
inline LinePattern of(const char* name, std::u16string_view unit) {
    return {name, [text = std::u16string(unit)](std::size_t length) { return repeated(text, length); }};
}

/// A line whose first half repeats `first` and whose second half repeats `second`: every opener
/// first, then every closer.
inline LinePattern halves(const char* name, std::u16string_view first, std::u16string_view second) {
    return {name, [a = std::u16string(first), b = std::u16string(second)](std::size_t length) {
                return repeated(a, length / 2) + repeated(b, length / 2);
            }};
}

/// At most this many steps per character for the three renders of `renderSteps`. The patterns of
/// the tests take up to 47, random lines of syntax up to 64.
inline constexpr std::uint64_t kStepsPerCharacter = 100;

/// Linear work: a line eight times as long takes at most eight and a half times the steps (work
/// that grows with the square of the length takes 64 times), and no more than
/// kStepsPerCharacter for each character.
inline void expectLinearWork(const LinePattern& pattern) {
    constexpr std::size_t kShort = 2000;
    constexpr std::size_t kLong = 8 * kShort;
    const std::u16string longLine = pattern.make(kLong);
    const std::uint64_t shortSteps = renderSteps(pattern.make(kShort));
    const std::uint64_t longSteps = renderSteps(longLine);
    EXPECT_LE(longSteps, shortSteps * 17 / 2 + 1000)
        << "a line of " << pattern.name << ": " << shortSteps << " steps for " << kShort << " characters, " << longSteps
        << " for " << kLong;
    EXPECT_LE(longSteps, kStepsPerCharacter * longLine.size())
        << "a line of " << pattern.name << ": " << longSteps << " steps for " << longLine.size() << " characters";
}

}  // namespace hitokoto::test
