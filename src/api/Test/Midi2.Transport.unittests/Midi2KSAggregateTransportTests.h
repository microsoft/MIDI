// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

class MidiKSAggregateTransportTests
    : public WEX::TestClass<MidiKSAggregateTransportTests>,
    public MidiTransportTestsBase
{
public:

    BEGIN_TEST_CLASS(MidiKSAggregateTransportTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSAggregateTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"midisrv.exe")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestMidiKSAggregateTransportBidi_UMP);
    TEST_METHOD(TestMidiKSAggregateIO_Latency_UMP);
    TEST_METHOD(TestMidiKSAggregateIOSlowMessages_Latency_UMP);
    TEST_METHOD(TestMidiKSAggregateIO_Latency_UMP_WaitForSendComplete);
};

