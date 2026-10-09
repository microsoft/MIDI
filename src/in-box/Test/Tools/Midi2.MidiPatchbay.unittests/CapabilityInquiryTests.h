// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The MIDI-CI responder, its file and the MIDI-CI filter.
class CapabilityInquiryTests : public WEX::TestClass<CapabilityInquiryTests>
{
public:

    BEGIN_TEST_CLASS(CapabilityInquiryTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(SysExPacketsGoBothWays);
    TEST_METHOD(TheFileReadsWhatItCan);
    TEST_METHOD(AResponderNeverReadsAPath);
    TEST_METHOD(DiscoveryIsAnsweredWithWhoItIs);
    TEST_METHOD(DiscoverySaysWhatTheFileAdds);
    TEST_METHOD(EndpointInquiryGetsTheProductInstanceId);
    TEST_METHOD(ProfileInquiryRepliesInOrder);
    TEST_METHOD(SetProfileReportsHowItIs);
    TEST_METHOD(ProfileDetailsComeFromTheFile);
    TEST_METHOD(PropertiesComeFromTheFile);
    TEST_METHOD(ALargePropertyComesInChunks);
    TEST_METHOD(AReportSaysWhatWasSent);
    TEST_METHOD(AnInvalidatedMuidIsReplaced);
    TEST_METHOD(MidiCiIsKeptFromTheDeviceUnlessPassed);
    TEST_METHOD(TwoPathsAreKeptApart);
    TEST_METHOD(TheFilterSortsByCategory);
    TEST_METHOD(AnswersGoBackWhereTheQuestionCameFrom);
    TEST_METHOD(APassedQuestionGoesOutBeforeItsAnswer);
    TEST_METHOD(AResponderAnswersWithNothingAfterIt);
    TEST_METHOD(TheEndpointsAResponderAnswers);
};
