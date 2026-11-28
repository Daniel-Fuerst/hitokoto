// SPDX-License-Identifier: GPL-3.0-or-later
// S052: task list items. A bullet item (S003) whose text starts with `[ ]`, `[x]`, `[X]` or `[]`
// followed by a blank shows a checkbox in place of the marker. The prefix up to the text is hidden
// whether or not the caret is on the line (the user's decision: no source view of a box); the
// blank before the text takes the box's width. Registered before the bullet element, which it
// asks for the bullet and for the paragraph rules (a task item is a bullet item to CommonMark).
#include "hitokoto/core/task.hpp"

#include "../block_rules.hpp"

#include <memory>

namespace hitokoto::core {
namespace {

std::optional<TaskLineInfo> taskAfterBullet(std::u16string_view line, const LineMatch& bullet) {
    const std::uint32_t box = bullet.marker.end();
    if (box >= line.size() || line[box] != u'[') {
        return std::nullopt;
    }
    std::uint32_t close = box + 1;
    bool checked = false;
    if (close < line.size() && (line[close] == u' ' || line[close] == u'x' || line[close] == u'X')) {
        checked = line[close] != u' ';
        ++close;
    }
    if (close >= line.size() || line[close] != u']') {
        return std::nullopt;
    }
    std::uint32_t content = close + 1;
    if (content >= line.size() || !blocks::isBlank(line[content])) {
        return std::nullopt;  // created on the blank after `]` (S052, decision 5)
    }
    while (content < line.size() && blocks::isBlank(line[content])) {
        ++content;
    }
    return TaskLineInfo{
        .markerChar = bullet.markerChar,
        .box = box,
        .boxLength = close + 1 - box,
        .contentStart = content,
        .checked = checked,
        .isEmpty = content == line.size(),
    };
}

class TaskElement final : public LineElement {
public:
    [[nodiscard]] std::optional<LineMatch> parse(std::u16string_view line) const override {
        auto match = bullet_->parse(line);
        if (!match.has_value()) {
            return std::nullopt;
        }
        const auto task = taskAfterBullet(line, *match);
        if (!task.has_value()) {
            return std::nullopt;
        }
        match->marker = Span{0, task->contentStart};
        return match;
    }

    [[nodiscard]] ParagraphRole paragraphRole(std::u16string_view line,
                                              const LineMatch& match) const noexcept override {
        return bullet_->paragraphRole(line, match);
    }

    [[nodiscard]] bool startsAfterParagraph(std::u16string_view /*line*/, const LineMatch& /*match*/,
                                            bool /*lazy*/) const noexcept override {
        return true;  // the box is text after the marker, so the item is never empty (CommonMark 5.2)
    }

    void render(std::u16string_view line, const LineMatch& match, const LineContext& /*context*/,
                LineRender& out) const override {
        // The reveal is ignored on purpose: a box never shows its source (S052, decision 2).
        const std::uint32_t content = match.marker.end();
        const bool checked = taskChecked(line, match.markerChar, content);
        out.hidden.push_back({{match.markerChar, content - 1 - match.markerChar}, HideMode::ZeroWidth});
        out.hidden.push_back({{content - 1, 1}, HideMode::Box});
        out.decorations.push_back({DecorationKind::Checkbox, match.markerChar, match.level, checked});
        out.wrapIndent = content;
        if (checked && content < line.size()) {
            out.doneText = Span{content, static_cast<std::uint32_t>(line.size()) - content};
        }
    }

private:
    /// Whether the box between the marker and `content` is ticked.
    static bool taskChecked(std::u16string_view line, std::uint32_t markerChar, std::uint32_t content) {
        for (std::uint32_t i = markerChar + 1; i < content; ++i) {
            if (line[i] == u'[') {
                return line[i + 1] == u'x' || line[i + 1] == u'X';
            }
        }
        return false;
    }

    std::unique_ptr<LineElement> bullet_ = makeBulletElement();
};

}  // namespace

std::unique_ptr<LineElement> makeTaskElement() {
    return std::make_unique<TaskElement>();
}

std::optional<TaskLineInfo> parseTaskLine(std::u16string_view line) {
    static const std::unique_ptr<LineElement> bullet = makeBulletElement();
    const auto match = bullet->parse(line);
    return match.has_value() ? taskAfterBullet(line, *match) : std::nullopt;
}

std::optional<LineEdit> toggledTask(std::u16string_view line) {
    const auto task = parseTaskLine(line);
    if (!task.has_value()) {
        return std::nullopt;
    }
    return LineEdit{{task->box, task->boxLength}, task->checked ? u"[ ]" : u"[x]"};
}

std::u16string continuedTaskPrefix(std::u16string_view line) {
    const auto task = parseTaskLine(line);
    if (!task.has_value()) {
        return {};
    }
    return std::u16string(line.substr(0, task->box)) + u"[ ] ";
}

}  // namespace hitokoto::core
