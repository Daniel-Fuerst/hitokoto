# SPDX-License-Identifier: GPL-3.0-or-later
# S031 (review O5): the clipboard samples hold invented content only. Fails if a file under DATA_DIR
# holds a link with the file scheme, a path into a user profile (C:\Users, /Users/, /home/) or an
# e-mail address, which Office and browsers write into the HTML they copy. Prints one line per
# finding and "personal-data-lint: <n> findings".
if(NOT DATA_DIR)
    message(FATAL_ERROR "DATA_DIR not set")
endif()

file(GLOB_RECURSE files LIST_DIRECTORIES false "${DATA_DIR}/*")
set(patterns
    "[Ff][Ii][Ll][Ee]:"
    "[A-Za-z]:[\\\\/]+[Uu][Ss][Ee][Rr][Ss][\\\\/]"
    "/[Uu]sers/"
    "/home/"
    "[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z][A-Za-z]+"
)
set(findings 0)
set(report "")
foreach(f IN LISTS files)
    file(STRINGS "${f}" lines)
    set(lineno 0)
    foreach(line IN LISTS lines)
        math(EXPR lineno "${lineno} + 1")
        foreach(p IN LISTS patterns)
            if(line MATCHES "${p}")
                math(EXPR findings "${findings} + 1")
                string(APPEND report "\n  ${f}:${lineno}: matches ${p}")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()

message("personal-data-lint: ${findings} findings${report}")
if(findings GREATER 0)
    message(FATAL_ERROR "Personal data in the S031 test data (see tests/data/s031/README.md)")
endif()
