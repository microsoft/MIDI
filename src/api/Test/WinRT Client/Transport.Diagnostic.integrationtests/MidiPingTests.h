// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class MidiPingTests
    : public WEX::TestClass<MidiPingTests>
{
public:

    BEGIN_TEST_CLASS(MidiPingTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Devices.Midi2.Endpoints.Loopback.dll")
        END_TEST_CLASS()

    TEST_METHOD(TestPing);

private:


};