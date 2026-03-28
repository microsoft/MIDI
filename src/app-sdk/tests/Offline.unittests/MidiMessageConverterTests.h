// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiMessageConverterTests
    : public WEX::TestClass<MidiMessageConverterTests>
{
public:

    BEGIN_TEST_CLASS(MidiMessageConverterTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.dll")
        END_TEST_CLASS()


        TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    //Generic Tests
    TEST_METHOD(TestConvertControlChangeMessages);
    TEST_METHOD(TestConvertNoteOnMessages);
    TEST_METHOD(TestConvertNoteOffMessages);
    TEST_METHOD(TestConvertClockMessages);


private:
    init::MidiDesktopAppSdkInitializer m_initializer{};


};
