# SPDX-License-Identifier: GPL-3.0-or-later
# S005: all colours, fonts, radii and spacings in the UI come from theme tokens.
# Fails if UI sources contain styling literals. A line may opt out with the comment
# "theme-lint: allow" plus a reason (for mechanism, not styling, e.g. transparent concealment).
if(NOT UI_DIR)
    message(FATAL_ERROR "UI_DIR not set")
endif()

file(GLOB_RECURSE sources "${UI_DIR}/*.cpp" "${UI_DIR}/*.hpp")
set(patterns
    "#[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]([0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])?([0-9a-fA-F][0-9a-fA-F])?\""  # "#rrggbb" literals
    "QColor\\([0-9]"                        # QColor(12, 34, 56)
    "Qt::(white|black|red|green|blue|cyan|magenta|yellow|gray|grey|darkGray|lightGray|transparent)"
    "setPointSizeF?\\([0-9]"
    "setPixelSize\\([0-9]"
    "QFont\\(\"[^\"]+\""                   # QFont("Some Family")
    "rgba?\\("
)
set(violations "")
foreach(src IN LISTS sources)
    file(STRINGS "${src}" lines)
    set(lineno 0)
    foreach(line IN LISTS lines)
        math(EXPR lineno "${lineno} + 1")
        if(line MATCHES "theme-lint: allow")
            continue()
        endif()
        foreach(p IN LISTS patterns)
            if(line MATCHES "${p}")
                string(APPEND violations "\n  ${src}:${lineno}: ${line}")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()

if(violations)
    message(FATAL_ERROR "Styling literals found in UI code (use theme tokens):${violations}")
endif()
message(STATUS "No styling literals in ${UI_DIR}")
