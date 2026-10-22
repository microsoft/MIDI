// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// OSC connections: packets, address patterns, time tags, and MIDI to OSC and back.
class OscTests : public WEX::TestClass<OscTests>
{
public:

    BEGIN_TEST_CLASS(OscTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(TheSpecExamplesAreByteExact);
    TEST_METHOD(BundlesNestAndCarryTheirTimeTags);
    TEST_METHOD(EveryArgumentTypeReads);
    TEST_METHOD(BrokenPacketsAreRefused);
    TEST_METHOD(MutatedPacketsNeverReadOutOfBounds);
    TEST_METHOD(PatternsMatchAsTheSpecSays);
    TEST_METHOD(TimeTagsBecomeTimestamps);
    TEST_METHOD(TimeTagsWrapIn2036);
    TEST_METHOD(SpacingIsKeptAndJitterRemoved);
    TEST_METHOD(ReaperPathsBecomeMidi);
    TEST_METHOD(ReaperAddressesWithoutAChannelUseTheRowsChannel);
    TEST_METHOD(TheMidiTypePicksTheRowByPort);
    TEST_METHOD(ArgumentFormatsBecomeMidi);
    TEST_METHOD(ValuesAreReadGenerously);
    TEST_METHOD(WildcardsExpandAndAreCapped);
    TEST_METHOD(UnknownAddressesMatchNoRow);
    TEST_METHOD(SysExIsReadEveryWay);
    TEST_METHOD(EveryPresetRoundTripsEveryMessage);
    TEST_METHOD(MessagesGoOutAsTheRowSays);
    TEST_METHOD(Midi2IsTranslatedTheSpecsWay);
    TEST_METHOD(SysExIsCollectedAcrossPackets);
    TEST_METHOD(RowSettingsLeaveMessagesOut);
    TEST_METHOD(UnitPitchBendIsCenteredExactly);
    TEST_METHOD(ConnectionsSurviveTheFile);
    TEST_METHOD(BadConnectionSettingsAreRepaired);
    TEST_METHOD(RowsAreSuggestedFromWhatArrives);
    TEST_METHOD(OscConnectionsSurviveThePatchFile);
    TEST_METHOD(OscConnectionsRouteLikeEndpoints);
    TEST_METHOD(RuleAddressesAndNumbersRead);
    TEST_METHOD(CustomRulesBecomeMidi);
    TEST_METHOD(CustomRulesSendOsc);
    TEST_METHOD(CustomRulesSurviveTheFile);
    TEST_METHOD(RulesAreSuggestedForTouchOsc);
    TEST_METHOD(AdvertisingIsSavedAndNamed);
    TEST_METHOD(PacketsAreFramedOnAStream);
    TEST_METHOD(TcpSettingsSurviveTheFile);
    TEST_METHOD(FloatsBecomeMidi2);
};
