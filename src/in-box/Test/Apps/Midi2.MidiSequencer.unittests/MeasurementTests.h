// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Phase 0 measurements. They log numbers for the design document, and fail only when something is
// far slower than any reasonable target, so a slow test machine doesn't make them flaky.
class MeasurementTests : public WEX::TestClass<MeasurementTests>
{
public:

    BEGIN_TEST_CLASS(MeasurementTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(JsonSizeAndLoadTime);
    TEST_METHOD(RenderingAWindowIsCheap);
};
