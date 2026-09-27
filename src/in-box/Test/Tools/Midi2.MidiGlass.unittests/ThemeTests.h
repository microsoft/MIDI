// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ThemeTests : public WEX::TestClass<ThemeTests>
{
public:

    BEGIN_TEST_CLASS(ThemeTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ShipsTheThemesTheDesignNames);
    TEST_METHOD(EveryBuiltInThemeFillsAllSixSlots);
    TEST_METHOD(ContrastMatchesTheWcagReferenceValues);
    TEST_METHOD(MeasuresEverySlotAgainstTheDeck);
    TEST_METHOD(EverySlotOfEveryShippedThemeIsLegible);
    TEST_METHOD(NoThemeLeavesTheTrackColorAgainstItsOwnDeck);
    TEST_METHOD(BigwigIsALadderOfGraysWithColorOnlyForTheValue);
    TEST_METHOD(TheTonalThemesTurnOffTheGlass);
    TEST_METHOD(HighContrastTurnsOffEveryEffect);
    TEST_METHOD(BoneIsSeparatedByItsShadowRatherThanItsValue);
    TEST_METHOD(BoneNeverLetsTheSpaceGoDarkerThanBone);
    TEST_METHOD(OnlyALightThemeRaisesItsRestingRim);
    TEST_METHOD(OnlyBoneMovesTheShadowOffItsShippedGeometry);

    // ---- the tube themes, and what they added to the engine ----

    TEST_METHOD(OnlyATubeThemeLaysAnOverlayOverTheDeck);
    TEST_METHOD(EveryTubeThemeCarriesItsControlsOnLightRatherThanValue);
    TEST_METHOD(ATubeThemeLightsUpInAColorThatIsNeitherTheHueNorWhite);
    TEST_METHOD(EveryThemeThatDerivesAColorStillResolvesToSomething);
    TEST_METHOD(EveryMeterZoneNamesASlotThatExists);
    TEST_METHOD(CathodeCannotColorCodeAndSaysSo);
    TEST_METHOD(TheAmberRampIsARisingBrightnessAsWellAsARisingHue);
    TEST_METHOD(TerminalGreenGlassIsBlueSlateRatherThanGreen);
    TEST_METHOD(NothingShippedReachesAPureBlackOrAPureWhite);

    // ---- the two hardware panels, and what they added to the engine ----

    TEST_METHOD(OnlyAPanelThemeCarriesAGrainOrANeutral);
    TEST_METHOD(JoveFillsItsSwitchesWithoutFillingItsKnobs);
    TEST_METHOD(SupersawSaysOnWithItsLampRatherThanItsPlate);
    TEST_METHOD(TheNeutralSlotIsTheAbsenceOfAColorRatherThanASeventhHue);
    TEST_METHOD(APointerAndACapLineCanStopFollowingTheHue);
    TEST_METHOD(EveryPanelThemePutsItsLabelsAboveItsControls);
    TEST_METHOD(NoShippedThemeGoesDarkerWhenItIsLit);
    TEST_METHOD(APointerIsVisibleOnEveryThemesOwnPlate);

    // ---- the engine round that brought the themes up to their comps ----

    TEST_METHOD(TheTouchRimIsTheRimComingUpNotBlack);
    TEST_METHOD(ASwitchNameReadsOnItsPlateLitOrNot);
    TEST_METHOD(AFullFillIsTheHueTopToBottom);
    TEST_METHOD(AnLfoIsNeverFilledLikeASwitch);
    TEST_METHOD(AFaderFillFollowsTheThemesStrength);
    TEST_METHOD(TheValueColorDrawsEveryValueInOneColor);
    TEST_METHOD(EveryKeyboardHasALightAndADarkKey);
    TEST_METHOD(ALampThemeLightsItsOwnLampColor);
    TEST_METHOD(ADefaultThemeAsksForNoneOfTheNewLooks);
    TEST_METHOD(TheDeckColorIsReadDownThePage);

    // ---- Five-iSH and Airy System, and what they added to the engine ----

    TEST_METHOD(FiveIshPrintsTwoInksOnTwoSurfaces);
    TEST_METHOD(FiveIshShowsColorOnlyInItsLamps);
    TEST_METHOD(ANeutralCapIsTheNeutralOnlyWhereTheThemeAsks);
    TEST_METHOD(AirySwitchesRestDarkWhileKnobsAndFadersStayLit);
    TEST_METHOD(AiryPadsAreColoredPlasticThatReadsLitOrNot);
    TEST_METHOD(AnAirySliderLightsTheFrameAroundItsSlot);
    TEST_METHOD(AKnobRingCanBeItsOwnColor);
    TEST_METHOD(ARimOfZeroIsNoRim);
    TEST_METHOD(ASectionIsRaisedLikeAControlUnlessTheThemeSaysOtherwise);
    TEST_METHOD(ARuleIsTheInkTurnedDownUnlessTheThemeNamesOne);
};


