// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

class MonitorFormatTests : public WEX::TestClass<MonitorFormatTests>
{
public:

    BEGIN_TEST_CLASS(MonitorFormatTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- the bytes ----

    TEST_METHOD(OneWordIsFourBytes);
    TEST_METHOD(TwoWordsAreTwoGroupsOfFour);
    TEST_METHOD(NothingIsNotACrash);

    // ---- MIDI 1.0 in a UMP ----

    TEST_METHOD(AMidi1ControlChangeReadsBack);
    TEST_METHOD(AMidi1NoteOnReadsBack);
    TEST_METHOD(AMidi1ProgramChangeHasNoValue);
    TEST_METHOD(AMidi1PitchBendIsSignedAroundCenter);

    // ---- MIDI 2.0 ----

    TEST_METHOD(AMidi2ControlChangeReadsAsAFraction);
    TEST_METHOD(AMidi2NoteOnKeepsItsSixteenBitVelocity);
    TEST_METHOD(AMidi2ControlChangeAtFullScaleIsOne);
    TEST_METHOD(ARegisteredControllerNamesBothHalves);

    // ---- group and channel ----

    TEST_METHOD(GroupAndChannelAreCountedFromOne);

    // ---- anything else ----

    TEST_METHOD(SystemExclusiveIsNamedNotDecoded);
    TEST_METHOD(SomethingUnknownStillShowsItsBytes);

    // ---- the elapsed column ----

    TEST_METHOD(ElapsedAlwaysHasThreeDecimals);
    TEST_METHOD(ElapsedCountsPastASecond);
};
