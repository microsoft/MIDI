// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class RemoteControlTests : public WEX::TestClass<RemoteControlTests>
{
public:

    BEGIN_TEST_CLASS(RemoteControlTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the file ----

    TEST_METHOD(RulesRoundTripThroughTheFile);
    TEST_METHOD(ALayoutWithNoRulesWritesNoObject);
    TEST_METHOD(UnknownFieldsSurviveASave);
    TEST_METHOD(AnUnknownActionMarksTheFileNewer);
    TEST_METHOD(HostileValuesAreBounded);
    TEST_METHOD(StartStatesAndOpenSequenceRoundTrip);

    // ---- matching ----

    TEST_METHOD(AProgramChangeMatchesItsRule);
    TEST_METHOD(DeviceGroupChannelAndNumberAllHaveToAgree);
    TEST_METHOD(AnyChannelAndAnyNumberMatchEverything);
    TEST_METHOD(AFootSwitchReleaseDoesNotFireTwice);
    TEST_METHOD(ARuleThatTakesTheValueFiresOnZero);
    TEST_METHOD(ANoteOffFiresNothing);
    TEST_METHOD(AMidi2MessageMatchesToo);
    TEST_METHOD(ARuleThatIsOffOrOnAMissingDeviceNeverMatches);

    // ---- what a rule does ----

    TEST_METHOD(PagesGoByIdNextPreviousAndNumber);
    TEST_METHOD(NextAndPreviousStopAtTheEnds);
    TEST_METHOD(AGroupTargetReachesEveryMember);
    TEST_METHOD(ShowOnlyThisHidesTheRestOfItsGroup);
    TEST_METHOD(ValuesRootsAndScalesComeFromTheMessage);
    TEST_METHOD(ARuleNamingWhatIsNotThereDoesNothing);

    // ---- control state ----

    TEST_METHOD(ControlsStartAsTheLayoutSays);
    TEST_METHOD(EnableDisableAndToggleChangeState);
    TEST_METHOD(AnEditKeepsWhatARuleDid);

    // ---- loops and the controller's lights ----

    TEST_METHOD(TheLayoutsOwnOutputIsNotAPress);
    TEST_METHOD(TheRateLimitStopsARunaway);
    TEST_METHOD(ThePageIsToldAsAProgramOrAControlChange);

    // ---- the editor ----

    TEST_METHOD(EditorEditsAreOneUndoStep);
    TEST_METHOD(RenamingADeviceCarriesTheRules);
    TEST_METHOD(TheListensShortcutAddsARule);

    // ---- the example the docs hand out ----

    TEST_METHOD(TheDocsExampleLoadsAndRuns);
};
