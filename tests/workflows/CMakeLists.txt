# SPDX-License-Identifier: GPL-3.0-or-later
# S035: lints over the workflow files (a version only after a green CI on main).
set(s035_cases "${CMAKE_CURRENT_SOURCE_DIR}/s035_wiring_cases.cmake")
foreach(name IN ITEMS
        TheVersionJobNeedsEveryCiJob
        CiRunsOnMainAreNotCancelled
        VersionHasNoTriggerOfItsOwnOnMain
        PackagingFollowsTheCi)
    add_test(NAME S035_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name}
                -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows -P ${s035_cases})
endforeach()

# S036: lints over packaging.yml (the Debian package and the GitHub releases).
set(s036_cases "${CMAKE_CURRENT_SOURCE_DIR}/s036_release_cases.cmake")
foreach(name IN ITEMS
        OnlyTheReleaseJobMayWrite
        TheReleaseJobRunsOnlyOnMain
        TheDebianJobsDoNotWaitForTheAur
        OneVersionDoesNotStopTheOthers
        TheReleaseFindsEveryPackage
        TheReleaseWaitsForItsDraft)
    add_test(NAME S036_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name}
                -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows -P ${s036_cases})
endforeach()

# S037: lints over packaging.yml (the Windows installer in the same releases).
set(s037_cases "${CMAKE_CURRENT_SOURCE_DIR}/s037_release_cases.cmake")
foreach(name IN ITEMS
        TheWindowsJobsOnlyRead
        TheWindowsJobsRunOnTheCiImage
        OneWindowsVersionDoesNotStopTheOthers
        OnlyACheckedInstallerIsReleased
        TheReleaseWaitsForBothPackages
        TheWindowsRecipeStartsADryRun
        TheEulaIsAcceptedBeforeWixRuns
        AnEmptyIceListReachesTheBuild
        AFailedBuildDoesNotSkipTheOtherChecks
        AReleaseWithoutAnyMsiIsNotStopped
        TheHandOverOfTheMsiIsCheckedInADryRun
        WindowsInstallsAqtinstallFromAPinnedCommit)
    add_test(NAME S037_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name}
                -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows -P ${s037_cases})
endforeach()

# S049: lints over packaging.yml (the macOS disk image in the same releases).
set(s049_cases "${CMAKE_CURRENT_SOURCE_DIR}/s049_release_cases.cmake")
foreach(name IN ITEMS
        TheMacosJobsOnlyRead
        TheMacosJobsRunOnTheCiImage
        OneMacosVersionDoesNotStopTheOthers
        OnlyACheckedDiskImageIsReleased
        TheReleaseWaitsForTheDiskImage
        TheMacosRecipeStartsADryRun
        TheHandOverOfTheDiskImageIsCheckedInADryRun
        TheMacosJobsTakeTheToolsOfThePlan)
    add_test(NAME S049_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name}
                -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows -P ${s049_cases})
endforeach()

# S000: lints over every workflow file (actions from a fixed commit, the rights of each token, what
# the CI checks beside the suite).
set(s000_cases "${CMAKE_CURRENT_SOURCE_DIR}/s000_workflow_cases.cmake")
foreach(name IN ITEMS
        EveryActionIsPinnedToACommit
        AqtinstallIsPinnedToOneVersion
        EveryWorkflowStartsWithoutPermissions
        EveryCiJobAsksForItsPermissions
        OnlyTheStepThatCallsGhHasTheToken
        TheUiTestsRunWithSanitizers
        TheFuzzJobRunsEveryTarget
        PartOfTheFuzzTimeLimitsEachInput
        TheFuzzCorpusIsKept)
    add_test(NAME S000_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows
                -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${s000_cases})
endforeach()

# S042: the decision of the job "Changes" (scripts/ci/DocsOnly.cmake) on fixed inputs.
set(s042_cases "${CMAKE_CURRENT_SOURCE_DIR}/s042_docs_only_cases.cmake")
foreach(name IN ITEMS
        OnlyDocumentationSkipsTheBuild
        WithoutTheLabelEveryJobRuns
        AWrongLabelNamesEachFile
        AFileMovedIntoDocsCountsWithItsOldPath
        OtherFilesOutsideDocsNeverCount
        CarriageReturnsAreIgnored)
    add_test(NAME S042_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${PROJECT_SOURCE_DIR}/scripts/ci/DocsOnly.cmake
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s042_${name} -P ${s042_cases})
endforeach()

# S042: lints over ci.yml (pull requests that change only documentation skip the build), each shown
# red on a broken copy, and the script of the job "CI result" on the results GitHub hands it.
set(s042_ci "${CMAKE_CURRENT_SOURCE_DIR}/s042_ci_cases.cmake")
set(s042_list "${PROJECT_SOURCE_DIR}/tests/docs-read-by-tests.txt")
foreach(name IN ITEMS
        ThePullRequestTriggerHasNoLabelEvents
        ChangesReadsTheLabelsAtRunTime
        EveryOtherJobWaitsForChanges
        TheCiResultNeedsEveryJob
        ThePushFilterFollowsTheList)
    add_test(NAME S042_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows
                -DLIST_FILE=${s042_list} -P ${s042_ci})
endforeach()
foreach(name IN ITEMS
        Unchanged
        LabelEvents
        LabelsFromTheEvent
        ChangesWithWriteRights
        AJobWithoutChanges
        AJobWithoutTheCondition
        CiResultWithoutAJob
        CiResultNotAlways
        PushFilterWithoutTheList
        PushFilterInAnotherOrder)
    add_test(NAME S042_LintFinds${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DWORKFLOW=${PROJECT_SOURCE_DIR}/.github/workflows/ci.yml
                -DLINT=${s042_ci} -DLIST_FILE=${s042_list}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s042_lint_${name}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/s042_ci_lint_cases.cmake)
endforeach()
if(NOT WIN32)
    find_program(S042_BASH bash REQUIRED)
    foreach(name IN ITEMS
            EveryJobPassed
            OnlyDocumentationSkippedTheRest
            AFailedOrCancelledJobFails
            AWrongVersionDocFails
            ASkippedJobInAFullRunFails
            ARunJobInADocumentationRunFails)
        add_test(NAME S042_CiResult${name}
            COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows
                    -DBASH=${S042_BASH} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s042_result_${name}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/s042_ci_result_cases.cmake)
    endforeach()
endif()
