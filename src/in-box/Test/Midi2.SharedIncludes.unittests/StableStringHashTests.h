// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiStableStringHash.h"

#include <WexTestClass.h>

class StableStringHashTests
    : public WEX::TestClass<StableStringHashTests>
{
public:

    BEGIN_TEST_CLASS(StableStringHashTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestFrozenValuesForRealDeviceInstanceIds);
    TEST_METHOD(TestMatchesStandardLibraryHash);
    TEST_METHOD(TestEmptyString);
    TEST_METHOD(TestHashStringIsDecimal);
};
