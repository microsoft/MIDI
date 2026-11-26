// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// MIDI 2.0 clip files (M2-116-U), written and read by this app's own code.
class ClipFileTests : public WEX::TestClass<ClipFileTests>
{
public:

    BEGIN_TEST_CLASS(ClipFileTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AFileHasTheShapeTheSpecDescribes);
    TEST_METHOD(ALongGapUsesANoop);
    TEST_METHOD(WhatIsWrittenIsReadBack);
    TEST_METHOD(AFileThatEndsEarlyKeepsWhatWasRead);
    TEST_METHOD(SomethingElseIsRefused);
    TEST_METHOD(TempoTimeSignatureAndTextMessages);
    TEST_METHOD(ATrackExportsAndImportsBack);
};

// Standard MIDI Files, through the shared reader and writer.
class StandardMidiFileTests : public WEX::TestClass<StandardMidiFileTests>
{
public:

    BEGIN_TEST_CLASS(StandardMidiFileTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(AnExportReadsBackWithTheSameNotes);
    TEST_METHOD(VelocityScalesUpOnImportAndBackOnExport);
    TEST_METHOD(WhatMidi1CantSayIsCounted);
    TEST_METHOD(TagsTravelAsMarkers);
};
