# SPDX-License-Identifier: GPL-3.0-or-later
# S034: the decision logic of the AUR workflow (scripts/packaging) and the recipe in
# packaging/archlinux, through cmake -P on fixed inputs.
set(s034_script "${PROJECT_SOURCE_DIR}/scripts/packaging/AurPackage.cmake")
set(s034_recipe "${PROJECT_SOURCE_DIR}/packaging/archlinux")
set(s034_cases "${CMAKE_CURRENT_SOURCE_DIR}/s034_aur_cases.cmake")

foreach(name IN ITEMS
        AHigherTagIsPublished
        TheSameVersionPublishesNothing
        TheAurIsNeverDowngraded
        SeveralTagsLeadToTheHighest
        OnlyExactVersionTagsCount
        NoVersionTagFails
        NoSrcinfoFails
        SrcinfoWithoutPkgverFails
        AurPkgverOfAnotherFormFails
        TheAurChecksumIsReported
        CarriageReturnsAreIgnored
        ThePkgbuildGetsVersionReleaseAndChecksum
        ASingleLineChecksumArrayIsFilledIn
        ThePkgbuildModeRejectsBadInput
        TheRecipeBuildsForX86Only
        TheDesktopEntryPassesOneFile
        TheDesktopEntryChecksumMatches
        TheRepositoryKeepsNoSrcinfo)
    add_test(NAME S034_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${s034_script} -DRECIPE_DIR=${s034_recipe}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/${name} -P ${s034_cases})
endforeach()

# S035 (decision 5): in a dry run an unreachable AUR gives a warning, not a red run.
add_test(NAME S035_AnUnreachableAurStillGivesTheVersion
    COMMAND ${CMAKE_COMMAND} -DCASE=AnUnreachableAurStillGivesTheVersion -DSCRIPT=${s034_script}
            -DRECIPE_DIR=${s034_recipe}
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/AnUnreachableAurStillGivesTheVersion
            -P ${s034_cases})

# S036: the decision logic of the Debian package and the GitHub releases (scripts/packaging) and
# the recipe in packaging/debian.
set(s036_script "${PROJECT_SOURCE_DIR}/scripts/packaging/DebianPackage.cmake")
set(s036_cases "${CMAKE_CURRENT_SOURCE_DIR}/s036_debian_cases.cmake")
foreach(name IN ITEMS
        TheFirstRunReleasesTheFirstVersion
        APublishedVersionIsNotReleasedAgain
        SeveralMissingVersionsComeOldestFirst
        ACaughtUpVersionIsNeverLatest
        OnlyExactVersionTagsCount
        SkippedVersionsAreLeftOutAndNamed
        ASkippedHighestVersionIsNotLatest
        AReleaseWithoutThePackageIsReported
        ADryRunBuildsTheHighestVersion
        NoVersionFromTheFirstOnIsGreen
        BadSettingsFail
        CarriageReturnsAreIgnored
        TheRecipeStartsAt051
        TheChangelogHasAnEntryPerVersion
        MergesShowNumberAndTitle
        LongLinesAreWrapped
        TheNotesListOnlyTheVersion
        TheChangelogNeedsTheVersionTag
        TheControlFileGetsVersionDependsAndSize
        TheQtPluginsAreDependedOn
        TheControlModeRejectsBadInput
        TheMaintainerIsTheOneOfThePkgbuild
        TheCopyrightRefersToCommonLicenses
        CPackMakesNoDeb
        TheRecipeKeepsLfLineEndings
        TheBuildTakesTheSharedDesktopEntry)
    add_test(NAME S036_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${s036_script}
                -DRECIPE_DIR=${PROJECT_SOURCE_DIR}/packaging/debian -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s036/${name} -P ${s036_cases})
endforeach()
