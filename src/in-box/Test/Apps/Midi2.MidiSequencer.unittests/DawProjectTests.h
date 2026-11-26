// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// DAWproject files in and out: what comes back from a round trip, what another app's file turns
// into, and what an import refuses.
class DawProjectTests : public WEX::TestClass<DawProjectTests>
{
public:

    BEGIN_TEST_CLASS(DawProjectTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(ASequenceComesBackFromARoundTrip);
    TEST_METHOD(TheExportSaysWhereEachTrackPlayed);
    TEST_METHOD(AnotherAppsProjectImports);
    TEST_METHOD(SecondsBecomeTicksAtTheProjectTempo);
    TEST_METHOD(PatternsGoOutAsTheNotesTheyPlay);
    TEST_METHOD(ThingsTheFormatCantHoldAreCounted);
    TEST_METHOD(JunkIsRefused);
};
