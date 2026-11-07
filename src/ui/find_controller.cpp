// SPDX-License-Identifier: GPL-3.0-or-later
// S028: find controller. Stub for the red tests; the implementation follows.
#include "find_controller.hpp"

#include "editor_widget.hpp"
#include "find_bar.hpp"

namespace hitokoto::ui {

FindController::FindController(EditorWidget* editor, FindBar* bar, const core::ElementRegistry& registry,
                               QObject* parent)
    : QObject(parent), editor_(editor), bar_(bar), registry_(registry) {}

FindController::~FindController() = default;

void FindController::applyTheme(const core::Theme& theme) {
    (void)theme;
}
void FindController::open() {}
void FindController::close() {}
void FindController::next() {}
void FindController::previous() {}
bool FindController::isOpen() const {
    return false;
}
bool FindController::isComplete() const {
    return false;
}
std::optional<core::FindMatch> FindController::currentMatch() const {
    return std::nullopt;
}
int FindController::currentIndex() const {
    return 0;
}
int FindController::matchCount() const {
    return 0;
}
QString FindController::countText() const {
    return {};
}
bool FindController::matchCase() const {
    return false;
}
bool FindController::highlightAll() const {
    return false;
}
std::vector<core::FindMatch> FindController::visibleMatches() const {
    return {};
}
QList<QTextLayout::FormatRange> FindController::highlightFormats(int blockNumber) const {
    (void)blockNumber;
    return {};
}
QString FindController::lastAnnouncement() const {
    return {};
}

}  // namespace hitokoto::ui
