// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class ValueThrottleTests : public WEX::TestClass<ValueThrottleTests>
{
public:

    BEGIN_TEST_CLASS(ValueThrottleTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(SendsEverythingWhenThereIsNoLimit);
    TEST_METHOD(TheFirstMoveAlwaysGoes);
    TEST_METHOD(HoldsBackMovesInsideTheInterval);
    TEST_METHOD(TheLastValueIsAlwaysSent);
    TEST_METHOD(DoesNotRepeatAValueThatAlreadyWentOut);
    TEST_METHOD(AFullDragEndsOnWhereTheFingerLeftIt);
};
