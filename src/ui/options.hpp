// SPDX-License-Identifier: GPL-3.0-or-later
// Behaviour the user will choose in an options dialog. There is no such dialog yet: until there
// is, each option keeps the default given here (a feature toggle switched off or on in code).
#pragma once

namespace hitokoto::ui {

struct Options {
    /// S018, ADR 0002 decision 6: after opening a file, a message says how many blank lines were
    /// added below tables. Off by the user's decision of 2026-10-27 (the document's modified mark
    /// is enough); meant to become an option.
    bool reportBlankLinesAdded = false;
    /// S024 (review O2): front matter shows as a properties block while the caret is elsewhere; off
    /// gives the S023 behaviour (plain text), as Obsidian's "Source". On by default.
    bool frontMatterAsProperties = true;
};

}  // namespace hitokoto::ui
