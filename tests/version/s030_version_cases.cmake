# SPDX-License-Identifier: GPL-3.0-or-later
# S030: cases for scripts/version/NextVersion.cmake and scripts/version/HitokotoVersion.cmake, run
# with cmake -P on fixed inputs (no git history of the repository needed).
#   -DCASE=<name>          the case below
#   -DSCRIPTS_DIR=<dir>    scripts/version
#   -DWORK_DIR=<dir>       a scratch directory for this case (emptied first)
# Cases of the built tree take EXPECTED, HEADER, CPACK_CONFIG or PLIST in addition.
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE SCRIPTS_DIR WORK_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# Code review O5: git commands of these cases never follow a repository named by the environment
# (git hooks set GIT_DIR, for example); otherwise the throwaway repository below would be the
# caller's repository.
foreach(v IN ITEMS GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_CEILING_DIRECTORIES)
    unset(ENV{${v}})
endforeach()

set(next_script "${SCRIPTS_DIR}/NextVersion.cmake")
set(four_labels "version:major, version:minor, version:patch, version:none")

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

# Runs NextVersion.cmake. Named arguments: MODE, TAGS (list), LABELS (list), CANDIDATES (text).
# Sets <prefix>_CODE (exit code), <prefix>_OUTPUT (content of the output file, stripped) and
# <prefix>_LOG (stdout and stderr).
function(run_next prefix)
    cmake_parse_arguments(PARSE_ARGV 1 a "" "MODE;CANDIDATES" "TAGS;LABELS")
    set(args -DMODE=${a_MODE} -DOUTPUT=${WORK_DIR}/output.txt)
    file(REMOVE "${WORK_DIR}/output.txt")
    if("TAGS" IN_LIST ARGN)
        write_lines("${WORK_DIR}/tags.txt" ${a_TAGS})
        list(APPEND args -DTAGS_FILE=${WORK_DIR}/tags.txt)
    endif()
    if("LABELS" IN_LIST ARGN)
        write_lines("${WORK_DIR}/labels.txt" ${a_LABELS})
        list(APPEND args -DLABELS_FILE=${WORK_DIR}/labels.txt)
    endif()
    if("CANDIDATES" IN_LIST ARGN)
        file(WRITE "${WORK_DIR}/candidates.txt" "${a_CANDIDATES}")
        list(APPEND args -DCANDIDATES_FILE=${WORK_DIR}/candidates.txt)
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -P "${next_script}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    set(output "")
    if(EXISTS "${WORK_DIR}/output.txt")
        file(READ "${WORK_DIR}/output.txt" output)
        string(STRIP "${output}" output)
    endif()
    set(${prefix}_CODE "${code}" PARENT_SCOPE)
    set(${prefix}_OUTPUT "${output}" PARENT_SCOPE)
    set(${prefix}_LOG "${out}\n${err}" PARENT_SCOPE)
endfunction()

# The next tag from TAGS and one LABEL; checks success and the result.
function(expect_next label expected)
    run_next(r MODE next TAGS ${ARGN} LABELS ${label})
    expect_eq("${r_CODE}" "0" "exit code for ${label}\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "${expected}" "next version for ${label}")
endfunction()

# The commit ids below are made up; only their order and the pull request data matter.
set(c1 "1111111111111111111111111111111111111111")
set(c2 "2222222222222222222222222222222222222222")
set(c3 "3333333333333333333333333333333333333333")
set(c4 "4444444444444444444444444444444444444444")

# --- NextVersion.cmake: the level of a merge ---------------------------------------------------

if("${CASE}" STREQUAL "NextPatchMinorMajor")
    # Scenario: merging makes the next version.
    expect_next(version:patch v0.3.2 v0.3.1)
    expect_next(version:minor v0.4.0 v0.3.1)
    expect_next(version:major v1.0.0 v0.3.1)

elseif("${CASE}" STREQUAL "NoneMakesNoTag")
    # Scenario: merging makes the next version (version:none).
    expect_next(version:none none v0.3.1)

elseif("${CASE}" STREQUAL "OtherLabelsDoNotCount")
    run_next(r MODE next TAGS v0.3.1 LABELS bench-accepted version:patch "good first issue")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "v0.3.2" "next version")

