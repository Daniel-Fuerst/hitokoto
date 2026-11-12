# SPDX-License-Identifier: GPL-3.0-or-later
# S034: cases for scripts/packaging/AurPackage.cmake and for the recipe in packaging/archlinux, run
# with cmake -P on fixed inputs (no AUR, no network, no git history needed).
#   -DCASE=<name>          the case below
#   -DSCRIPT=<file>        scripts/packaging/AurPackage.cmake
#   -DRECIPE_DIR=<dir>     packaging/archlinux
#   -DWORK_DIR=<dir>       a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE SCRIPT RECIPE_DIR WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(expect_eq actual expected what)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: '${actual}', expected '${expected}'")
    endif()
endfunction()

# CMake wraps long messages, so runs of white space count as one space.
function(expect_contains text part what)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat_text "${text}")
    string(REGEX REPLACE "[ \t\r\n]+" " " flat_part "${part}")
    string(FIND "${flat_text}" "${flat_part}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' not in\n${text}")
    endif()
endfunction()

# Writes the items of a list one per line (an empty list gives an empty file).
function(write_lines file)
    set(text "")
    foreach(item IN LISTS ARGN)
        string(APPEND text "${item}\n")
    endforeach()
    file(WRITE "${file}" "${text}")
endfunction()

# A .SRCINFO as makepkg --printsrcinfo writes it, for the given pkgver and archive checksum.
function(srcinfo_text pkgver sha out)
    set(${out}
"pkgbase = hitokoto
\tpkgdesc = A native, single-file hybrid Markdown editor
\tpkgver = ${pkgver}
\tpkgrel = 1
\turl = https://github.com/Daniel-Fuerst/hitokoto
\tarch = x86_64
\tlicense = GPL-3.0-or-later
\tmakedepends = cmake
\tmakedepends = ninja
\tdepends = qt6-base
\tsource = hitokoto-${pkgver}.tar.gz::https://github.com/Daniel-Fuerst/hitokoto/archive/refs/tags/v${pkgver}.tar.gz
\tsource = hitokoto.desktop
\tsha256sums = ${sha}
\tsha256sums = eabd8f8ea10edf6b570fe7772df7219c78ef7a63fe846cd7044ddf12f5e754e9

pkgname = hitokoto
" PARENT_SCOPE)
endfunction()

# Runs the plan. Named arguments: TAGS (list), SRCINFO (text; absent: no .SRCINFO file).
# Sets <prefix>_CODE, <prefix>_LOG and, for each line key=value of the output, <prefix>_<key>.
function(run_plan prefix)
    cmake_parse_arguments(PARSE_ARGV 1 a "" "SRCINFO" "TAGS")
    write_lines("${WORK_DIR}/tags.txt" ${a_TAGS})
    file(REMOVE "${WORK_DIR}/output.txt" "${WORK_DIR}/SRCINFO")
    if("SRCINFO" IN_LIST ARGN)
        file(WRITE "${WORK_DIR}/SRCINFO" "${a_SRCINFO}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DMODE=plan -DTAGS_FILE=${WORK_DIR}/tags.txt
                -DSRCINFO_FILE=${WORK_DIR}/SRCINFO -DOUTPUT=${WORK_DIR}/output.txt -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(${prefix}_CODE "${code}" PARENT_SCOPE)
    set(${prefix}_LOG "${out}\n${err}" PARENT_SCOPE)
    foreach(key IN ITEMS action version aur_version aur_sha256)
        set(${prefix}_${key} "<unset>" PARENT_SCOPE)
    endforeach()
    if(EXISTS "${WORK_DIR}/output.txt")
        file(STRINGS "${WORK_DIR}/output.txt" lines)
        foreach(line IN LISTS lines)
            if(line MATCHES "^([a-z_0-9]+)=(.*)$")
                set(${prefix}_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
            endif()
        endforeach()
    endif()
endfunction()

# Runs the PKGBUILD mode on INPUT (a file). Sets <prefix>_CODE, <prefix>_LOG, <prefix>_OUTPUT.
function(run_pkgbuild prefix input version sha)
    file(REMOVE "${WORK_DIR}/PKGBUILD.out")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DMODE=pkgbuild -DPKGBUILD_FILE=${input} -DVERSION=${version}
                -DSHA256=${sha} -DOUTPUT=${WORK_DIR}/PKGBUILD.out -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(${prefix}_CODE "${code}" PARENT_SCOPE)
    set(${prefix}_LOG "${out}\n${err}" PARENT_SCOPE)
    set(text "")
    if(EXISTS "${WORK_DIR}/PKGBUILD.out")
        file(READ "${WORK_DIR}/PKGBUILD.out" text)
    endif()
    set(${prefix}_OUTPUT "${text}" PARENT_SCOPE)
endfunction()

# Checksums made up for the cases; only their form matters.
set(sha_a "1111111111111111111111111111111111111111111111111111111111111111")
set(sha_b "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789")

# --- plan: which version to publish (decision 2) ----------------------------------------------

if("${CASE}" STREQUAL "AHigherTagIsPublished")
    srcinfo_text(0.5.0 ${sha_a} srcinfo)
    run_plan(r TAGS v0.4.0 v0.5.0 v0.5.1 SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_action}" "publish" "action")
    expect_eq("${r_version}" "0.5.1" "version")
    expect_eq("${r_aur_version}" "0.5.0" "aur_version")

elseif("${CASE}" STREQUAL "TheSameVersionPublishesNothing")
    srcinfo_text(0.5.1 ${sha_a} srcinfo)
    run_plan(r TAGS v0.5.0 v0.5.1 SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_action}" "current" "action")
    expect_eq("${r_version}" "0.5.1" "version")
    expect_contains("${r_LOG}" "already at 0.5.1" "message")

elseif("${CASE}" STREQUAL "TheAurIsNeverDowngraded")
    srcinfo_text(0.6.0 ${sha_a} srcinfo)
    run_plan(r TAGS v0.5.0 v0.5.1 SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_action}" "ahead" "action")
    expect_contains("${r_LOG}" "ahead" "message")

elseif("${CASE}" STREQUAL "SeveralTagsLeadToTheHighest")
    # Numeric, not textual order: 0.5.10 is above 0.5.9.
    srcinfo_text(0.5.0 ${sha_a} srcinfo)
    run_plan(r TAGS v0.5.10 v0.5.1 v0.5.9 v0.5.2 SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_action}" "publish" "action")
    expect_eq("${r_version}" "0.5.10" "version")

elseif("${CASE}" STREQUAL "OnlyExactVersionTagsCount")
    srcinfo_text(0.4.0 ${sha_a} srcinfo)
    run_plan(r TAGS v0.5.0 v0.3.0-tulip v9.9.9-rc1 v1.0 v01.2.3 release-2.0.0 2.0.0 v3.0.0.0
             SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_version}" "0.5.0" "version")

elseif("${CASE}" STREQUAL "NoVersionTagFails")
    srcinfo_text(0.4.0 ${sha_a} srcinfo)
    run_plan(r TAGS v0.3.0-tulip SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "no version tag" "message")
    expect_eq("${r_action}" "<unset>" "action")

elseif("${CASE}" STREQUAL "NoSrcinfoFails")
    run_plan(r TAGS v0.5.0)
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "creates no package" "message")
    expect_eq("${r_action}" "<unset>" "action")

elseif("${CASE}" STREQUAL "SrcinfoWithoutPkgverFails")
    run_plan(r TAGS v0.5.0 SRCINFO "pkgbase = hitokoto\n\tpkgrel = 1\n\npkgname = hitokoto\n")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "creates no package" "message")
    run_plan(r TAGS v0.5.0 SRCINFO "")
    expect_eq("${r_CODE}" "1" "exit code with an empty .SRCINFO")

elseif("${CASE}" STREQUAL "AurPkgverOfAnotherFormFails")
    srcinfo_text(0.5.0.r3.gabc1234 ${sha_a} srcinfo)
    run_plan(r TAGS v0.5.0 SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "0.5.0.r3.gabc1234" "message names the pkgver")

elseif("${CASE}" STREQUAL "TheAurChecksumIsReported")
    # Decision 9: the first sha256sums entry is that of the archive.
    srcinfo_text(0.5.0 ${sha_b} srcinfo)
    run_plan(r TAGS v0.5.0 SRCINFO "${srcinfo}")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_aur_sha256}" "${sha_b}" "aur_sha256")
    expect_eq("${r_aur_version}" "0.5.0" "aur_version")

