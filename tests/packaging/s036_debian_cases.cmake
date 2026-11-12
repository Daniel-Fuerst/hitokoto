# SPDX-License-Identifier: GPL-3.0-or-later
# S036: cases for scripts/packaging/DebianPackage.cmake and for the recipe in packaging/debian, run
# with cmake -P on fixed inputs (no network; the changelog cases build a throwaway git repository).
#   -DCASE=<name>          the case below
#   -DSCRIPT=<file>        scripts/packaging/DebianPackage.cmake
#   -DRECIPE_DIR=<dir>     packaging/debian
#   -DSOURCE_DIR=<dir>     the source tree (CMakeLists.txt, packaging/archlinux, .gitattributes)
#   -DWORK_DIR=<dir>       a scratch directory for this case (emptied first)
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE SCRIPT RECIPE_DIR SOURCE_DIR WORK_DIR)
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

# The same, character for character (line breaks and indentation count).
function(expect_contains_exactly text part what)
    string(FIND "${text}" "${part}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' not in\n${text}")
    endif()
endfunction()

function(expect_not_contains text part what)
    string(FIND "${text}" "${part}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${part}' found in\n${text}")
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

# The settings file of the recipe with the given first version and skip lines.
function(write_versions first)
    set(text "# test\nchangelog-from 0.3.0\nfirst ${first}\n")
    foreach(line IN LISTS ARGN)
        string(APPEND text "${line}\n")
    endforeach()
    file(WRITE "${WORK_DIR}/versions.txt" "${text}")
endfunction()