elseif("${CASE}" STREQUAL "NextNeedsExactlyOneLabel")
    run_next(r MODE next TAGS v0.3.1 LABELS bench-accepted)
    expect_eq("${r_CODE}" "1" "exit code without a version label")
    expect_contains("${r_LOG}" "${four_labels}" "message without a version label")
    expect_eq("${r_OUTPUT}" "" "output without a version label")
    run_next(r MODE next TAGS v0.3.1 LABELS version:patch version:minor)
    expect_eq("${r_CODE}" "1" "exit code with two version labels")
    expect_contains("${r_LOG}" "${four_labels}" "message with two version labels")

# --- NextVersion.cmake: the check "Version label" ----------------------------------------------

elseif("${CASE}" STREQUAL "LabelCheck")
    # Scenario: a pull request into main needs exactly one version label.
    run_next(r MODE labels LABELS)
    expect_eq("${r_CODE}" "1" "exit code without labels")
    expect_contains("${r_LOG}" "${four_labels}" "message without labels")
    run_next(r MODE labels LABELS bench-accepted)
    expect_eq("${r_CODE}" "1" "exit code without a version label")
    expect_contains("${r_LOG}" "${four_labels}" "message without a version label")
    run_next(r MODE labels LABELS version:minor version:none)
    expect_eq("${r_CODE}" "1" "exit code with two version labels")
    expect_contains("${r_LOG}" "${four_labels}" "message with two version labels")
    expect_contains("${r_LOG}" "version:minor, version:none" "message names the labels it found")
    foreach(label IN ITEMS version:major version:minor version:patch version:none)
        run_next(r MODE labels LABELS bench-accepted ${label})
        expect_eq("${r_CODE}" "0" "exit code with ${label}\n${r_LOG}")
    endforeach()

# --- NextVersion.cmake: the base tag -----------------------------------------------------------

elseif("${CASE}" STREQUAL "OnlyExactVersionTagsCount")
    # Scenario: only exact version tags reachable from the merge count. The tag list is what
    # `git tag --merged <merge>` gives, so a tag on a commit that is not an ancestor (v9.0.0) is
    # not in it.
    run_next(r MODE base TAGS v0.3.0 v0.3.0-tulip v0.10.0-rc1 release-2 V1.0.0 v1.2 v01.0.0 v1.0.0.1)
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "v0.3.0" "base tag")
    expect_next(version:patch v0.3.1 v0.3.0 v0.3.0-tulip v0.10.0-rc1 release-2)

elseif("${CASE}" STREQUAL "BaseIsTheHighestVersion")
    # Numeric comparison: v0.10.0 beats v0.9.0, whatever the order of the list.
    run_next(r MODE base TAGS v0.10.0 v0.9.0 v0.2.11)
    expect_eq("${r_OUTPUT}" "v0.10.0" "base tag")
    expect_next(version:minor v0.11.0 v0.9.0 v0.10.0)

elseif("${CASE}" STREQUAL "NoVersionTagFails")
    # Scenario: no version tag, no new version.
    run_next(r MODE base TAGS v0.3.0-tulip release-2)
    expect_eq("${r_CODE}" "1" "exit code of base")
    expect_contains("${r_LOG}" "no version tag" "message of base")
    expect_eq("${r_OUTPUT}" "" "output of base")
    run_next(r MODE next TAGS LABELS version:patch)
    expect_eq("${r_CODE}" "1" "exit code of next")
    expect_contains("${r_LOG}" "no version tag" "message of next")
    expect_eq("${r_OUTPUT}" "" "output of next")

# --- NextVersion.cmake: the plan of the tag job ------------------------------------------------