elseif("${CASE}" STREQUAL "CarriageReturnsAreIgnored")
    srcinfo_text(0.5.0 ${sha_b} srcinfo)
    string(REPLACE "\n" "\r\n" srcinfo "${srcinfo}")
    file(WRITE "${WORK_DIR}/tags.txt" "v0.5.0\r\nv0.5.1\r\n")
    file(WRITE "${WORK_DIR}/SRCINFO" "${srcinfo}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DMODE=plan -DTAGS_FILE=${WORK_DIR}/tags.txt
                -DSRCINFO_FILE=${WORK_DIR}/SRCINFO -DOUTPUT=${WORK_DIR}/output.txt -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_eq("${code}" "0" "exit code\n${out}\n${err}")
    file(READ "${WORK_DIR}/output.txt" text)
    expect_contains("${text}" "version=0.5.1\n" "version")
    expect_contains("${text}" "aur_version=0.5.0\n" "aur_version")
    expect_contains("${text}" "aur_sha256=${sha_b}\n" "aur_sha256")

# --- pkgbuild: the values written into the PKGBUILD (decision 3) -------------------------------

elseif("${CASE}" STREQUAL "ThePkgbuildGetsVersionReleaseAndChecksum")
    # The recipe of the repository itself: only pkgver, pkgrel and the archive checksum change.
    set(recipe "${RECIPE_DIR}/PKGBUILD")
    run_pkgbuild(r "${recipe}" 0.5.1 ${sha_b})
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    file(READ "${recipe}" before)
    string(REGEX REPLACE "\r\n" "\n" before "${before}")
    expect_contains("${r_OUTPUT}" "\npkgver=0.5.1\n" "pkgver")
    expect_contains("${r_OUTPUT}" "\npkgrel=1\n" "pkgrel")
    string(REGEX MATCH "sha256sums=\\([^)]*\\)" sums "${r_OUTPUT}")
    string(REGEX MATCHALL "'[^']*'" entries "${sums}")
    list(GET entries 0 first)
    expect_eq("${first}" "'${sha_b}'" "first sha256sums entry")
    string(REGEX MATCH "sha256sums=\\([^)]*\\)" sums_before "${before}")
    string(REGEX MATCHALL "'[^']*'" entries_before "${sums_before}")
    list(LENGTH entries count)
    list(LENGTH entries_before count_before)
    expect_eq("${count}" "${count_before}" "number of sha256sums entries")
    list(REMOVE_AT entries 0)
    list(REMOVE_AT entries_before 0)
    expect_eq("${entries}" "${entries_before}" "the other sha256sums entries")
    # Everything else stays as it is.
    foreach(text_var IN ITEMS before r_OUTPUT)
        string(REGEX REPLACE "\npkgver=[^\n]*" "\npkgver=X" ${text_var} "${${text_var}}")
        string(REGEX REPLACE "\npkgrel=[^\n]*" "\npkgrel=X" ${text_var} "${${text_var}}")
        string(REGEX REPLACE "sha256sums=\\([^)]*\\)" "sha256sums=(X)" ${text_var} "${${text_var}}")
    endforeach()
    expect_eq("${r_OUTPUT}" "${before}" "the rest of the PKGBUILD")

elseif("${CASE}" STREQUAL "ASingleLineChecksumArrayIsFilledIn")
    file(WRITE "${WORK_DIR}/PKGBUILD"
        "pkgname=x\npkgver=0.1.0\npkgrel=3\nsource=(a b)\nsha256sums=('SKIP' '${sha_a}')\n")
    run_pkgbuild(r "${WORK_DIR}/PKGBUILD" 2.0.0 ${sha_b})
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}"
        "pkgname=x\npkgver=2.0.0\npkgrel=1\nsource=(a b)\nsha256sums=('${sha_b}' '${sha_a}')\n"
        "PKGBUILD")

