# SPDX-License-Identifier: GPL-3.0-or-later
# S036: lints over packaging.yml for the Debian package and the GitHub releases: only the release
# job may write, it runs only on main, never cancelled, and the Debian jobs do not depend on the
# AUR jobs. Run with cmake -P; the file is read as text (no YAML parser).
#   -DCASE=<name>            the case below
#   -DWORKFLOWS_DIR=<dir>    .github/workflows
cmake_minimum_required(VERSION 3.25)

foreach(v IN ITEMS CASE WORKFLOWS_DIR)
    if(NOT DEFINED ${v} OR "${${v}}" STREQUAL "")
        message(FATAL_ERROR "${v} not set")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/workflow_lines.cmake")

set(debian_jobs debian-plan debian-package debian-release)
set(aur_jobs build handover publish)

read_workflow(packaging.yml packaging)
jobs("${packaging}" job)
foreach(id IN LISTS debian_jobs aur_jobs)
    if(NOT id IN_LIST job_ids)
        message(FATAL_ERROR "packaging.yml has no job '${id}' (jobs: ${job_ids})")
    endif()
endforeach()

if("${CASE}" STREQUAL "OnlyTheReleaseJobMayWrite")
    # Decision 8: only the release job has contents: write; no job gets write-all.
    foreach(id IN LISTS job_ids)
        list(JOIN job_${id} "\n" text)
        if(text MATCHES "write-all")
            message(FATAL_ERROR "the job ${id} has write-all")
        endif()
        if(text MATCHES ": write" AND NOT id STREQUAL "debian-release")
            message(FATAL_ERROR "the job ${id} gets a write permission (S036 decision 8)")
        endif()
    endforeach()
    list(JOIN job_debian-release "\n" release)
    if(NOT release MATCHES "\n    permissions:\n      contents: write\n    [a-z]")
        message(FATAL_ERROR "the job debian-release does not have exactly contents: write")
    endif()
    list(JOIN packaging "\n" whole)
    if(NOT whole MATCHES "(^|\n)permissions: {}\n")
        message(FATAL_ERROR "packaging.yml does not start from permissions: {}")
    endif()

elseif("${CASE}" STREQUAL "TheReleaseJobRunsOnlyOnMain")
    # Decisions 8 and 10: never on a pull request; after a CI run on main or a manual run with
    # "publish"; never cancelled while running.
    job_condition("${job_debian-release}" condition)
    foreach(part IN ITEMS "!cancelled()" "needs.debian-plan.result == 'success'"
                          "needs.debian-plan.outputs.action == 'release'"
                          "github.ref == 'refs/heads/main'" "github.event_name == 'workflow_run'"
                          "(github.event_name == 'workflow_dispatch' && inputs.publish)")
        string(FIND "${condition}" "${part}" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "the condition of debian-release lacks ${part}: '${condition}'")
        endif()
    endforeach()
    if(condition MATCHES "pull_request")
        message(FATAL_ERROR "the condition of debian-release mentions pull_request: '${condition}'")
    endif()
    list(JOIN job_debian-release "\n" release)
    if(NOT release MATCHES "\n    concurrency:\n      group: releases\n      cancel-in-progress: false\n")
        message(FATAL_ERROR "debian-release is not in the group releases without cancelling")
    endif()

elseif("${CASE}" STREQUAL "TheDebianJobsDoNotWaitForTheAur")
    # Decision 1: the AUR and the releases do not wait for each other.
    foreach(id IN LISTS debian_jobs)
        job_value("${job_${id}}" needs needs)
        foreach(other IN LISTS aur_jobs)
            if(needs MATCHES "(^|[^a-z-])${other}([^a-z-]|$)")
                message(FATAL_ERROR "the job ${id} needs the AUR job ${other}")
            endif()
        endforeach()
    endforeach()
    foreach(id IN LISTS aur_jobs)
        job_value("${job_${id}}" needs needs)
        if(needs MATCHES "debian-")
            message(FATAL_ERROR "the AUR job ${id} needs a Debian job")
        endif()
    endforeach()

