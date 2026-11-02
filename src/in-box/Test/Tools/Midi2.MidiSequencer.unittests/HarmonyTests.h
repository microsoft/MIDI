// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The Key and chords row: chord names spelled in the key, typed chords read back, Roman numerals,
// the notes a chord plays, keeping the row in order, its undo step and the sequence file.
class HarmonyTests : public WEX::TestClass<HarmonyTests>
{
public:

    BEGIN_TEST_CLASS(HarmonyTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(NamesAreSpelledInTheKey);
    TEST_METHOD(EveryTypeHasItsSuffix);
    TEST_METHOD(EveryNameReadsBack);
    TEST_METHOD(TypedChordsAndAliases);
    TEST_METHOD(RomanNumerals);
    TEST_METHOD(ChordsInAndOutOfTheKey);
    TEST_METHOD(ChordNotesAndPitchClasses);
    TEST_METHOD(KeyAndChordAtATick);
    TEST_METHOD(PlacingAChordCutsWhatItCovers);
    TEST_METHOD(NormalizeSortsAndCleans);
    TEST_METHOD(TransposeMovesOnlyTheRange);
    TEST_METHOD(UndoPutsTheRowBack);
    TEST_METHOD(SequenceFileRoundTrip);
    TEST_METHOD(SequenceFileWithoutKeyAndChords);
    TEST_METHOD(DamagedEntriesAreSkipped);
    TEST_METHOD(OlderTicksAreScaled);
};
