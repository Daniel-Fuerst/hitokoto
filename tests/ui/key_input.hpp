// SPDX-License-Identifier: GPL-3.0-or-later
// Simulated key input for UI tests and benchmarks. Use these instead of QTest::key* (a lint test,
// S000_KeyInputOnlyThroughTestHelpers, enforces it).
//
// Why: clang-tidy's static analyzer reports a false positive inside Qt's key simulation
// (clang-analyzer-cplusplus.NewDelete on the QPointer that QTest::keyClick keeps). Builds and test
// runs call QTest unchanged; only the analyzer (__clang_analyzer__) sees empty bodies here, so the
// suppression lives in one place and every other check still covers the tests.
#pragma once

#include <QString>
#include <QTest>
#include <QWidget>

namespace hitokoto::test {

inline void keyClick([[maybe_unused]] QWidget* widget, [[maybe_unused]] Qt::Key key,
                     [[maybe_unused]] Qt::KeyboardModifiers modifier = Qt::NoModifier) {
#ifndef __clang_analyzer__
    QTest::keyClick(widget, key, modifier);
#endif
}

inline void keyClick([[maybe_unused]] QWidget* widget, [[maybe_unused]] char key,
                     [[maybe_unused]] Qt::KeyboardModifiers modifier = Qt::NoModifier) {
#ifndef __clang_analyzer__
    QTest::keyClick(widget, key, modifier);
#endif
}

inline void keyClicks([[maybe_unused]] QWidget* widget, [[maybe_unused]] const QString& text,
                      [[maybe_unused]] Qt::KeyboardModifiers modifier = Qt::NoModifier) {
#ifndef __clang_analyzer__
    QTest::keyClicks(widget, text, modifier);
#endif
}

}  // namespace hitokoto::test
