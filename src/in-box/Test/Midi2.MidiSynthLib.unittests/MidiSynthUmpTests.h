// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The UMP dispatcher and the engine behavior it drives.
//
// These render real audio from the sound set Windows installed, because several of the checks can
// only be made by measuring the output: two velocities that collapse to the same seven bit value,
// a master volume curve, a tuning offset. A machine without a usable gm.dls skips them rather than
// failing, since there would be nothing to render with.
class MidiSynthUmpTests : public WEX::TestClass<MidiSynthUmpTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthUmpTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Unit", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TestPacketWordCounts);
    TEST_METHOD(TestNoteOnVelocityZeroSemantics);
    TEST_METHOD(TestGroupAndPacketFraming);
    TEST_METHOD(TestHighResolutionIsPreserved);
    TEST_METHOD(TestBankAddressing);
    TEST_METHOD(TestDrumChannelAssignment);
    TEST_METHOD(TestUserVolume);
    TEST_METHOD(TestResets);
    TEST_METHOD(TestResetAllControllersScope);
    TEST_METHOD(TestPropertyRequestParking);
    TEST_METHOD(TestPropertyExchangeProgramListLinks);
    TEST_METHOD(TestPropertyExchangeProgramListCategories);
    TEST_METHOD(TestPropertyExchangeProgramListPagination);
    TEST_METHOD(TestChannelListSubscriptionNotification);
    TEST_METHOD(TestChannelStateIsValidBeforeInitialize);
    TEST_METHOD(TestInitializeKeepsChannelState);
    TEST_METHOD(TestIdentityReply);
    TEST_METHOD(TestMidiCiDiscovery);
    TEST_METHOD(TestMasterVolume);
    TEST_METHOD(TestMasterTuning);
    TEST_METHOD(TestActiveSensing);
    TEST_METHOD(TestAllSoundOff);

    TEST_METHOD(TestNoteOnPitchAttribute);
    TEST_METHOD(TestPerNotePitchBend);
    TEST_METHOD(TestPerNoteControllers);
    TEST_METHOD(TestPerNoteManagement);
    TEST_METHOD(TestUmpStreamDiscovery);
};
