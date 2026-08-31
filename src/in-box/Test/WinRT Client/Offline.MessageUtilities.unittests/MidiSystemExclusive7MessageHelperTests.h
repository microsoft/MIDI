// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiSystemExclusive7MessageHelperTests
    : public WEX::TestClass<MidiSystemExclusive7MessageHelperTests>
{
public:

    BEGIN_TEST_CLASS(MidiSystemExclusive7MessageHelperTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()


    TEST_METHOD(TestMessageIsSystemExclusiveMessage);
    TEST_METHOD(TestGetDataByteCountFromFirstWord);

    TEST_METHOD(TestGetDataBytesFromSingleMessageWords);
    TEST_METHOD(TestGetDataBytesFromSingleMessage64);
    TEST_METHOD(TestGetDataBytesFromSingleMessageWithZeroDataBytes);
    TEST_METHOD(TestGetDataBytesDoesNotMaskHighBit);

    TEST_METHOD(TestAppendDataBytesFromSingleMessage);
    TEST_METHOD(TestAppendDataBytesFromSingleMessage64);

    TEST_METHOD(TestGetDataBytesFromMultipleMessages);
    TEST_METHOD(TestGetDataBytesFromEmptyMessageList);

};
