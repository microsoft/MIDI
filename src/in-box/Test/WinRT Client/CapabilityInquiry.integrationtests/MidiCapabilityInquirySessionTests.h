// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


class MidiCapabilityInquirySessionTests
    : public WEX::TestClass<MidiCapabilityInquirySessionTests>
{
public:

    BEGIN_TEST_CLASS(MidiCapabilityInquirySessionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Integration")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_METHOD(TestDiscoveryFindsAResponder);
    TEST_METHOD(TestDiscoveryFindsNothingWhenNobodyAnswers);
    TEST_METHOD(TestGetPropertyDataReturnsTheResource);
    TEST_METHOD(TestPropertyExchangeCapabilitiesComeFirst);
    TEST_METHOD(TestLargeResourceIsReassembledFromChunks);
    TEST_METHOD(TestSilenceIsReportedAsNoResponse);
    TEST_METHOD(TestNegativeAcknowledgmentIsReported);
    TEST_METHOD(TestDecliningStatusIsReported);
    TEST_METHOD(TestNamedResourcesAreParsed);
    TEST_METHOD(TestProgramListPagesUntilItIsComplete);
    TEST_METHOD(TestProfileInquiryReturnsBothLists);
    TEST_METHOD(TestUnsolicitedProfileReportRaisesAnEvent);

    TEST_METHOD(TestSubscriptionDeliversUpdates);
    TEST_METHOD(TestSubscriptionRefusalIsReported);
    TEST_METHOD(TestUnsubscribeStopsUpdates);
    TEST_METHOD(TestResponderCanEndASubscription);

    TEST_METHOD(TestVersion11ResponderIsFoundAndUsable);
    TEST_METHOD(TestInvalidateMuidForgetsTheResponder);

    TEST_METHOD(TestVirtualDeviceAnswersDiscoveryAndResources);
    TEST_METHOD(TestVirtualDeviceSaysNothingUntilEnabled);
    TEST_METHOD(TestVirtualDeviceAnswersProfilesAndUnknownResources);

private:

};
