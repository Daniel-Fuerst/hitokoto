// SPDX-License-Identifier: GPL-3.0-or-later
#include "text_input.hpp"

#include "hitokoto/core/text_codec.hpp"
#include "qt_bridge.hpp"

#include <QInputMethodEvent>
#include <QKeyEvent>

#include <string_view>

namespace hitokoto::ui {

QString insertableText(const QString& text) {
    const std::u16string_view chars = view(text);
    if (core::findLoneSurrogate(chars) == std::u16string_view::npos) {
        return text;  // the usual case: no copy
    }
    return toQString(core::replaceLoneSurrogates(chars));
}

std::unique_ptr<QKeyEvent> insertableKey(const QKeyEvent& event) {
    const QString text = insertableText(event.text());
    if (text == event.text()) {
        return nullptr;
    }
    auto copy = std::make_unique<QKeyEvent>(event.type(), event.key(), event.modifiers(), event.nativeScanCode(),
                                            event.nativeVirtualKey(), event.nativeModifiers(), text,
                                            event.isAutoRepeat(), static_cast<quint16>(event.count()), event.device());
    copy->setAccepted(event.isAccepted());
    return copy;
}

std::unique_ptr<QInputMethodEvent> insertableInput(const QInputMethodEvent& event) {
    const QString commit = insertableText(event.commitString());
    if (commit == event.commitString()) {
        return nullptr;
    }
    auto copy = std::make_unique<QInputMethodEvent>(event.preeditString(), event.attributes());
    copy->setCommitString(commit, event.replacementStart(), event.replacementLength());
    copy->setAccepted(event.isAccepted());
    return copy;
}

}  // namespace hitokoto::ui
