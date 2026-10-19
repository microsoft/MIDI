// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ChordTextTests.h"

#include "..\..\..\user-tools\midi-player\ChordText.h"

#include <windows.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using midiplayer::TransposeChordSymbol;
using midiplayer::TransposeKeySignature;

namespace
{
    // As the player does it: the key comes from the file, and decides the spelling.
    std::string Transpose(_In_ std::string_view symbol, _In_ int32_t semitones, _In_ int32_t fileKeySignature = 0)
    {
        return TransposeChordSymbol(symbol, semitones, TransposeKeySignature(fileKeySignature, semitones));
    }

    void VerifyChord(_In_ std::string_view expected, _In_ std::string const& actual)
    {
        Log::Comment(String().Format(L"expected %S, got %S", std::string{ expected }.c_str(), actual.c_str()));
        VERIFY_IS_TRUE(actual == expected);
    }
}

void ChordTextTests::FindsTheKeyTheMusicMovesTo()
{
    // C up a semitone is D flat, five flats, rather than C sharp, seven sharps.
    VERIFY_ARE_EQUAL(-5, TransposeKeySignature(0, 1));

    // C down a semitone is B, five sharps, rather than C flat.
    VERIFY_ARE_EQUAL(5, TransposeKeySignature(0, -1));

    // F up a fifth is C. B flat down a tone is A flat.
    VERIFY_ARE_EQUAL(0, TransposeKeySignature(-1, 7));
    VERIFY_ARE_EQUAL(-4, TransposeKeySignature(-2, -2));

    // F sharp and G flat tie, so the side the file was on wins.
    VERIFY_ARE_EQUAL(6, TransposeKeySignature(0, 6));
    VERIFY_ARE_EQUAL(-6, TransposeKeySignature(-2, 8));

    // An octave is the same key.
    VERIFY_ARE_EQUAL(3, TransposeKeySignature(3, 12));

    // A key signature out of range in a damaged file is held to seven.
    VERIFY_ARE_EQUAL(TransposeKeySignature(7, 2), TransposeKeySignature(40, 2));
}

void ChordTextTests::LeavesChordsAloneWhenTheKeyDoesNotChange()
{
    VerifyChord("F#m7", Transpose("F#m7", 0));
    VerifyChord("F#m7", Transpose("F#m7", 12));
    VerifyChord("Bb/D", Transpose("Bb/D", -12));
}

void ChordTextTests::MovesTheRootAndKeepsTheRest()
{
    VerifyChord("D", Transpose("C", 2));
    VerifyChord("Gm7", Transpose("F#m7", 1));
    VerifyChord("Cm", Transpose("Am", 3));
    VerifyChord("Gmaj7#11", Transpose("Fmaj7#11", 2));
    VerifyChord("Fsus4", Transpose("Gsus4", -2));

    // A double flat is read as the note it is.
    VerifyChord("Eb", Transpose("Ebb", 1));
}

void ChordTextTests::SpellsNamesToMatchTheNewKey()
{
    // C up a semitone lands in D flat, so every name uses flats.
    VerifyChord("Db", Transpose("C", 1));
    VerifyChord("Gb", Transpose("F", 1));
    VerifyChord("Ab7", Transpose("G7", 1));
    VerifyChord("Bbm", Transpose("Am", 1));

    // Down a semitone lands in B, so they use sharps.
    VerifyChord("F#", Transpose("G", -1));
    VerifyChord("G#m", Transpose("Am", -1));

    // A file in E flat, moved up a tone, is in F, and keeps flats.
    VerifyChord("Bb", Transpose("Ab", 2, -3));

    // Back in C, the names a lead sheet would use.
    VerifyChord("Bb", Transpose("C", -2, 2));
    VerifyChord("F#7", Transpose("E7", 2, -2));
}

void ChordTextTests::MovesABassNoteAfterASlash()
{
    VerifyChord("E/G#", Transpose("D/F#", 2));
    VerifyChord("Ebm7/Db", Transpose("Dm7/C", 1));

    // A slash that is part of the chord, not a bass note.
    VerifyChord("D6/9", Transpose("C6/9", 2));
}

void ChordTextTests::LeavesAnythingElseUnchanged()
{
    VerifyChord("N.C.", Transpose("N.C.", 3));
    VerifyChord("", Transpose("", 3));
    VerifyChord("x", Transpose("x", 3));

    // A bass that is not a whole note name stays as it was.
    VerifyChord("D/Hx", Transpose("C/Hx", 2));
}
