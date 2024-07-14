// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION (uint8_t)0x11
#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION (uint8_t)0x12


class MidiFunctionBlockMessageBuilderTests
    : public WEX::TestClass<MidiFunctionBlockMessageBuilderTests>
{
public:

    BEGIN_TEST_CLASS(MidiFunctionBlockMessageBuilderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.Messages.dll")
    END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        //Generic Tests
    TEST_METHOD(TestBuildFunctionBlockNameNotificationLong);
    TEST_METHOD(TestBuildFunctionBlockNameNotificationMedium);
    TEST_METHOD(TestBuildFunctionBlockInfoNotification);


private:


};
