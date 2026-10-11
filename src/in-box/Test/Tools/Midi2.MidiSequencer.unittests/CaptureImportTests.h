// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Captures from MIDI Monitor and the console: reading their CSV, and turning it into tracks that
// play back the way it was played.
class CaptureImportTests : public WEX::TestClass<CaptureImportTests>
{
public:

    BEGIN_TEST_CLASS(CaptureImportTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AConsoleCaptureBecomesATrackForEachGroup);
    TEST_METHOD(AMidiMonitorCaptureReadsToo);
    TEST_METHOD(TimesFollowTheTempo);
    TEST_METHOD(OtherTextIsNotACapture);
    TEST_METHOD(CopiedWordsPasteOneAStep);
};
