// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class RuntimeSurfaceTests : public WEX::TestClass<RuntimeSurfaceTests>
{
public:

    BEGIN_TEST_CLASS(RuntimeSurfaceTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- where the page sits in the window ----

    TEST_METHOD(ActualSizeNeverScales);
    TEST_METHOD(ActualSizeCentersWhatFits);
    TEST_METHOD(ActualSizeScrollsWhatDoesNot);
    TEST_METHOD(FitScalesBothAxesTheSame);
    TEST_METHOD(FitLetterboxesRatherThanStretching);
    TEST_METHOD(FitCanScaleUpOnABigDisplay);
    TEST_METHOD(CustomPercentIsClamped);
    TEST_METHOD(APointInTheLetterboxIsNotOnThePage);
    TEST_METHOD(APointRoundTripsThroughTheScale);
    TEST_METHOD(AWindowWithNoSizeIsHarmless);

    // ---- panic ----

    TEST_METHOD(PanicSendsFourMessagesPerChannel);
    TEST_METHOD(PanicReleasesTheSustainPedalFirst);
    TEST_METHOD(PanicCentersPitchBend);
    TEST_METHOD(PanicCoversEveryChannelOfAGroup);
    TEST_METHOD(PanicRefusesABufferThatIsTooSmall);

    // ---- which groups a layout drives ----

    TEST_METHOD(GroupMasksFollowTheDeviceTable);
    TEST_METHOD(GroupMasksIgnoreADeviceThatIsNotInTheTable);
    TEST_METHOD(AMessageOnEveryGroupSetsEveryBit);
    TEST_METHOD(GroupMasksIncludeSequenceSteps);

    // ---- which way a finger moves a control ----

    TEST_METHOD(AVerticalFaderReadsBottomToTop);
    TEST_METHOD(AHorizontalFaderReadsLeftToRight);
    TEST_METHOD(AKnobIsNudgedRatherThanSet);
    TEST_METHOD(ADisplayOnlyControlTakesNoInput);

    // ---- colors ----

    TEST_METHOD(ATonalThemeTintsThePlateWithTheControlHue);
    TEST_METHOD(AGlassThemeLeavesTheDeckShowingThrough);
    TEST_METHOD(ANamedPlateColorWins);
    TEST_METHOD(ALiteralColorThatDoesNotParseFallsBackToTheSlot);
    TEST_METHOD(TheLampRingFallsBackToASolidArcWhenSmall);
    TEST_METHOD(LabelInkIsChosenByMeasuringTheBackground);

    // ---- detents, as the surface sees them ----

    TEST_METHOD(ASmoothControlHasNoStops);
    TEST_METHOD(EveryListedStopGetsAnEqualShareOfTheTravel);
    TEST_METHOD(SnappingPicksTheNearestStop);

    // ---- the starter layout ----

    TEST_METHOD(TheStarterLayoutIsValid);
    TEST_METHOD(TheStarterLayoutFitsOnItsPage);
    TEST_METHOD(TheStarterLayoutPointsAtOneDevice);
    TEST_METHOD(TheStarterLayoutHasAKeyboardOrder);

    // ---- every template the New layout picker offers ----

    TEST_METHOD(EveryTemplateIsValid);
    TEST_METHOD(EveryTemplateFitsOnItsPage);
    TEST_METHOD(EveryTemplateDrivesTheOneDevice);
    TEST_METHOD(TheBlankTemplateHasAPageAndADeviceAndNothingElse);
};
