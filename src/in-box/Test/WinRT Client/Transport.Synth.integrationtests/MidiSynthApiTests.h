// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

// The Windows.Devices.Midi2.Transports.Synth projection, against the live service.
//
// These cover the API's own contract rather than the transport's: that the manager reports what
// the transport holds, that a config built from a status round-trips, and that changing one
// property leaves the others alone, which is the whole reason the status constructor exists.
class MidiSynthApiTests : public WEX::TestClass<MidiSynthApiTests>
{
public:
    BEGIN_TEST_CLASS(MidiSynthApiTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Integration", L"Integration")
    END_TEST_CLASS()

    TEST_METHOD(TestTransportAvailabilityAndId);
    TEST_METHOD(TestStatusIsReadable);
    TEST_METHOD(TestSoundSetIsReadable);
    TEST_METHOD(TestMelodicInstrumentsAreReadable);
    TEST_METHOD(TestDefaultConfigHasSaneValues);
    TEST_METHOD(TestConfigFromStatusCopiesEveryProperty);
    TEST_METHOD(TestSinglePropertyChangeLeavesTheRestAlone);
    TEST_METHOD(TestVolumeIsClampedByTheService);
    TEST_METHOD(TestConfigFromNullStatusIsUsable);
    TEST_METHOD(TestSetDrumChannelRejectsOutOfRange);
    TEST_METHOD(TestSetDrumChannelNeedsAnOpenConnection);
    TEST_METHOD(TestEndpointDeviceIdMatchesEnumeration);
};
