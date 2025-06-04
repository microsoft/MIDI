// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiEndpointIdHelperTests
    : public WEX::TestClass<MidiEndpointIdHelperTests>
{
public:

    BEGIN_TEST_CLASS(MidiEndpointIdHelperTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    //TEST_METHOD_SETUP(TestSetup);
    //TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestCreateShortIdFromFullId);
    TEST_METHOD(TestCreateFullIdFromShortId);
    TEST_METHOD(TestIsWindowsMidiServicesId);
    TEST_METHOD(TestNormalizeId);


private:
    init::MidiDesktopAppSdkInitializer m_initializer{};

};