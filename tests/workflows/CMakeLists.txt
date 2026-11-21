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
