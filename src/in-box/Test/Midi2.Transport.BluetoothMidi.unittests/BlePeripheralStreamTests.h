// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Unit tests for the answers this PC gives a remote Central, as a BLE MIDI 2.0 Peripheral, to UMP
// Endpoint Discovery and Stream Configuration Requests.
class BlePeripheralStreamTests
    : public WEX::TestClass<BlePeripheralStreamTests>
{
public:

    BEGIN_TEST_CLASS(BlePeripheralStreamTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestEndpointDiscoveryIsAnsweredAndNotForwarded);
    TEST_METHOD(TestEndpointDiscoveryAnswersOnlyWhatWasAskedFor);
    TEST_METHOD(TestEndpointNameIsSplitAcrossMessages);
    TEST_METHOD(TestEndpointNameIsNotCutInsideACharacter);
    TEST_METHOD(TestStreamConfigurationRequestIsAgreed);
    TEST_METHOD(TestStreamConfigurationRefusesWhatCannotBeDone);
    TEST_METHOD(TestFunctionBlockDiscoveryIsConsumedWithoutAnswer);
    TEST_METHOD(TestResetForgetsTheConfiguration);
};
