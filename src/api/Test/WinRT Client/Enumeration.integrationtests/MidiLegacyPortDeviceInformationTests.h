// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiLegacyPortDeviceInformationTests
    : public WEX::TestClass<MidiLegacyPortDeviceInformationTests>
{
public:

    BEGIN_TEST_CLASS(MidiLegacyPortDeviceInformationTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.Enumeration.dll")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassSetup);
        TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        TEST_METHOD(TestCreateFromId);
    
        TEST_METHOD(TestFindAll);
        TEST_METHOD(TestFindAllInputs);
        TEST_METHOD(TestFindAllOutputs);
        TEST_METHOD(TestFindAllForEndpoint);
        TEST_METHOD(TestWalkUpToParent);
        TEST_METHOD(TestFindAllForName);
        //TEST_METHOD(TestFindAllForParentId);

private:


};

  
