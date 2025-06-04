// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiMessageBuilderTests
    : public WEX::TestClass<MidiMessageBuilderTests>
{
public:

    BEGIN_TEST_CLASS(MidiMessageBuilderTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.Messages.dll")
    END_TEST_CLASS()


    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestBuildType0UtilityMessages);
    TEST_METHOD(TestBuildType1SystemMessages);
    TEST_METHOD(TestBuildType2Midi1ChannelVoiceMessages);
    TEST_METHOD(TestBuildType4Midi2ChannelVoiceMessages);
    TEST_METHOD(TestBuildTypeFStreamMessages1);
    TEST_METHOD(TestBuildTypeFStreamMessages2);
    TEST_METHOD(TestBuildMixedDatasetHeaderMessage);
    TEST_METHOD(TestBuildMixedDatasetPayloadMessage);


private:
    init::MidiDesktopAppSdkInitializer m_initializer{};


};