elseif("${CASE}" STREQUAL "FirstRunFromTheStartingTag")
    # Scenario: the first run starts from the starting tag.
    run_next(r MODE plan TAGS v0.3.0 v0.3.0-tulip CANDIDATES
"commit ${c1}
pr 31
label version:none
commit ${c2}
pr 33
label version:none
label bench-accepted
commit ${c3}
pr 32
label version:patch
")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "${c3} v0.3.1 32" "plan")

elseif("${CASE}" STREQUAL "OnlyMergesOfPullRequestsIntoMain")
    # Scenario: only merges of pull requests into main make versions. Commits without a pull request
    # into main (pushed directly) come without a pr line; merges into other branches are reached only
    # through a second parent and are never candidates.
    run_next(r MODE plan TAGS v0.3.0 CANDIDATES
"commit ${c1}
commit ${c2}
merge
pr 26
label version:minor
commit ${c3}
")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "${c2} v0.4.0 26" "plan")

elseif("${CASE}" STREQUAL "AnUnconfirmedMergeStops")
    # Code review O1: a merge commit (two parents, marked "merge") that no pull request into main
    # confirms is never skipped as if it were pushed directly: the job stops there, after tagging
    # the merges before it, so that a re-run can tag it once GitHub finds its pull request.
    run_next(r MODE plan TAGS v0.3.1 CANDIDATES
"commit ${c1}
merge
pr 80
label version:patch
commit ${c2}
commit ${c3}
merge
commit ${c4}
merge
pr 81
label version:patch
")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "The merge commit ${c3}" "message names the merge commit")
    expect_contains("${r_LOG}" "not confirmed as the merge of a pull request into main"
        "message says why")
    expect_eq("${r_OUTPUT}" "${c1} v0.3.2 80" "plan")

elseif("${CASE}" STREQUAL "NoneThenPatch")
    # Scenario: merging makes the next version (version:none, then version:patch).
    run_next(r MODE plan TAGS v0.3.1 CANDIDATES
"commit ${c1}
pr 40
label version:none
commit ${c2}
pr 41
label version:patch
")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "${c2} v0.3.2 41" "plan")

elseif("${CASE}" STREQUAL "MergesInQuickSuccession")
    # Scenario: merges in quick succession each get their version, in merge order.
    run_next(r MODE plan TAGS v0.3.1 CANDIDATES
"commit ${c1}
pr 40
label version:patch
commit ${c2}
pr 41
label version:minor
commit ${c3}
pr 42
label version:patch
")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "${c1} v0.3.2 40\n${c2} v0.4.0 41\n${c3} v0.4.1 42" "plan")

elseif("${CASE}" STREQUAL "MissingLabelStops")
    # Scenario: a later merge waits for an earlier merge without a label.
    run_next(r MODE plan TAGS v0.3.1 CANDIDATES
"commit ${c1}
pr 50
label bench-accepted
commit ${c2}
pr 51
label version:patch
")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "pull request #50" "message names the pull request")
    expect_contains("${r_LOG}" "Set its label and re-run this job" "message says what to do")
    expect_contains("${r_LOG}" "${four_labels}" "message names the labels")
    expect_eq("${r_OUTPUT}" "" "plan")

elseif("${CASE}" STREQUAL "MissingLabelAfterAVersion")
    # Merges before the one without a label are tagged; nothing after it.
    run_next(r MODE plan TAGS v0.3.1 CANDIDATES
"commit ${c1}
pr 60
label version:patch
commit ${c2}
commit ${c3}
pr 61
label version:patch
label version:minor
commit ${c4}
pr 62
label version:patch
")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "pull request #61" "message names the pull request")
    expect_eq("${r_OUTPUT}" "${c1} v0.3.2 60" "plan")

elseif("${CASE}" STREQUAL "NothingToTag")
    # Scenario: tagging is idempotent (the merge is the last version tag, so no candidates follow).
    run_next(r MODE plan TAGS v0.4.0 CANDIDATES "")
    expect_eq("${r_CODE}" "0" "exit code\n${r_LOG}")
    expect_eq("${r_OUTPUT}" "" "plan")

elseif("${CASE}" STREQUAL "PlanWithoutVersionTagFails")
    run_next(r MODE plan TAGS release-2 CANDIDATES "commit ${c1}\npr 70\nlabel version:patch\n")
    expect_eq("${r_CODE}" "1" "exit code")
    expect_contains("${r_LOG}" "no version tag" "message")
    expect_eq("${r_OUTPUT}" "" "plan")

elseif("${CASE}" STREQUAL "CarriageReturnsAreIgnored")
    # Input files written on Windows may end their lines with CRLF.
    file(WRITE "${WORK_DIR}/tags.txt" "v0.3.1\r\n")
    file(WRITE "${WORK_DIR}/labels.txt" "version:patch\r\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=next -DTAGS_FILE=${WORK_DIR}/tags.txt
                            -DLABELS_FILE=${WORK_DIR}/labels.txt -DOUTPUT=${WORK_DIR}/output.txt
                            -P "${next_script}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    expect_eq("${code}" "0" "exit code\n${out}\n${err}")
    file(READ "${WORK_DIR}/output.txt" output)
    string(STRIP "${output}" output)
    expect_eq("${output}" "v0.3.2" "next version")

# --- HitokotoVersion.cmake: the version of a build ---------------------------------------------

else()
    include("${SCRIPTS_DIR}/HitokotoVersion.cmake")

    if("${CASE}" STREQUAL "DescribeOnTheTag")
        # Scenario: the build reports the version (on the tagged commit).
        hitokoto_version_from_describe("v0.4.0" full core)
        expect_eq("${full}" "0.4.0" "full version")
        expect_eq("${core}" "0.4.0" "numeric core")

    elseif("${CASE}" STREQUAL "DescribeAfterTheTag")
        hitokoto_version_from_describe("v0.4.0-3-gabc1234" full core)
        expect_eq("${full}" "0.4.0+3.gabc1234" "full version")
        expect_eq("${core}" "0.4.0" "numeric core")
        hitokoto_version_from_describe("v0.10.12-140-g0123456789ab" full core)
        expect_eq("${full}" "0.10.12+140.g0123456789ab" "full version with a longer hash")
        expect_eq("${core}" "0.10.12" "numeric core with a longer hash")

    elseif("${CASE}" STREQUAL "DescribeDirty")
        hitokoto_version_from_describe("v0.4.0-3-gabc1234-dirty" full core)
        expect_eq("${full}" "0.4.0+3.gabc1234.dirty" "full version after the tag")
        expect_eq("${core}" "0.4.0" "numeric core after the tag")
        hitokoto_version_from_describe("v0.4.0-dirty" full core)
        expect_eq("${full}" "0.4.0+dirty" "full version on the tag")
        expect_eq("${core}" "0.4.0" "numeric core on the tag")

    elseif("${CASE}" STREQUAL "DescribeOfAnotherTagIsRejected")
        foreach(d IN ITEMS "v0.3.0-tulip" "v0.3.0-tulip-2-gabc1234" "release-2" "" "abc1234")
            hitokoto_version_from_describe("${d}" full core)
            expect_eq("${full}" "" "full version of '${d}'")
            expect_eq("${core}" "" "numeric core of '${d}'")
        endforeach()

    elseif("${CASE}" STREQUAL "VersionFromTheOption")
        # Scenario: builds without git (-DHITOKOTO_VERSION=0.4.0); the option also wins over git.
        hitokoto_determine_version(SOURCE_DIR "${WORK_DIR}" OPTION 0.4.0 NO_GIT
            FULL full CORE core WARNING warning SOURCE source)
        expect_eq("${full}" "0.4.0" "full version")
        expect_eq("${core}" "0.4.0" "numeric core")
        expect_eq("${warning}" "" "warning")
        expect_eq("${source}" "HITOKOTO_VERSION" "source")
        hitokoto_determine_version(SOURCE_DIR "${WORK_DIR}" OPTION 0.4.0+3.gabc1234 NO_GIT
            FULL full CORE core WARNING warning)
        expect_eq("${full}" "0.4.0+3.gabc1234" "full version with build metadata")
        expect_eq("${core}" "0.4.0" "numeric core with build metadata")

    elseif("${CASE}" STREQUAL "InvalidOptionFails")
        file(WRITE "${WORK_DIR}/invalid.cmake"
            "include(\"${SCRIPTS_DIR}/HitokotoVersion.cmake\")\n"
            "hitokoto_determine_version(SOURCE_DIR \"${WORK_DIR}\" OPTION 0.4 NO_GIT\n"
            "    FULL full CORE core WARNING warning)\n")
        execute_process(COMMAND "${CMAKE_COMMAND}" -P "${WORK_DIR}/invalid.cmake"
            RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
        expect_eq("${code}" "1" "exit code")
        expect_contains("${out}\n${err}" "HITOKOTO_VERSION" "message")

    elseif("${CASE}" STREQUAL "UnknownWithoutGit")
        # Scenario: builds without git, without the option.
        hitokoto_determine_version(SOURCE_DIR "${WORK_DIR}" NO_GIT FULL full CORE core WARNING warning
            SOURCE source)
        expect_eq("${full}" "0.0.0+unknown" "full version")
        expect_eq("${core}" "0.0.0" "numeric core")
        expect_contains("${warning}" "HITOKOTO_VERSION" "warning")
        expect_eq("${source}" "none" "source")

    elseif("${CASE}" STREQUAL "GitRepository")
        # A throwaway repository: git describe with the real tags rules. Skipped without git.
        find_package(Git QUIET)
        if(NOT GIT_FOUND)
            message("S030-SKIP: git not found")
            return()
        endif()
        set(repo "${WORK_DIR}/repo")
        file(MAKE_DIRECTORY "${repo}")
        set(git "${GIT_EXECUTABLE}" -C "${repo}" -c user.name=S030 -c user.email=s030@example.invalid
                -c commit.gpgsign=false -c tag.gpgsign=false -c core.autocrlf=false)
        function(git_run)
            execute_process(COMMAND ${git} ${ARGN} RESULT_VARIABLE code OUTPUT_QUIET ERROR_VARIABLE err)
            if(NOT code EQUAL 0)
                message(FATAL_ERROR "git ${ARGN}: ${err}")
            endif()
        endfunction()
        function(short_head out)
            execute_process(COMMAND ${git} rev-parse --short=7 HEAD OUTPUT_VARIABLE h
                OUTPUT_STRIP_TRAILING_WHITESPACE)
            set(${out} "${h}" PARENT_SCOPE)
        endfunction()
        function(expect_version expected what)
            hitokoto_determine_version(SOURCE_DIR "${repo}" FULL full CORE core WARNING warning
                SOURCE source)
            expect_eq("${full}" "${expected}" "${what}")
            expect_eq("${warning}" "" "warning (${what})")
            expect_eq("${source}" "git describe" "source (${what})")
        endfunction()

        git_run(init -q)
        file(WRITE "${repo}/a.txt" "a\n")
        git_run(add a.txt)
        git_run(commit -q -m a)
        hitokoto_determine_version(SOURCE_DIR "${repo}" FULL full CORE core WARNING warning)
        expect_eq("${full}" "0.0.0+unknown" "full version without a tag")
        expect_contains("${warning}" "version tag" "warning without a tag")
        if(warning MATCHES ";")
            message(FATAL_ERROR "warning without a tag is a list: ${warning}")
        endif()

        git_run(tag -a v0.3.0 -m v0.3.0)
        expect_version("0.3.0" "on v0.3.0")

        file(WRITE "${repo}/a.txt" "b\n")
        git_run(commit -q -a -m b)
        git_run(tag v0.3.0-tulip)
        git_run(tag -a v0.10.0-rc1 -m rc)
        git_run(tag -a release-2 -m release)
        short_head(b)
        expect_version("0.3.0+1.g${b}" "one commit after v0.3.0, other tags on it")

        file(WRITE "${repo}/a.txt" "c\n")
        expect_version("0.3.0+1.g${b}.dirty" "uncommitted change")

        git_run(commit -q -a -m c)
        git_run(tag -a v0.4.0 -m v0.4.0)
        expect_version("0.4.0" "on v0.4.0")
        file(WRITE "${repo}/a.txt" "d\n")
        expect_version("0.4.0+dirty" "uncommitted change on v0.4.0")

        # Code review O4: the option wins over git, but in a checkout with a version tag it is
        # reported, so that a value left in the cache is not taken for the version of the commit.
        hitokoto_determine_version(SOURCE_DIR "${repo}" OPTION 0.9.0 FULL full CORE core
            WARNING warning SOURCE source)
        expect_eq("${full}" "0.9.0" "full version from the option in a checkout")
        expect_eq("${source}" "HITOKOTO_VERSION" "source of the option in a checkout")
        expect_contains("${warning}" "HITOKOTO_VERSION=0.9.0 overrides the version from git (0.4.0+dirty)"
            "warning for the option in a checkout")
        expect_contains("${warning}" "-DHITOKOTO_VERSION=" "warning says how to remove the option")

        # A source tree without .git inside another repository (an unpacked source archive) does
        # not take the version of the outer repository.
        file(MAKE_DIRECTORY "${repo}/archive")
        hitokoto_determine_version(SOURCE_DIR "${repo}/archive" FULL full CORE core WARNING warning)
        expect_eq("${full}" "0.0.0+unknown" "source archive inside a repository")
        expect_contains("${warning}" "HITOKOTO_VERSION" "warning for the source archive")
        hitokoto_determine_version(SOURCE_DIR "${repo}/archive" OPTION 0.9.0 FULL full CORE core
            WARNING warning)
        expect_eq("${full}" "0.9.0" "source archive with the option")
        expect_eq("${warning}" "" "no warning for the option in a source archive")

    elseif("${CASE}" STREQUAL "GitEnvironmentIsIgnored")
        # Code review O5: GIT_DIR and GIT_WORK_TREE of the caller (a git hook, an IDE) do not
        # redirect the version to another repository. Skipped without git.
        find_package(Git QUIET)
        if(NOT GIT_FOUND)
            message("S030-SKIP: git not found")
            return()
        endif()
        foreach(name IN ITEMS repo decoy)
            set(dir "${WORK_DIR}/${name}")
            file(MAKE_DIRECTORY "${dir}")
            set(git "${GIT_EXECUTABLE}" -C "${dir}" -c user.name=S030 -c user.email=s030@example.invalid
                    -c commit.gpgsign=false -c tag.gpgsign=false -c core.autocrlf=false)
            file(WRITE "${dir}/a.txt" "${name}\n")
            foreach(step IN ITEMS "init;-q" "add;a.txt" "commit;-q;-m;a")
                execute_process(COMMAND ${git} ${step} RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_QUIET)
                if(NOT code EQUAL 0)
                    message(FATAL_ERROR "git ${step} in ${name}: ${err}")
                endif()
            endforeach()
        endforeach()
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK_DIR}/repo" -c tag.gpgsign=false
            -c user.name=S030 -c user.email=s030@example.invalid tag -a v0.3.0 -m v0.3.0)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK_DIR}/decoy" -c tag.gpgsign=false
            -c user.name=S030 -c user.email=s030@example.invalid tag -a v9.0.0 -m v9.0.0)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK_DIR}/decoy" rev-parse HEAD
            OUTPUT_VARIABLE decoy_head OUTPUT_STRIP_TRAILING_WHITESPACE)

        set(ENV{GIT_DIR} "${WORK_DIR}/decoy/.git")
        set(ENV{GIT_WORK_TREE} "${WORK_DIR}/decoy")
        hitokoto_determine_version(SOURCE_DIR "${WORK_DIR}/repo" FULL full CORE core WARNING warning)
        expect_eq("${full}" "0.3.0" "version with GIT_DIR naming another repository")
        expect_eq("$ENV{GIT_DIR}" "${WORK_DIR}/decoy/.git" "GIT_DIR of the caller is kept")
        unset(ENV{GIT_DIR})
        unset(ENV{GIT_WORK_TREE})
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${WORK_DIR}/decoy" rev-parse HEAD
            OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE)
        expect_eq("${head}" "${decoy_head}" "HEAD of the other repository")

    # --- the configured build -------------------------------------------------------------------

    elseif("${CASE}" STREQUAL "TheConfiguredVersion")
        # The version the build was configured with is a valid one (0.0.0+unknown in a checkout
        # without tags, as in the CI test jobs).
        hitokoto_version_from_option("${EXPECTED}" full core)
        expect_eq("${full}" "${EXPECTED}" "configured version")

    elseif("${CASE}" STREQUAL "TheAppIsBuiltWithTheVersion")
        # Scenario: the build reports the version (QApplication::applicationVersion, set in main.cpp
        # from this header).
        file(READ "${HEADER}" text)
        expect_contains("${text}" "\"${EXPECTED}\"" "generated header")

    elseif("${CASE}" STREQUAL "TheAppReportsTheVersion")
        # QCommandLineParser::addVersionOption prints applicationVersion. On Windows a GUI program
        # shows it in a message box unless its output is redirected (as here) or this variable is
        # set (qcommandlineparser.cpp, displayMessageBox; code review O8).
        set(ENV{QT_QPA_PLATFORM} offscreen)
        set(ENV{QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES} 1)
        execute_process(COMMAND "${APP}" --version RESULT_VARIABLE code OUTPUT_VARIABLE out
            ERROR_VARIABLE err TIMEOUT 60)
        expect_eq("${code}" "0" "exit code of --version\n${out}\n${err}")
        string(STRIP "${out}" out)
        expect_eq("${out}" "hitokoto ${EXPECTED}" "output of --version")

    elseif("${CASE}" STREQUAL "CPackCarriesTheFullVersion")
        include("${CPACK_CONFIG}")
        expect_eq("${CPACK_PACKAGE_VERSION}" "${EXPECTED}" "CPACK_PACKAGE_VERSION")

    elseif("${CASE}" STREQUAL "MacBundleCarriesTheCore")
        file(READ "${PLIST}" text)
        string(REGEX REPLACE "[ \t\r\n]+" "" text "${text}")
        expect_contains("${text}" "<key>CFBundleShortVersionString</key><string>${EXPECTED}</string>"
            "CFBundleShortVersionString")
        expect_contains("${text}" "<key>CFBundleVersion</key><string>${EXPECTED}</string>" "CFBundleVersion")

    else()
        message(FATAL_ERROR "unknown case '${CASE}'")
    endif()
endif()
