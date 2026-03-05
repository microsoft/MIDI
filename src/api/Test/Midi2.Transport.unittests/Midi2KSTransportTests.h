// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <WexTestClass.h>

class MidiKSTransportTests
    : public WEX::TestClass<MidiKSTransportTests>,
    public MidiTransportTestsBase
{
public:

    BEGIN_TEST_CLASS(MidiKSTransportTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Midi2.KSTransport.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Minmidi.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"usbmidi2.sys")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"midisrv.exe")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestMidiKSTransport_UMP);
    TEST_METHOD(TestMidiKSTransport_Any);
    TEST_METHOD(TestMidiKSTransportCreationOrder_UMP);
    TEST_METHOD(TestMidiKSTransportCreationOrder_Any);
    TEST_METHOD(TestMidiKSTransportBidi_UMP);
    TEST_METHOD(TestMidiKSTransportBidi_Any);
    TEST_METHOD(TestMidiKSIO_Latency_UMP);
    TEST_METHOD(TestMidiKSIOSlowMessages_Latency_UMP);
    TEST_METHOD(TestMidiKSIO_Latency_UMP_WaitForSendComplete);

    TEST_METHOD(TestKsHandleWrapperQueryRemove_FilterHandle);
    TEST_METHOD(TestKsHandleWrapperSurpriseRemove_FilterHandle);
    TEST_METHOD(TestKsHandleWrapperQueryRemove_PinHandle);
    TEST_METHOD(TestKsHandleWrapperSurpriseRemove_PinHandle);
};

