// SPDX-License-Identifier: GPL-3.0-or-later
// Text on its way into a document: what the editor and a table's cell editor insert (S004).
#pragma once

#include <QString>

#include <memory>

class QInputMethodEvent;
class QKeyEvent;

namespace hitokoto::ui {

/// `text` as it may enter the document: each half of a surrogate pair without its other half
/// becomes U+FFFD (core::replaceLoneSurrogates), so that the document can always be saved as UTF-8
/// (S004, rework of 2026-11-21). Every way text comes into the editor or a cell passes here:
/// pastes and drops, a pasted table, keys, input methods, the value a cell hands to its table.
[[nodiscard]] QString insertableText(const QString& text);

/// A copy of `event` with insertableText of its text, or nullptr when its text may enter as it is.
[[nodiscard]] std::unique_ptr<QKeyEvent> insertableKey(const QKeyEvent& event);

/// A copy of `event` with insertableText of its commit string, or nullptr when it may enter as it
/// is. The preedit text is not part of the document and stays.
[[nodiscard]] std::unique_ptr<QInputMethodEvent> insertableInput(const QInputMethodEvent& event);

}  // namespace hitokoto::ui
