// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthPitchEnvelopeTests.h"
#include "MidiSynthTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;
using namespace MidiSynthTestRig;

namespace
{
    // Its pitch envelope holds about a semitone up for as long as the note sounds, and its samples
    // are tuned to expect that.
    constexpr uint8_t Bagpipe = 109;

    // Its pitch envelope starts an octave up and falls to the written pitch.
    constexpr uint8_t SynthDrum = 118;

    constexpr uint8_t NoAttribute = 0x00;

    std::vector<float> Slice(_In_ const std::vector<float>& stereo, _In_ uint32_t sampleRate, _In_ double fromSeconds, _In_ double toSeconds)
    {
        const auto first = static_cast<size_t>(fromSeconds * sampleRate) * 2;
        const auto last = (std::min)(stereo.size(), static_cast<size_t>(toSeconds * sampleRate) * 2);

        return std::vector<float>(stereo.begin() + first, stereo.begin() + last);
    }
}

// A held pitch envelope is part of the instrument's tuning. Without it, Bagpipe plays about a
// semitone flat.
void MidiSynthPitchEnvelopeTests::TestHeldPitchEnvelopeTunesTheInstrument()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    const auto* const bagpipe = collection->FindInstrument(0, 0, Bagpipe, false);
    VERIFY_IS_NOT_NULL(bagpipe);

    const auto articulation = ResolveArticulation(bagpipe->Connections, {});

    Log::Comment(String().Format(L"Bagpipe pitch envelope depth: %.1f cents, sustain %.2f",
        articulation.Eg2ToPitchCents, articulation.Eg2SustainFraction));

    VERIFY_IS_TRUE(articulation.Eg2ToPitchCents > 90.0, L"the sound set raises Bagpipe with its pitch envelope");

    Rig rig(*collection);
    rig.Engine().ProgramChange(0, Bagpipe);
    rig.NoteOn(0, 69, NoAttribute, 0);
    (void)rig.Render(0.3);

    const double hertz = EstimateHertz(rig.Render(0.6), rig.SampleRate(), 300.0, 600.0);
    const double offset = Cents(hertz, 440.0);

    Log::Comment(String().Format(L"Bagpipe A4: %.2f Hz, %.1f cents from 440", hertz, offset));

    VERIFY_IS_TRUE(std::fabs(offset) < 30.0, L"A4 plays at A440, not a semitone flat");
}

// Synth Drum starts high and falls, because its pitch envelope decays to a sustain of zero.
void MidiSynthPitchEnvelopeTests::TestPitchEnvelopeFallsAfterTheStrike()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection);
    rig.Engine().ProgramChange(0, SynthDrum);
    rig.NoteOn(0, 60, NoAttribute, 0);

    const auto output = rig.Render(1.2);
    const auto rate = rig.SampleRate();

    const double early = EstimateHertz(Slice(output, rate, 0.01, 0.07), rate, 100.0, 2000.0);
    const double late = EstimateHertz(Slice(output, rate, 0.8, 1.1), rate, 100.0, 2000.0);
    const double fall = Cents(early, late);

    Log::Comment(String().Format(L"Synth Drum: %.1f Hz at the strike, %.1f Hz later, a fall of %.0f cents", early, late, fall));

    VERIFY_IS_TRUE(early > 0.0 && late > 0.0, L"both pitches can be measured");
    VERIFY_IS_TRUE(fall > 600.0, L"the pitch falls by well over half an octave");
}
