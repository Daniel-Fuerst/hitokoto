# SPDX-License-Identifier: GPL-3.0-or-later
# S042 lint sample, never configured: docs/ in a comment is allowed, a glob over docs/ is not.
file(GLOB documents ${PROJECT_SOURCE_DIR}/docs/*.md)  # finding 4
set(note "see the story")  # docs/stories/042-docs-only-pull-requests.md, a comment
