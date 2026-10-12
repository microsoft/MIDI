// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Joysticks, wheels and gamepads read as HID devices, and the steps that read them.
class HidControllerTests : public WEX::TestClass<HidControllerTests>
{
public:

    BEGIN_TEST_CLASS(HidControllerTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(EveryHidControlHasOneKey);
    TEST_METHOD(ValuesSpanTheLogicalRange);
    TEST_METHOD(TheHatPointsEightWays);
    TEST_METHOD(ButtonsHatsAndAxisEndsPress);
    TEST_METHOD(ASliderHasNoDeadZoneAndNoRest);
    TEST_METHOD(LearnFindsAMovedHidControl);
    TEST_METHOD(LearnStartsFromWhereControlsRest);
    TEST_METHOD(DevicesMatchByPathThenModelThenName);
    TEST_METHOD(XInputCollectionsAreLeftToXInput);
    TEST_METHOD(DeviceStepsSurviveTheFile);
    TEST_METHOD(ChoosingAnotherDeviceRestartsTheStep);
    TEST_METHOD(TheGuidesJoystickExamplesRead);
};
