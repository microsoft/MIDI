// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiMessagePacketTests
    : public WEX::TestClass<MidiMessagePacketTests>
{
public:

    BEGIN_TEST_CLASS(MidiMessagePacketTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Microsoft.Windows.Devices.Midi2.dll")
        END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        TEST_METHOD(TestUmp32);
    TEST_METHOD(TestUmp64);
    TEST_METHOD(TestUmp96);
    TEST_METHOD(TestUmp128);
    TEST_METHOD(TestUmpInterfaceCasting);


private:


};