elseif("${CASE}" STREQUAL "OneVersionDoesNotStopTheOthers")
    # Decision 2 (review O2): a matrix entry per version without fail-fast, and the release job
    # releases what passed even if another version failed.
    list(JOIN job_debian-package "\n" package)
    if(NOT package MATCHES "\n      fail-fast: false\n")
        message(FATAL_ERROR "the matrix of debian-package can fail fast")
    endif()
    if(NOT package MATCHES "version: \\$\\{\\{ fromJSON\\(needs.debian-plan.outputs.build\\) \\}\\}")
        message(FATAL_ERROR "the matrix of debian-package does not come from the plan")
    endif()
    job_condition("${job_debian-release}" condition)
    if(condition MATCHES "needs.debian-package.result")
        message(FATAL_ERROR "debian-release waits for every version: '${condition}'")
    endif()

elseif("${CASE}" STREQUAL "TheReleaseFindsEveryPackage")
    # Fix of 2026-11-13: download-artifact puts a single matched artifact directly into its path,
    # several into one directory each (run on main after pull request 43: "0.5.1 has no checked
    # package"). Every file carries its version and all land in one flat directory.
    list(JOIN job_debian-package "\n" package)
    if(NOT package MATCHES "debian-work/out/notes-\\$\\{\\{ matrix.version \\}\\}.md")
        message(FATAL_ERROR "debian-package does not hand on notes-<version>.md")
    endif()
    list(JOIN job_debian-release "\n" release)
    if(NOT release MATCHES "\n          pattern: debian-\\*\n          merge-multiple: true\n")
        message(FATAL_ERROR "debian-release does not download the packages into one directory")
    endif()
    if(release MATCHES "debian-\\$version" OR NOT release MATCHES "notes-\\$version.md")
        message(FATAL_ERROR "debian-release looks for the files of a version in a directory of its own")
    endif()

elseif("${CASE}" STREQUAL "TheReleaseWaitsForItsDraft")
    # Fix of 2026-11-15 (the packaging run of v0.5.4): right after gh release create the list of
    # the releases did not hold the new draft yet ("expected one draft of v0.5.4, found:"). The
    # step asks again in a loop with a pause until a draft is listed; the check for exactly one
    # draft comes after the loop.
    list(JOIN job_debian-release "\n" release)
    string(FIND "${release}" "gh release create" create)
    if(create EQUAL -1)
        message(FATAL_ERROR "debian-release does not create a release")
    endif()
    string(SUBSTRING "${release}" ${create} -1 after)
    string(REGEX MATCH "\n *(for|while|until) [^\n]*do\n" loop "${after}")
    if(loop STREQUAL "")
        message(FATAL_ERROR "debian-release has no loop after gh release create")
    endif()
    string(FIND "${after}" "${loop}" loop_at)
    string(SUBSTRING "${after}" 0 ${loop_at} before_loop)
    if(before_loop MATCHES "releases_of")
        message(FATAL_ERROR "debian-release asks for the drafts once before the loop")
    endif()
    string(SUBSTRING "${after}" ${loop_at} -1 from_loop)
    string(REGEX MATCH "\n *done\n" done "${from_loop}")
    if(done STREQUAL "")
        message(FATAL_ERROR "the loop after gh release create has no done")
    endif()
    string(FIND "${from_loop}" "${done}" done_at)
    string(SUBSTRING "${from_loop}" 0 ${done_at} body)
    if(NOT body MATCHES "releases_of" OR NOT body MATCHES "\n *sleep [0-9]")
        message(FATAL_ERROR "the loop after gh release create does not ask for the drafts again after a pause")
    endif()
    string(SUBSTRING "${from_loop}" ${done_at} -1 after_loop)
    if(NOT after_loop MATCHES "expected one draft of")
        message(FATAL_ERROR "the check for exactly one draft does not follow the loop")
    endif()

else()
    message(FATAL_ERROR "unknown case '${CASE}'")
endif()
