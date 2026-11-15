# SPDX-License-Identifier: GPL-3.0-or-later
# S030: the version scripts (scripts/version) through cmake -P on fixed inputs, and the version in
# the configured build.
set(s030_scripts "${PROJECT_SOURCE_DIR}/scripts/version")
set(s030_cases "${CMAKE_CURRENT_SOURCE_DIR}/s030_version_cases.cmake")

function(s030_case name)
    add_test(NAME S030_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPTS_DIR=${s030_scripts}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/${name} ${ARGN} -P ${s030_cases})
endfunction()

foreach(name IN ITEMS
        NextPatchMinorMajor
        NoneMakesNoTag
        OtherLabelsDoNotCount
        NextNeedsExactlyOneLabel
        LabelCheck
        OnlyExactVersionTagsCount
        BaseIsTheHighestVersion
        NoVersionTagFails
        FirstRunFromTheStartingTag
        OnlyMergesOfPullRequestsIntoMain
        AnUnconfirmedMergeStops
        NoneThenPatch
        MergesInQuickSuccession
        MissingLabelStops
        MissingLabelAfterAVersion
        NothingToTag
        PlanWithoutVersionTagFails
        CarriageReturnsAreIgnored
        DescribeOnTheTag
        DescribeAfterTheTag
        DescribeDirty
        DescribeOfAnotherTagIsRejected
        VersionFromTheOption
        InvalidOptionFails
        UnknownWithoutGit
        GitRepository
        GitEnvironmentIsIgnored)
    s030_case(${name})
endforeach()
set_tests_properties(S030_GitRepository S030_GitEnvironmentIsIgnored PROPERTIES
    SKIP_REGULAR_EXPRESSION "S030-SKIP")

s030_case(TheConfiguredVersion "-DEXPECTED=${HITOKOTO_VERSION_FULL}")
s030_case(CPackCarriesTheFullVersion "-DEXPECTED=${HITOKOTO_VERSION_FULL}"
    "-DCPACK_CONFIG=${PROJECT_BINARY_DIR}/CPackConfig.cmake")

if(TARGET hitokoto)
    s030_case(TheAppIsBuiltWithTheVersion "-DEXPECTED=${HITOKOTO_VERSION_FULL}"
        "-DHEADER=${PROJECT_BINARY_DIR}/src/app/hitokoto_version.hpp")
    s030_case(TheAppReportsTheVersion "-DEXPECTED=${HITOKOTO_VERSION_FULL}"
        "-DAPP=$<TARGET_FILE:hitokoto>")
    if(APPLE)
        s030_case(MacBundleCarriesTheCore "-DEXPECTED=${HITOKOTO_VERSION_CORE}"
            "-DPLIST=$<TARGET_BUNDLE_CONTENT_DIR:hitokoto>/Info.plist")
    endif()
endif()

# S032 (code review O2): the check "Version label" in version.yml runs NextVersion.cmake from main;
# a lint guards the workflow file against an accidental change, and its own cases show that it
# finds each deviation.
set(s032_workflow "${PROJECT_SOURCE_DIR}/.github/workflows/version.yml")
set(s032_lint "${CMAKE_CURRENT_SOURCE_DIR}/s032_workflow_lint.cmake")
add_test(NAME S032_TheLabelCheckUsesMainsScript
    COMMAND ${CMAKE_COMMAND} -DWORKFLOW=${s032_workflow} -P ${s032_lint})
foreach(name IN ITEMS
        Unchanged
        CheckoutWithoutMain
        CheckoutOfAnotherRef
        SecondCheckout
        StoredCredentials
        NoScript
        PullRequestTarget
        NoLabelJob)
    add_test(NAME S032_LintFinds${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DWORKFLOW=${s032_workflow} -DLINT=${s032_lint}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s032_${name}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/s032_lint_cases.cmake)
endforeach()
