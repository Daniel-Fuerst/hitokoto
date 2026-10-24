// SPDX-License-Identifier: GPL-3.0-or-later
// Conversions between core types and Qt types. The only place where both meet.
#pragma once

#include "hitokoto/core/theme.hpp"

#include <QColor>
#include <QString>

#include <string_view>

namespace hitokoto::ui {

/// Zero-copy view of a QString as UTF-16 code units.
inline std::u16string_view view(const QString& s) noexcept {
    return {reinterpret_cast<const char16_t*>(s.utf16()), static_cast<std::size_t>(s.size())};
}

inline QString toQString(std::u16string_view s) {
    return QString::fromUtf16(s.data(), static_cast<qsizetype>(s.size()));
}

inline QString toQString(std::string_view utf8) {
    return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
}

inline QColor toQColor(const core::Color& c) {
    return QColor(c.r, c.g, c.b, c.a);
}

}  // namespace hitokoto::ui
