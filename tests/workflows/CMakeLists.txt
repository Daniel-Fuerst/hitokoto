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
        TheReleaseFindsEveryPackage)
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
        AReleaseWithoutAnyMsiIsNotStopped)
    add_test(NAME S037_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name}
                -DWORKFLOWS_DIR=${PROJECT_SOURCE_DIR}/.github/workflows -P ${s037_cases})
endforeach()
