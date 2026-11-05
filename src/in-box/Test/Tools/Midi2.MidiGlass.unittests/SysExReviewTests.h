// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The question asked before a layout from somewhere else may send system exclusive: which
// packets are SysEx, which devices a layout sends it to, and when the question comes up.
class SysExReviewTests
{
    BEGIN_TEST_CLASS(SysExReviewTests)
        TEST_CLASS_PROPERTY(L"TestClassification:Type", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD_SETUP(Setup);
    TEST_METHOD_CLEANUP(Cleanup);

    TEST_METHOD(SysExPacketsAreFoundAndTakenOut);
    TEST_METHOD(DevicesAreListedFromControlsAndSequences);
    TEST_METHOD(TheFingerprintFollowsTheSysEx);
    TEST_METHOD(UnknownSysExAsksWhereverTheFileIs);
    TEST_METHOD(TrustedSysExIsNotAskedAgain);
    TEST_METHOD(ChangedSysExIsAskedAgain);
    TEST_METHOD(ALayoutWithNoSysExIsNeverAsked);
    TEST_METHOD(ALostOrDamagedListAsksAgain);
};
