// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The device link the tools use to reach each other on a network: the frames, the pairing code,
// the list of paired PCs, and a real TLS connection between two keys on this PC.
class DeviceLinkTests
{
    BEGIN_TEST_CLASS(DeviceLinkTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    // ---- frames ----

    TEST_METHOD(AFrameRoundTrips);
    TEST_METHOD(AnOversizedFrameIsRefusedBeforeAllocating);
    TEST_METHOD(ATruncatedFrameIsNamedAsSuch);
    TEST_METHOD(AnUnexpectedTypeIsRefused);
    TEST_METHOD(ACleanCloseIsNotTruncation);
    TEST_METHOD(TheReaderStopsAtTheEnd);
    TEST_METHOD(TheReaderRefusesBadText);

    // ---- pairing ----

    TEST_METHOD(BothSidesGetTheSameCode);
    TEST_METHOD(APersonInTheMiddleGetsDifferentCodes);
    TEST_METHOD(ANonceThatDoesNotMatchItsCommitmentIsRefused);
    TEST_METHOD(TheCodeIsSixDigits);

    // ---- keys and the paired list ----

    TEST_METHOD(AKeyHashRoundTripsThroughHex);
    TEST_METHOD(ThePairedListRoundTrips);
    TEST_METHOD(ThePairedListCleansNames);
    TEST_METHOD(RemovingAPairingForgetsIt);

    // ---- the connection ----

    TEST_METHOD(TwoKeysConnectOverTls);
    TEST_METHOD(PairingOverTheConnectionAgrees);
    TEST_METHOD(ALargeFrameCrossesTheConnection);
};
