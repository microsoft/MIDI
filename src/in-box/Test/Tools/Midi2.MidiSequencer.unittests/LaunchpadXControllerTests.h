// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The Launchpad X controller the app runs: which port it picks, what it sends the device, and
// how it hands pads and buttons to the app, with a recorder in place of the device.
class LaunchpadXControllerTests : public WEX::TestClass<LaunchpadXControllerTests>
{
public:
    BEGIN_TEST_CLASS(LaunchpadXControllerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(FindsTheMidiPortOfOneEndpointWithBothPorts);
    TEST_METHOD(FindsTheMidiPortWhenEachPortIsItsOwnEndpoint);
    TEST_METHOD(FindsTheSecondPortOfAnUnnamedLaunchpadByUsbIds);
    TEST_METHOD(NeverChoosesTheDawPortOrAnotherDevice);
    TEST_METHOD(PrefersThePickedLaunchpadWhenThereAreTwo);

    TEST_METHOD(StartEntersProgrammerModeThenPaintsEveryLed);
    TEST_METHOD(IgnoresInputBeforeStartAndOnOtherGroups);
    TEST_METHOD(ClipPadsGoToTheAppAndNotesPlayAtOnce);
    TEST_METHOD(SwitchingModeWhileHoldingPadsLetsGoOfEveryNote);
    TEST_METHOD(StopLetsGoOfNotesAndLeavesProgrammerModeOnce);
    TEST_METHOD(AFailedLedUpdateRepaintsEverythingNextTime);
    TEST_METHOD(BuildNoteWordsMakesMidi2NotesAndPressure);
};
