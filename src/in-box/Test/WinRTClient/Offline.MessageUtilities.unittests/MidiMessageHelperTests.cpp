// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


// Note 60 is middle C. Whatever octave the app calls middle C, note 60 is in that octave, and
// the octave changes every 12 notes from there.

void MidiMessageHelperTests::TestGetNoteOctaveWithDefaultMiddleC()
{
    // middle C is C3 unless told otherwise, so note 0 is C-2 and note 127 is G8
    VERIFY_ARE_EQUAL(int16_t{ 3 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(60));
    VERIFY_ARE_EQUAL(int16_t{ -2 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(0));
    VERIFY_ARE_EQUAL(int16_t{ 2 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(59));
    VERIFY_ARE_EQUAL(int16_t{ 8 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(127));

    VERIFY_ARE_EQUAL(int16_t{ 3 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(60, 3));
    VERIFY_ARE_EQUAL(int16_t{ -2 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(0, 3));
}

void MidiMessageHelperTests::TestGetNoteOctaveWithOtherMiddleC()
{
    for (uint8_t middleC = 1; middleC <= 7; ++middleC)
    {
        VERIFY_ARE_EQUAL(static_cast<int16_t>(middleC), MidiMessageHelper::GetNoteOctaveFromNoteIndex(60, middleC));
        VERIFY_ARE_EQUAL(static_cast<int16_t>(middleC - 1), MidiMessageHelper::GetNoteOctaveFromNoteIndex(59, middleC));
        VERIFY_ARE_EQUAL(static_cast<int16_t>(middleC + 1), MidiMessageHelper::GetNoteOctaveFromNoteIndex(72, middleC));
        VERIFY_ARE_EQUAL(static_cast<int16_t>(middleC - 5), MidiMessageHelper::GetNoteOctaveFromNoteIndex(0, middleC));
    }

    // C4 as middle C, which many apps use
    VERIFY_ARE_EQUAL(int16_t{ -1 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(0, 4));
    VERIFY_ARE_EQUAL(int16_t{ 9 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(127, 4));
}

void MidiMessageHelperTests::TestGetNoteOctaveRejectsBadInput()
{
    VERIFY_ARE_EQUAL(int16_t{ 0 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(128));
    VERIFY_ARE_EQUAL(int16_t{ 0 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(60, 0));
    VERIFY_ARE_EQUAL(int16_t{ 0 }, MidiMessageHelper::GetNoteOctaveFromNoteIndex(60, 8));
}