# Runs the plan; the result lines become variables plan_<key>, and plan_result the exit code.
function(run_plan)
    cmake_parse_arguments(a "DRY_RUN" "" "" ${ARGN})
    set(args -DMODE=plan -DVERSIONS_FILE=${WORK_DIR}/versions.txt -DTAGS_FILE=${WORK_DIR}/tags.txt
             -DRELEASES_FILE=${WORK_DIR}/releases.txt -DOUTPUT=${WORK_DIR}/plan.txt)
    if(a_DRY_RUN)
        list(APPEND args -DDRY_RUN=ON)
    endif()
    file(REMOVE "${WORK_DIR}/plan.txt")
    execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(plan_result "${code}" PARENT_SCOPE)
    set(plan_messages "${out}${err}" PARENT_SCOPE)
    foreach(key IN ITEMS action versions build latest skipped without_package)
        set(plan_${key} "<unset>" PARENT_SCOPE)
    endforeach()
    if(code EQUAL 0 AND EXISTS "${WORK_DIR}/plan.txt")
        file(STRINGS "${WORK_DIR}/plan.txt" lines)
        foreach(line IN LISTS lines)
            if(line MATCHES "^([a-z_]+)=(.*)$")
                set(plan_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
            endif()
        endforeach()
    endif()
endfunction()

function(expect_plan_ok)
    if(NOT plan_result EQUAL 0)
        message(FATAL_ERROR "the plan failed (${plan_result}):\n${plan_messages}")
    endif()
endfunction()

function(expect_plan_fails part)
    if(plan_result EQUAL 0)
        message(FATAL_ERROR "the plan succeeded, expected it to fail with '${part}'")
    endif()
    expect_contains("${plan_messages}" "${part}" "message of the failed plan")
endfunction()

set(tags_so_far v0.3.0 v0.3.0-tulip v0.3.1 v0.4.0 v0.5.0 v0.5.1)

# A throwaway git repository with the history the changelog cases read. Skipped without git.
macro(make_repo)
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message("S036-SKIP: git not found")
        return()
    endif()
    set(repo "${WORK_DIR}/repo")
    file(MAKE_DIRECTORY "${repo}")
    set(git "${GIT_EXECUTABLE}" -C "${repo}" -c user.name=S036 -c user.email=s036@example.invalid
            -c commit.gpgsign=false -c tag.gpgsign=false -c core.autocrlf=false)
    set(counter 0)
endmacro()

function(git_run)
    execute_process(COMMAND ${git} ${ARGN} RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "git ${ARGN}: ${err}")
    endif()
endfunction()

# A commit on the current branch with the given date (seconds since the epoch, UTC+1).
function(commit_at date message)
    math(EXPR n "${counter} + 1")
    set(counter ${n} PARENT_SCOPE)
    file(WRITE "${repo}/file-${n}.txt" "${n}\n")
    git_run(add "file-${n}.txt")
    set(ENV{GIT_AUTHOR_DATE} "@${date} +0100")
    set(ENV{GIT_COMMITTER_DATE} "@${date} +0100")
    git_run(commit -q -m "${message}")
endfunction()

# A merge of a side branch into main as GitHub writes it: "Merge pull request #N from …", then the
# title of the pull request as the body.
function(merge_pr date number title)
    math(EXPR n "${counter} + 1")
    set(counter ${n} PARENT_SCOPE)
    git_run(checkout -q -b "pr-${number}")
    file(WRITE "${repo}/pr-${number}.txt" "${number}\n")
    git_run(add "pr-${number}.txt")
    set(ENV{GIT_AUTHOR_DATE} "@${date} +0100")
    set(ENV{GIT_COMMITTER_DATE} "@${date} +0100")
    git_run(commit -q -m "work for ${number}")
    git_run(checkout -q main)
    # A message file: a title with a semicolon would split a CMake argument list.
    file(WRITE "${WORK_DIR}/merge-message.txt"
         "Merge pull request #${number} from Daniel-Fuerst/branch-${number}\n\n${title}\n")
    git_run(merge -q --no-ff "pr-${number}" -F "${WORK_DIR}/merge-message.txt")
endfunction()

# The history of the changelog cases; 1794426129 is Wed, 11 Nov 2026 20:42:09 +0100.
macro(make_history)
    make_repo()
    git_run(init -q -b main)
    commit_at(1794426129 "Start")
    git_run(tag -a v0.3.0 -m v0.3.0)
    merge_pr(1794512529 31 "S029: benchmark compared with main on the same runner")
    commit_at(1794512600 "unrefined.md: a note on main")
    merge_pr(1794512700 32 "S030: a defined version step; with a semicolon")
    git_run(tag v0.3.1)
    git_run(tag v0.3.1-rc)
    string(CONCAT long_title "S031: Pasting a table from Outlook, Excel or a web page as a Markdown "
                             "table, a title long enough to be wrapped")
    merge_pr(1794598929 35 "${long_title}")
    git_run(tag v0.4.0)
    merge_pr(1794685329 40 "Packaging/aur update on new version")
    git_run(tag v0.5.1)
endmacro()

function(run_changelog version)
    file(REMOVE "${WORK_DIR}/changelog" "${WORK_DIR}/notes.md")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=changelog -DREPO_DIR=${repo}
            -DVERSION=${version} -DVERSIONS_FILE=${WORK_DIR}/versions.txt
            -DCONTROL_FILE=${RECIPE_DIR}/control -DOUTPUT=${WORK_DIR}/changelog
            -DNOTES=${WORK_DIR}/notes.md -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(changelog_result "${code}" PARENT_SCOPE)
    set(changelog_messages "${out}${err}" PARENT_SCOPE)
    set(changelog "" PARENT_SCOPE)
    set(notes "" PARENT_SCOPE)
    if(code EQUAL 0)
        file(READ "${WORK_DIR}/changelog" text)
        set(changelog "${text}" PARENT_SCOPE)
        file(READ "${WORK_DIR}/notes.md" text)
        set(notes "${text}" PARENT_SCOPE)
    endif()
endfunction()

function(expect_changelog_ok)
    if(NOT changelog_result EQUAL 0)
        message(FATAL_ERROR "the changelog failed (${changelog_result}):\n${changelog_messages}")
    endif()
endfunction()

# The maintainer line of the recipe's control template.
function(recipe_maintainer out)
    file(STRINGS "${RECIPE_DIR}/control" lines REGEX "^Maintainer: " ENCODING UTF-8)
    list(LENGTH lines n)
    expect_eq("${n}" "1" "Maintainer lines in packaging/debian/control")
    string(REGEX REPLACE "^Maintainer: " "" m "${lines}")
    set(${out} "${m}" PARENT_SCOPE)
endfunction()

function(run_control)
    cmake_parse_arguments(a "" "VERSION;DEPENDS;SIZE;TEMPLATE" "" ${ARGN})
    if(NOT a_TEMPLATE)
        set(a_TEMPLATE "${RECIPE_DIR}/control")
    endif()
    file(REMOVE "${WORK_DIR}/control")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=control -DCONTROL_FILE=${a_TEMPLATE}
            "-DVERSION=${a_VERSION}" "-DDEPENDS=${a_DEPENDS}" "-DINSTALLED_SIZE=${a_SIZE}"
            -DOUTPUT=${WORK_DIR}/control -P "${SCRIPT}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(control_result "${code}" PARENT_SCOPE)
    set(control_messages "${out}${err}" PARENT_SCOPE)
    set(control "" PARENT_SCOPE)
    if(code EQUAL 0)
        file(READ "${WORK_DIR}/control" text)
        set(control "${text}" PARENT_SCOPE)
    endif()
endfunction()

# ---- The plan (decision 2) ----

if("${CASE}" STREQUAL "TheFirstRunReleasesTheFirstVersion")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far})
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_action}" "release" "action")
    expect_eq("${plan_versions}" "0.5.1" "versions to release (none before the first)")
    expect_eq("${plan_build}" "[\"0.5.1\"]" "build matrix")
    expect_eq("${plan_latest}" "0.5.1" "latest")

