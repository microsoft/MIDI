// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// What the customize and port names dialogs in MIDI Settings send when they save. A save resets
// whatever it leaves out, and a saved port list replaces the stored one, so each dialog has to send
// the endpoint's whole set of settings, and every named port rather than only the changed ones.
// Needs Windows.Devices.Midi2.dll beside the test, but no service.
class EndpointSettingsTests : public WEX::TestClass<EndpointSettingsTests>
{
public:

    BEGIN_TEST_CLASS(EndpointSettingsTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // which port names a save sends (issue #1269)
    TEST_METHOD(TestEveryNamedPortIsSentNotJustTheChangedOnes);
    TEST_METHOD(TestEmptiedPortIsSentAsCleared);
    TEST_METHOD(TestUntouchedEmptyPortIsLeftOut);
    TEST_METHOD(TestSpacesOnlyCountAsEmpty);

    // the service keeps showing a cleared name until it restarts
    TEST_METHOD(TestClearedNameIsLeftOutWhileTheEndpointStillShowsIt);
    TEST_METHOD(TestClearedNameIsForgottenOnceTheEndpointStopsShowingIt);
    TEST_METHOD(TestNamingAClearedPortAgainForgetsIt);
    TEST_METHOD(TestOtherEndpointsAndDirectionsAreLeftAlone);

    // the cleared names are kept between runs of the app
    TEST_METHOD(TestClearedNamesSurviveARoundTrip);
    TEST_METHOD(TestMalformedStoredTextIsIgnored);
    TEST_METHOD(TestClearedNamesAreCapped);

    // everything else the endpoint has goes back with every save
    TEST_METHOD(TestCopyIntoSendsEverySetting);
    TEST_METHOD(TestCopyIntoLeavesEmptyDisplayValuesOut);
    TEST_METHOD(TestCopyIntoDoesNotChooseWhetherToUseTheLatency);
    TEST_METHOD(TestClearedPortIsSentEmpty);
    TEST_METHOD(TestEverythingButTheDisplayValuesCountsAsContent);
};