elseif("${CASE}" STREQUAL "ThePkgbuildModeRejectsBadInput")
    set(recipe "${RECIPE_DIR}/PKGBUILD")
    run_pkgbuild(r "${recipe}" 0.5 ${sha_b})
    expect_eq("${r_CODE}" "1" "exit code with version 0.5")
    expect_contains("${r_LOG}" "0.5" "message names the version")
    run_pkgbuild(r "${recipe}" 0.5.1 "ABCDEF")
    expect_eq("${r_CODE}" "1" "exit code with a bad checksum")
    file(WRITE "${WORK_DIR}/PKGBUILD" "pkgname=x\npkgrel=1\nsha256sums=('SKIP')\n")
    run_pkgbuild(r "${WORK_DIR}/PKGBUILD" 0.5.1 ${sha_b})
    expect_eq("${r_CODE}" "1" "exit code without pkgver")
    expect_contains("${r_LOG}" "pkgver" "message without pkgver")
    file(WRITE "${WORK_DIR}/PKGBUILD" "pkgname=x\npkgver=1.0.0\npkgver=1.0.0\npkgrel=1\nsha256sums=('SKIP')\n")
    run_pkgbuild(r "${WORK_DIR}/PKGBUILD" 0.5.1 ${sha_b})
    expect_eq("${r_CODE}" "1" "exit code with two pkgver lines")
    file(WRITE "${WORK_DIR}/PKGBUILD" "pkgname=x\npkgver=1.0.0\npkgrel=1\n")
    run_pkgbuild(r "${WORK_DIR}/PKGBUILD" 0.5.1 ${sha_b})
    expect_eq("${r_CODE}" "1" "exit code without sha256sums")
    expect_contains("${r_LOG}" "sha256sums" "message without sha256sums")
    expect_eq("${r_OUTPUT}" "" "no output after an error")