elseif("${CASE}" STREQUAL "APublishedVersionIsNotReleasedAgain")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far})
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_action}" "current" "action")
    expect_eq("${plan_versions}" "" "versions to release")
    expect_eq("${plan_build}" "[]" "build matrix")
    expect_eq("${plan_latest}" "0.5.1" "latest")
    expect_eq("${plan_without_package}" "" "releases without the package")

elseif("${CASE}" STREQUAL "SeveralMissingVersionsComeOldestFirst")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far} v0.5.10 v0.5.2 v0.6.0)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.2 hitokoto_0.5.2-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_action}" "release" "action")
    expect_eq("${plan_versions}" "0.5.1 0.5.10 0.6.0" "versions to release (numeric order)")
    expect_eq("${plan_build}" "[\"0.5.1\",\"0.5.10\",\"0.6.0\"]" "build matrix")
    expect_eq("${plan_latest}" "0.6.0" "latest")

elseif("${CASE}" STREQUAL "ACaughtUpVersionIsNeverLatest")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far} v0.5.2 v0.5.3)
    write_lines("${WORK_DIR}/releases.txt"
        "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS"
        "v0.5.3 hitokoto_0.5.3-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_versions}" "0.5.2" "versions to release")
    expect_eq("${plan_latest}" "0.5.3" "latest stays the highest version")

elseif("${CASE}" STREQUAL "OnlyExactVersionTagsCount")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" v0.5.1 v0.5.2-rc1 v0.6.0-tulip 0.7.0 v1.0 v01.0.0 V2.0.0)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.2-rc1 something.deb" "v0.6.0-tulip x")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_versions}" "0.5.1" "versions to release")
    expect_eq("${plan_latest}" "0.5.1" "latest")

elseif("${CASE}" STREQUAL "SkippedVersionsAreLeftOutAndNamed")
    # Review O2: a version that cannot be built with Debian's Qt for good.
    write_versions(0.5.1 "skip 0.5.2 needs QFoo from Qt 6.9" "# skip 0.5.3 only a comment")
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far} v0.5.2 v0.5.3)
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_versions}" "0.5.1 0.5.3" "versions to release")
    expect_eq("${plan_skipped}" "0.5.2" "skipped versions")
    expect_eq("${plan_latest}" "0.5.3" "latest")
    expect_contains("${plan_messages}" "0.5.2 is skipped: needs QFoo from Qt 6.9" "message")

elseif("${CASE}" STREQUAL "ASkippedHighestVersionIsNotLatest")
    write_versions(0.5.1 "skip 0.5.3 broken on trixie")
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far} v0.5.2 v0.5.3)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_versions}" "0.5.2" "versions to release")
    expect_eq("${plan_latest}" "0.5.2" "latest")

elseif("${CASE}" STREQUAL "AReleaseWithoutThePackageIsReported")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far} v0.5.2)
    write_lines("${WORK_DIR}/releases.txt" "v0.5.1" "v0.5.2 hitokoto_0.5.2-1_amd64.deb SHA256SUMS")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_action}" "current" "action (a published release is never changed)")
    expect_eq("${plan_without_package}" "0.5.1" "releases without the package")
    expect_contains("${plan_messages}" "v0.5.1 has no hitokoto_0.5.1-1_amd64.deb" "message")

elseif("${CASE}" STREQUAL "ADryRunBuildsTheHighestVersion")
    write_versions(0.5.1)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far} v0.5.2)
    write_lines("${WORK_DIR}/releases.txt"
        "v0.5.1 hitokoto_0.5.1-1_amd64.deb SHA256SUMS"
        "v0.5.2 hitokoto_0.5.2-1_amd64.deb SHA256SUMS")
    run_plan(DRY_RUN)
    expect_plan_ok()
    expect_eq("${plan_action}" "current" "action")
    expect_eq("${plan_build}" "[\"0.5.2\"]" "build matrix of a dry run")

