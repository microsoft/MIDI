// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// "Sync to device" in Windows MIDI Glass: what the receiver asks for, how it puts the pack back
// together, what it remembers, and a real sender and receiver talking over this PC's loopback.
class GlassSyncTests
{
    BEGIN_TEST_CLASS(GlassSyncTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    // ---- what to ask for ----

    TEST_METHOD(TheFirstSyncNeedsEverything);
    TEST_METHOD(NeedAsksOnlyForMissingFiles);
    TEST_METHOD(TheLayoutFileIsAlwaysAskedFor);
    TEST_METHOD(NeedRefusesTheWrongKindAndTooMuch);

    // ---- putting it back together ----

    TEST_METHOD(AssemblyGivesAPackThatChecksOut);
    TEST_METHOD(AChangedLocalFileCountsAsMissing);
    TEST_METHOD(AMissingFileIsNamed);
    TEST_METHOD(ATamperedFileFailsTheCheck);
    TEST_METHOD(ASignedPackSurvivesADeltaSync);
    TEST_METHOD(ABadSignatureNameIsRefused);

    // ---- what the receiver remembers ----

    TEST_METHOD(TheIndexFindsFilesByHash);
    TEST_METHOD(ApprovalsArePerLayoutAndSender);
    TEST_METHOD(ALocalEditIsNoticed);

    // ---- over the network ----

    TEST_METHOD(ASyncSendsOnlyWhatIsMissing);
    TEST_METHOD(AnUnpairedSenderIsRefused);
    TEST_METHOD(NothingIsSentToTheWrongKey);
    TEST_METHOD(PairingNeedsPairingOpenAndTwoYeses);
};
