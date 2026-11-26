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
        TheBuildTakesTheSharedDesktopEntry
        TheTestsRunWithoutRoot
        ThePackageIsBuiltWithDebiansHardeningFlags)
    add_test(NAME S036_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${s036_script}
                -DRECIPE_DIR=${PROJECT_SOURCE_DIR}/packaging/debian -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s036/${name} -P ${s036_cases})
endforeach()

# S037: the decision logic of the Windows installer (scripts/packaging) and the recipe in
# packaging/windows.
set(s037_script "${PROJECT_SOURCE_DIR}/scripts/packaging/WindowsPackage.cmake")
set(s037_cases "${CMAKE_CURRENT_SOURCE_DIR}/s037_windows_cases.cmake")
foreach(name IN ITEMS
        TheVersionsToReleaseGetAnMsi
        ADryRunBuildsTheHighestVersion
        NothingToBuildBuildsNothing
        SkippedVersionsAreLeftOutAndNamed
        AllVersionsSkippedBuildNothing
        AReleaseWithoutTheMsiIsReported
        VersionsBeyondWindowsInstallerFail
        TheToolVersionsComeFromTheRecipe
        BadRecipesFail
        CarriageReturnsAreIgnored
        TheFileListMatches
        AMissingOrExtraFileFails
        TheLicenceBecomesRtf
        TheRealLicenceBecomesRtf
        TheNoticeNamesTheQtVersion
        TheRecipeBuildsWithTheCiQt
        WindowsBuildsWithQtVersion
        TheInstallerShipsNoImageFormatPlugins
        TheInstallerKeepsItsUpgradeCode
        TheInstallerOffersBothScopes
        TheInstallerAsksForTheLicence
        TheInstallerShowsTheLogo
        TheDebianReadmeIgnoresMissingFiles)
    add_test(NAME S037_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${s037_script}
                -DRECIPE_DIR=${PROJECT_SOURCE_DIR}/packaging/windows -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s037/${name} -P ${s037_cases})
endforeach()

# S049: the decision logic of the macOS disk image (scripts/packaging) and the recipe in
# packaging/macos.
set(s049_script "${PROJECT_SOURCE_DIR}/scripts/packaging/MacosPackage.cmake")
set(s049_cases "${CMAKE_CURRENT_SOURCE_DIR}/s049_macos_cases.cmake")
foreach(name IN ITEMS
        TheVersionsToReleaseGetADiskImage
        ADryRunBuildsTheHighestVersion
        NothingToBuildBuildsNothing
        SkippedVersionsAreLeftOutAndNamed
        AReleaseWithoutTheDiskImageIsReported
        TheToolVersionsComeFromTheRecipe
        BadRecipesFail
        CarriageReturnsAreIgnored
        TheFileListMatches
        AMissingOrExtraFileFails
        AFileForANewerMacosFails
        ALibraryOutsideTheBundleFails
        TheNoticeNamesTheQtVersion
        TheRecipeBuildsWithTheCiQt
        TheRecipeRunsOnTheOldestMacosOfItsQt
        TheDiskImageIsForAppleSiliconAlone
        TheBundleIsSignedAdHoc
        TheDiskImageShipsNoImageFormatPlugins
        TheBundleCarriesTheLicences
        TheBundleIdentityStays
        TheScriptsRunWithTheBashOfMacos
        TheReadmeChecksTheDownload)
    add_test(NAME S049_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${s049_script}
                -DRECIPE_DIR=${PROJECT_SOURCE_DIR}/packaging/macos -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/s049/${name} -P ${s049_cases})
endforeach()
