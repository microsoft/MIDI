// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiCapabilityInquiryMessageTests
    : public WEX::TestClass<MidiCapabilityInquiryMessageTests>
{
public:

    BEGIN_TEST_CLASS(MidiCapabilityInquiryMessageTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestProfileIdNamesItsParts);
    TEST_METHOD(TestDiscoveryRoundTripsThroughPackets);
    TEST_METHOD(TestDiscoveryReplyCarriesTheIdentity);
    TEST_METHOD(TestInvalidateMuidIsBroadcast);
    TEST_METHOD(TestProfileInquiryReplyReturnsProfiles);
    TEST_METHOD(TestSetProfileOnCarriesTheChannelCount);
    TEST_METHOD(TestProfileReportsAreBroadcast);
    TEST_METHOD(TestProfileSpecificDataRoundTrips);
    TEST_METHOD(TestNakCarriesStatusAndText);
    TEST_METHOD(TestBuilderRefusesTextThatCannotTravel);
    TEST_METHOD(TestPropertyGetDataInquiryHeaderIsReadable);
    TEST_METHOD(TestPropertyHeaderEscapesTextOutsideSevenBits);
    TEST_METHOD(TestLargePropertyBodyIsChunked);
    TEST_METHOD(TestBuilderRefusesAMessageTypeItCannotBuild);

private:

};
