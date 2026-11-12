// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiMessageHelperTests
    : public WEX::TestClass<MidiMessageHelperTests>
{
public:

    BEGIN_TEST_CLASS(MidiMessageHelperTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestGetNoteOctaveWithDefaultMiddleC);
    TEST_METHOD(TestGetNoteOctaveWithOtherMiddleC);
    TEST_METHOD(TestGetNoteOctaveRejectsBadInput);
};