elseif("${CASE}" STREQUAL "NoVersionFromTheFirstOnIsGreen")
    write_versions(0.6.0)
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far})
    write_lines("${WORK_DIR}/releases.txt")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_action}" "none" "action")
    expect_eq("${plan_build}" "[]" "build matrix")
    expect_eq("${plan_latest}" "" "latest")
    run_plan(DRY_RUN)
    expect_plan_fails("no version tag from 0.6.0 on")

elseif("${CASE}" STREQUAL "BadSettingsFail")
    write_lines("${WORK_DIR}/tags.txt" ${tags_so_far})
    write_lines("${WORK_DIR}/releases.txt")
    file(WRITE "${WORK_DIR}/versions.txt" "changelog-from 0.3.0\nfirst 0.5\n")
    run_plan()
    expect_plan_fails("first")
    write_versions(0.5.1 "skip 0.5.2")
    run_plan()
    expect_plan_fails("skip 0.5.2")
    write_versions(0.5.1 "skipp 0.5.2 typo")
    run_plan()
    expect_plan_fails("skipp 0.5.2 typo")
    file(WRITE "${WORK_DIR}/versions.txt" "changelog-from 0.3.0\n")
    run_plan()
    expect_plan_fails("first")

elseif("${CASE}" STREQUAL "CarriageReturnsAreIgnored")
    file(WRITE "${WORK_DIR}/versions.txt" "changelog-from 0.3.0\r\nfirst 0.5.1\r\nskip 0.5.2 why\r\n")
    file(WRITE "${WORK_DIR}/tags.txt" "v0.5.1\r\nv0.5.2\r\nv0.5.3\r\n")
    file(WRITE "${WORK_DIR}/releases.txt" "v0.5.3 hitokoto_0.5.3-1_amd64.deb SHA256SUMS\r\n")
    run_plan()
    expect_plan_ok()
    expect_eq("${plan_versions}" "0.5.1" "versions to release")
    expect_eq("${plan_skipped}" "0.5.2" "skipped")
    expect_eq("${plan_without_package}" "" "releases without the package")

elseif("${CASE}" STREQUAL "TheRecipeStartsAt051")
    file(READ "${RECIPE_DIR}/versions.txt" text)
    if(NOT text MATCHES "(^|\n)first 0\\.5\\.1\r?\n")
        message(FATAL_ERROR "packaging/debian/versions.txt does not start at 0.5.1:\n${text}")
    endif()
    if(NOT text MATCHES "(^|\n)changelog-from 0\\.3\\.0\r?\n")
        message(FATAL_ERROR "packaging/debian/versions.txt: the changelog does not start at 0.3.0")
    endif()

# ---- The changelog and the notes (decision 6) ----

elseif("${CASE}" STREQUAL "TheChangelogHasAnEntryPerVersion")
    make_history()
    write_versions(0.5.1)
    run_changelog(0.4.0)
    expect_changelog_ok()
    recipe_maintainer(maintainer)
    # Newest first, one entry per exact version tag up to 0.4.0, none for v0.3.1-rc or v0.5.1.
    string(REGEX MATCHALL "(^|\n)hitokoto \\([^)]*\\)" heads "${changelog}")
    list(TRANSFORM heads STRIP)
    expect_eq("${heads}" "hitokoto (0.4.0-1);hitokoto (0.3.1-1);hitokoto (0.3.0-1)" "entries")
    expect_contains_exactly("${changelog}" "hitokoto (0.4.0-1) trixie; urgency=medium\n\n" "head line")
    expect_contains_exactly("${changelog}"
        " -- ${maintainer}  Fri, 13 Nov 2026 20:42:09 +0100\n\nhitokoto (0.3.1-1)"
        "trailer of 0.4.0 (date of the tagged commit)")
    expect_contains_exactly("${changelog}"
        "  * First version with a version tag; earlier changes are not listed.\n" "entry of 0.3.0")
    expect_contains("${changelog}" " -- ${maintainer}  Wed, 11 Nov 2026 20:42:09 +0100\n"
        "trailer of 0.3.0")
    if(NOT changelog MATCHES "\n$" OR changelog MATCHES "\n\n$")
        message(FATAL_ERROR "the changelog does not end with exactly one newline")
    endif()

