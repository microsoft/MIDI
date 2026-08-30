// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiVirtualDeviceTests
    : public WEX::TestClass<MidiVirtualDeviceTests>
{
public:

    BEGIN_TEST_CLASS(MidiVirtualDeviceTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    //TEST_CLASS_SETUP(ClassSetup);
    //TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestCreateVirtualDevice);

    // endpoint name limits. The specification states these in UTF-8 bytes, not characters
    TEST_METHOD(TestCompliantNameIsNotModified);
    TEST_METHOD(TestOverlongUnicodeNameIsTruncatedOnCharacterBoundary);
    TEST_METHOD(TestOverlongAsciiNameIsTruncated);
    TEST_METHOD(TestDeviceSideNameKeepsSuffixWhenTruncated);

    // in-protocol stream message text, which is also limited in UTF-8 bytes
    TEST_METHOD(TestEndpointNameNotificationRespectsByteLimit);
    TEST_METHOD(TestFunctionBlockNameNotificationRespectsByteLimit);
    TEST_METHOD(TestSplitTextMessagesNeverSplitAMultiByteCharacter);

    // connection lifetime
    TEST_METHOD(TestClientDisconnectsBeforeDevice);
    TEST_METHOD(TestClientDisconnectsAfterDevice);
    TEST_METHOD(TestMultipleClientsConnectAndDisconnect);

    // endpoint publication and teardown
    TEST_METHOD(TestEndpointsPublishedBeforeAnyClientConnects);
    TEST_METHOD(TestBothEndpointsRemovedWhenDeviceDisconnects);
    TEST_METHOD(TestRepeatedCreateAndDestroyDoesNotLeakEndpoints);
    TEST_METHOD(TestEndpointStaysFunctionalAcrossClientReconnects);

    // misuse of the device-side endpoint
    TEST_METHOD(TestExtraConnectionToDeviceSideEndpointDoesNotBreakClient);

    // teardown robustness
    TEST_METHOD(TestDeviceTeardownWhileClientSendingDoesNotHang);
    TEST_METHOD(TestDeviceTeardownWithoutClientDisconnectDoesNotHang);

    // client endpoint in-use notification
    TEST_METHOD(TestClientEndpointInUseIsFalseBeforeAnyClientConnects);
    TEST_METHOD(TestClientEndpointInUseRaisedOnConnectAndDisconnect);
    TEST_METHOD(TestClientEndpointInUseSurvivesDeviceTeardownFirst);

    // general virtual device behavior
    TEST_METHOD(TestEmptyNameGetsDefaultName);
    TEST_METHOD(TestMultipleVirtualDevicesHaveDistinctIdentities);
    TEST_METHOD(TestFunctionBlocksAreVisibleOnVirtualDevice);


private:


};
