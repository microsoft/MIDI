// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiEndpointDeviceInformationTests
    : public WEX::TestClass<MidiEndpointDeviceInformationTests>
{
public:

    BEGIN_TEST_CLASS(MidiEndpointDeviceInformationTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.Enumeration.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestCreateFromId);
    TEST_METHOD(TestWalkUpToParent);

    TEST_METHOD(TestFindAll);


private:


};

