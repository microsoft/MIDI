// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Which notes are drums. Transposing moves every other note, so a mistake here either moves a
// drum kit's notes, swapping one drum for another, or leaves a melodic part in the wrong key.
class PercussionTests : public WEX::TestClass<PercussionTests>
{
public:

    BEGIN_TEST_CLASS(PercussionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // ---- read from files ----
    TEST_METHOD(TreatsChannelTenAsDrums);
    TEST_METHOD(FollowsDrumKitBankSelects);
    TEST_METHOD(FollowsGsRhythmParts);
    TEST_METHOD(FollowsXgPartMode);
    TEST_METHOD(GoesBackToChannelTenOnAReset);
    TEST_METHOD(NeverTreatsChannelTenAsMelodic);

    // ---- MIDI 2.0 packets, which a standard file cannot hold ----
    TEST_METHOD(FollowsMidi2ProgramChangeBanks);
    TEST_METHOD(IgnoresShortAndForeignMessages);
};
