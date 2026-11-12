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
        TheRepositoryKeepsNoSrcinfo)
    add_test(NAME S034_${name}
        COMMAND ${CMAKE_COMMAND} -DCASE=${name} -DSCRIPT=${s034_script} -DRECIPE_DIR=${s034_recipe}
                -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cases/${name} -P ${s034_cases})
endforeach()
