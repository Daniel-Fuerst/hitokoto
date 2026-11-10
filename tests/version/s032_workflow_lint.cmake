# SPDX-License-Identifier: GPL-3.0-or-later
# S032 (code review O2): guards .github/workflows/version.yml against an accidental change that
# would let the check "Version label" run a pull request's own copy of NextVersion.cmake again.
#   cmake -DWORKFLOW=<version.yml> -P s032_workflow_lint.cmake
# Fails, naming every deviation, unless the file has no pull_request_target and its job "label"
# checks out main once, without stored credentials, and runs NextVersion.cmake in the mode labels.
# A deliberate change of the versioning adapts this test with it (story S032, decision 2).
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED WORKFLOW OR NOT EXISTS "${WORKFLOW}")
    message(FATAL_ERROR "WORKFLOW not set or missing: '${WORKFLOW}'")
endif()
message(STATUS "S032 lint: not implemented yet")
