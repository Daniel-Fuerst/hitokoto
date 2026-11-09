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
