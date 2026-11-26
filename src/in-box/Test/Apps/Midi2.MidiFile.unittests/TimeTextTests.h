// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The times a person can type to move the player: into Go to time, or into the position slider
// through a screen reader.
class TimeTextTests : public WEX::TestClass<TimeTextTests>
{
public:

    BEGIN_TEST_CLASS(TimeTextTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ReadsMinutesAndSeconds);
    TEST_METHOD(ReadsHoursMinutesAndSeconds);
    TEST_METHOD(ReadsPlainSeconds);
    TEST_METHOD(IgnoresSpacesAround);
    TEST_METHOD(RefusesWhatIsNotATime);
};
