// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

using namespace winrt::Windows::Devices::Midi2;

class MidiEndpointConnectionBufferTests
    : public WEX::TestClass<MidiEndpointConnectionBufferTests>
{
public:

    BEGIN_TEST_CLASS(MidiEndpointConnectionBufferTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    //TEST_CLASS_SETUP(ClassSetup);
    //TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestSendBufferBoundsError);
    TEST_METHOD(TestSendBuffer);
    TEST_METHOD(TestSendAndReceiveBuffer);

    TEST_METHOD(TestSendAndReceiveMultipleMessagesBuffer);

private:


};