# --- the recipe in packaging/archlinux (decisions 3 and 10) -------------------------------------

elseif("${CASE}" STREQUAL "TheRecipeBuildsForX86Only")
    file(STRINGS "${RECIPE_DIR}/PKGBUILD" arch_lines REGEX "^arch=")
    expect_eq("${arch_lines}" "arch=('x86_64')" "arch")

elseif("${CASE}" STREQUAL "TheDesktopEntryPassesOneFile")
    file(STRINGS "${RECIPE_DIR}/hitokoto.desktop" exec_lines REGEX "^Exec=")
    expect_eq("${exec_lines}" "Exec=hitokoto %f" "Exec")

elseif("${CASE}" STREQUAL "TheDesktopEntryChecksumMatches")
    # The second sha256sums entry pins hitokoto.desktop; it must follow every change of the file.
    file(SHA256 "${RECIPE_DIR}/hitokoto.desktop" actual)
    file(READ "${RECIPE_DIR}/PKGBUILD" text)
    string(REGEX MATCH "sha256sums=\\([^)]*\\)" sums "${text}")
    string(REGEX MATCHALL "'[^']*'" entries "${sums}")
    list(GET entries 1 pinned)
    expect_eq("${pinned}" "'${actual}'" "checksum of hitokoto.desktop in the PKGBUILD")

elseif("${CASE}" STREQUAL "TheRepositoryKeepsNoSrcinfo")
    if(EXISTS "${RECIPE_DIR}/.SRCINFO")
        message(FATAL_ERROR "${RECIPE_DIR}/.SRCINFO exists; the workflow generates it (decision 3)")
    endif()

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
