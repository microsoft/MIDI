// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


void MidiGroupTests::TestBasics()
{
    uint8_t index{ 0x08 };

    MidiGroup g(index);

    VERIFY_ARE_EQUAL(g.Index(), index);
    VERIFY_ARE_EQUAL(g.DisplayValue(), index + 1);
}

void MidiGroupTests::TestLabels()
{
    VERIFY_IS_FALSE(MidiGroup::LongLabel().empty());
    VERIFY_IS_FALSE(MidiGroup::ShortLabel().empty());
}

void MidiGroupTests::TestInvalidData()
{
    // The class is designed to ignore the most significant nibble because
    // that allows for passing in a full status + channel byte without pre-cleaning

    MidiGroup g(0xAC);

    VERIFY_ARE_EQUAL(g.Index(), 0x0C);
}