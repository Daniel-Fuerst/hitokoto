// SPDX-License-Identifier: GPL-3.0-or-later
// S024 (ADR 0007): properties of the front matter. Stub until S024 is implemented: reads nothing.
#include "hitokoto/core/properties.hpp"

namespace hitokoto::core {

bool isUnsupported(PropertiesErrorReason reason) noexcept {
    (void)reason;
    return false;
}

std::string_view reasonText(PropertiesErrorReason reason) noexcept {
    (void)reason;
    return {};
}

Properties parseProperties(std::span<const std::u16string> yamlLines) {
    (void)yamlLines;
    return {};
}

}  // namespace hitokoto::core
