// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiCiProgramListTests
    : public WEX::TestClass<MidiCiProgramListTests>
{
public:

    BEGIN_TEST_CLASS(MidiCiProgramListTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestEmptyListIsStillValidJson);
    TEST_METHOD(TestSingleEntryBytes);
    TEST_METHOD(TestBankProgramAreZeroBased);
    TEST_METHOD(TestTitleIsEscaped);
    TEST_METHOD(TestOutputIsAlwaysSevenBit);
    TEST_METHOD(TestMeasureThenBuild);
    TEST_METHOD(TestShortBufferProducesNothing);
    TEST_METHOD(TestDeviceInfoBytes);
    TEST_METHOD(TestDeviceInfoAgreesWithTheOtherIdentityCarriers);
    TEST_METHOD(TestResourceListBytes);

private:

};