elseif("${CASE}" STREQUAL "MergesShowNumberAndTitle")
    make_history()
    write_versions(0.5.1)
    run_changelog(0.3.1)
    expect_changelog_ok()
    # Oldest first within an entry; a commit without a merge shows its subject.
    expect_contains_exactly("${changelog}" "hitokoto (0.3.1-1) trixie; urgency=medium

  * #31 S029: benchmark compared with main on the same runner
  * unrefined.md: a note on main
  * #32 S030: a defined version step; with a semicolon

 -- " "entry of 0.3.1")
    expect_not_contains("${changelog}" "work for" "commits of the side branches")

elseif("${CASE}" STREQUAL "LongLinesAreWrapped")
    make_history()
    write_versions(0.5.1)
    run_changelog(0.4.0)
    expect_changelog_ok()
    string(REPLACE "\n" ";" lines "${changelog}")
    foreach(line IN LISTS lines)
        string(LENGTH "${line}" n)
        if(n GREATER 79)
            message(FATAL_ERROR "line longer than 79 characters: '${line}'")
        endif()
    endforeach()
    string(CONCAT wrapped "  * #35 S031: Pasting a table from Outlook, Excel or a web page as a "
                          "Markdown\n    table, a title long enough to be wrapped\n")
    expect_contains_exactly("${changelog}" "${wrapped}" "wrapped entry")

elseif("${CASE}" STREQUAL "TheNotesListOnlyTheVersion")
    make_history()
    write_versions(0.5.1)
    run_changelog(0.5.1)
    expect_changelog_ok()
    expect_eq("${notes}" "Changes since v0.4.0:

- #40 Packaging/aur update on new version

Debian package for trixie (amd64): `hitokoto_0.5.1-1_amd64.deb`, checksum in `SHA256SUMS`.
" "notes of 0.5.1")
    run_changelog(0.3.0)
    expect_changelog_ok()
    expect_eq("${notes}" "First version with a version tag; earlier changes are not listed.

Debian package for trixie (amd64): `hitokoto_0.3.0-1_amd64.deb`, checksum in `SHA256SUMS`.
" "notes of the first version")

elseif("${CASE}" STREQUAL "TheChangelogNeedsTheVersionTag")
    make_history()
    write_versions(0.5.1)
    run_changelog(0.4.1)
    if(changelog_result EQUAL 0)
        message(FATAL_ERROR "a changelog for a version without a tag succeeded")
    endif()
    expect_contains("${changelog_messages}" "v0.4.1" "message")

# ---- The control file (decision 5) ----

elseif("${CASE}" STREQUAL "TheControlFileGetsVersionDependsAndSize")
    run_control(VERSION 0.5.1 SIZE 1234
        DEPENDS "libc6 (>= 2.38), libqt6core6t64 (>= 6.8.0), libqt6widgets6 (>= 6.8.0)")
    if(NOT control_result EQUAL 0)
        message(FATAL_ERROR "control failed:\n${control_messages}")
    endif()
    recipe_maintainer(maintainer)
    foreach(line IN ITEMS "Package: hitokoto" "Version: 0.5.1-1" "Architecture: amd64"
                          "Section: editors" "Maintainer: ${maintainer}" "Installed-Size: 1234"
                          "Depends: libc6 (>= 2.38), libqt6core6t64 (>= 6.8.0), libqt6widgets6 (>= 6.8.0)")
        string(FIND "\n${control}" "\n${line}\n" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "control lacks the line '${line}':\n${control}")
        endif()
    endforeach()
    if(control MATCHES "@[A-Z_]+@")
        message(FATAL_ERROR "placeholders left:\n${control}")
    endif()
    if(NOT control MATCHES "\nDescription: [^\n]+\n [^\n]+")
        message(FATAL_ERROR "control has no description with an extended part:\n${control}")
    endif()
    if(control MATCHES "\nDescription: [Aa]n? ")
        message(FATAL_ERROR "the synopsis starts with an article (lintian):\n${control}")
    endif()

elseif("${CASE}" STREQUAL "TheQtPluginsAreDependedOn")
    # No library links the platform plugin (offscreen, xcb, wayland) or the SVG icon engine of the
    # application icon, so dpkg-shlibdeps does not name them; without the platform plugin hitokoto
    # does not start, and Debian has them only as Recommends of libqt6gui6.
    run_control(VERSION 0.5.1 SIZE 1 DEPENDS "libc6 (>= 2.38)")
    if(NOT control_result EQUAL 0)
        message(FATAL_ERROR "control failed:\n${control_messages}")
    endif()
    expect_contains_exactly("${control}" "\nDepends: libc6 (>= 2.38), qt6-qpa-plugins, qt6-svg-plugins\n"
        "Depends with the Qt plugins")
    file(STRINGS "${RECIPE_DIR}/build-depends" build_depends)
    foreach(package IN ITEMS qt6-qpa-plugins qt6-svg-plugins)
        if(NOT package IN_LIST build_depends)
            message(FATAL_ERROR "build-depends lacks ${package} (the tests start Qt)")
        endif()
    endforeach()

elseif("${CASE}" STREQUAL "TheControlModeRejectsBadInput")
    run_control(VERSION 0.5.1 SIZE 1234 DEPENDS "")
    if(control_result EQUAL 0)
        message(FATAL_ERROR "control without Depends succeeded")
    endif()
    run_control(VERSION 0.5 SIZE 1234 DEPENDS "libc6")
    if(control_result EQUAL 0)
        message(FATAL_ERROR "control with version 0.5 succeeded")
    endif()
    run_control(VERSION 0.5.1 SIZE 12k DEPENDS "libc6")
    if(control_result EQUAL 0)
        message(FATAL_ERROR "control with size 12k succeeded")
    endif()
    file(WRITE "${WORK_DIR}/template" "Package: hitokoto\nVersion: @VERSION@-1\nX: @OTHER@\n")
    run_control(VERSION 0.5.1 SIZE 1 DEPENDS "libc6" TEMPLATE "${WORK_DIR}/template")
    if(control_result EQUAL 0)
        message(FATAL_ERROR "control with an unknown placeholder succeeded")
    endif()
    expect_contains("${control_messages}" "@OTHER@" "message")

# ---- The recipe ----

elseif("${CASE}" STREQUAL "TheMaintainerIsTheOneOfThePkgbuild")
    # Review O6 (Armin, 2026-11-12).
    recipe_maintainer(maintainer)
    file(STRINGS "${SOURCE_DIR}/packaging/archlinux/PKGBUILD" pkgbuild REGEX "^# Maintainer: " ENCODING UTF-8)
    string(REGEX REPLACE "^# Maintainer: " "" arch "${pkgbuild}")
    expect_eq("${maintainer}" "${arch}" "maintainer of the Debian package")

elseif("${CASE}" STREQUAL "TheCopyrightRefersToCommonLicenses")
    # Review O9: Debian Policy 12.5.
    file(READ "${RECIPE_DIR}/copyright" text)
    expect_contains("${text}"
        "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/" "format line")
    expect_contains("${text}" "License: GPL-3+" "licence")
    expect_contains("${text}" "/usr/share/common-licenses/GPL-3" "reference")
    expect_not_contains("${text}" "TERMS AND CONDITIONS" "the full text of the GPL")

elseif("${CASE}" STREQUAL "CPackMakesNoDeb")
    # Decision 12 (review O7).
    file(READ "${SOURCE_DIR}/CMakeLists.txt" text)
    if(text MATCHES "CPACK_GENERATOR[^)]*DEB")
        message(FATAL_ERROR "CMakeLists.txt still lets CPack make a .deb")
    endif()
    expect_not_contains("${text}" "CPACK_DEBIAN_" "CPack's Debian settings")

elseif("${CASE}" STREQUAL "TheRecipeKeepsLfLineEndings")
    # The build script runs under bash, also from a checkout on Windows.
    file(READ "${SOURCE_DIR}/.gitattributes" text)
    if(NOT text MATCHES "(^|\n)packaging/debian/\\*\\* text eol=lf\r?\n")
        message(FATAL_ERROR ".gitattributes does not keep packaging/debian at LF")
    endif()

elseif("${CASE}" STREQUAL "TheBuildTakesTheSharedDesktopEntry")
    # Review O1: one desktop entry for both recipes, where the AUR recipe needs it.
    file(READ "${RECIPE_DIR}/build-package.sh" text)
    expect_contains("${text}" "packaging/archlinux/hitokoto.desktop" "desktop entry in the build")
    if(EXISTS "${RECIPE_DIR}/hitokoto.desktop")
        message(FATAL_ERROR "packaging/debian has a desktop entry of its own")
    endif()

else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()
