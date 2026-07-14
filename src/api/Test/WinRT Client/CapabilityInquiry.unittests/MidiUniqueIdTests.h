// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiUniqueIdTests
    : public WEX::TestClass<MidiUniqueIdTests>
{
public:

    BEGIN_TEST_CLASS(MidiUniqueIdTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.CapabilityInquiry.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestCreateEmptyId);
    TEST_METHOD(TestCreateRandomId);
    TEST_METHOD(TestCreateBroadcastId);
    TEST_METHOD(TestCreateFrom28BitNumber);
    TEST_METHOD(TestCreateFromBytes);

private:
    init::MidiDesktopAppSdkInitializer m_initializer{};


};

