// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Saves customizations to a real KS or KSA endpoint the way MIDI Settings does, and restarts the
// service to check what was stored. Each test removes what it saved, and none runs against an
// endpoint which already has a customization. The tests that restart the service skip unless the
// prompt is elevated, and need every MIDI app closed:
//
//   te.exe Midi2.MidiSettings.unittests.dll /name:EndpointCustomizationHardwareTests::* /p:EndpointDeviceId=<id>
//
// Get the id from "midi enum endpoints --show-endpoint-id".
class EndpointCustomizationHardwareTests : public WEX::TestClass<EndpointCustomizationHardwareTests>
{
public:

    BEGIN_TEST_CLASS(EndpointCustomizationHardwareTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Hardware")
    END_TEST_CLASS()

    // issue #1269
    TEST_METHOD(TestPortNamesFromTwoSavesSurviveARestart);

    TEST_METHOD(TestEverySaveKeepsWhatItDoesNotShow);
    TEST_METHOD(TestClearedPortNameStaysCleared);
    TEST_METHOD(TestClearedPortNameLeavesTheEndpointAtOnce);
    TEST_METHOD(TestRemovalClearsPortNamesAtOnce);
};
