// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <WexTestClass.h>

// Glide and vibrato as MIDI 2.0 Per-Note Pitch Bend (design section 36): the messages are shaped
// as M2-104-UM sets out, a glide slides into each note and ends in tune, a vibrato swells and
// comes back to the middle, and doing either again replaces what was there.
class NoteExpressionTests : public WEX::TestClass<NoteExpressionTests>
{
public:

    BEGIN_TEST_CLASS(NoteExpressionTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(MessagesAreShapedAsTheSpecSays);
    TEST_METHOD(BendValuesStayInRange);
    TEST_METHOD(GlideSlidesIntoTheNextNote);
    TEST_METHOD(GlideSkipsChordsGapsAndLeapsTooBig);
    TEST_METHOD(VibratoSwellsAndComesBackToTheMiddle);
    TEST_METHOD(RemoveTakesOnlyThoseNotesBends);
};