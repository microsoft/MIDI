// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "TransposerTests.h"

#include "midi_sequence_transposer.h"

#include <windows.h>

#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using midiplayer::NoteTransposer;

namespace
{
    constexpr uint16_t NoDrums = 0;
    constexpr uint16_t ChannelTenDrums = 1u << 9;

    // MIDI 1.0 channel voice messages in a packet, group 0.
    constexpr uint32_t Midi1(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2) noexcept
    {
        return 0x20000000u
            | (static_cast<uint32_t>((status << 4) | channel) << 16)
            | (static_cast<uint32_t>(data1) << 8)
            | data2;
    }

    constexpr uint32_t NoteOn1(uint8_t channel, uint8_t note, uint8_t velocity = 100) noexcept { return Midi1(0x9, channel, note, velocity); }
    constexpr uint32_t NoteOff1(uint8_t channel, uint8_t note) noexcept { return Midi1(0x8, channel, note, 0); }
    constexpr uint32_t PolyPressure1(uint8_t channel, uint8_t note, uint8_t pressure) noexcept { return Midi1(0xA, channel, note, pressure); }

    // The first word of a MIDI 2.0 channel voice message, group 0.
    constexpr uint32_t Midi2(uint8_t status, uint8_t channel, uint8_t note, uint8_t index) noexcept
    {
        return 0x40000000u
            | (static_cast<uint32_t>((status << 4) | channel) << 16)
            | (static_cast<uint32_t>(note) << 8)
            | index;
    }

    std::vector<uint32_t> Rewrite(
        _In_ NoteTransposer& transposer,
        _In_ std::vector<uint32_t> const& input,
        _In_ uint16_t percussionChannels = NoDrums)
    {
        std::vector<uint32_t> output(input.size() * 2);

        auto const written = transposer.Rewrite(input, output, percussionChannels);

        VERIFY_IS_TRUE(written <= output.size());
        output.resize(written);

        return output;
    }

    void VerifyWords(_In_ std::vector<uint32_t> const& expected, _In_ std::vector<uint32_t> const& actual)
    {
        VERIFY_ARE_EQUAL(expected.size(), actual.size());

        for (size_t index = 0; index < expected.size() && index < actual.size(); ++index)
        {
            if (expected[index] != actual[index])
            {
                Log::Error(String().Format(L"word %d: expected 0x%08X, got 0x%08X",
                    static_cast<int>(index), expected[index], actual[index]));
            }
        }
    }
}

void TransposerTests::ChangesNothingAtZero()
{
    NoteTransposer transposer{};

    std::vector<uint32_t> const input{
        NoteOn1(0, 60), PolyPressure1(0, 60, 30), NoteOff1(0, 60),
        Midi2(0x9, 1, 72, 0x03), 0x80000000u | (72u << 9),
        Midi2(0x6, 1, 72, 0), 0x12345678u,
        Midi2(0x8, 1, 72, 0), 0x40000000u };

    VerifyWords(input, Rewrite(transposer, input));
}

void TransposerTests::ClampsToTheLimits()
{
    NoteTransposer transposer{};

    transposer.SetSemitones(40);
    VERIFY_ARE_EQUAL(NoteTransposer::MaximumSemitones, transposer.Semitones());

    transposer.SetSemitones(-40);
    VERIFY_ARE_EQUAL(NoteTransposer::MinimumSemitones, transposer.Semitones());

    transposer.SetSemitones(-3);
    VERIFY_ARE_EQUAL(-3, transposer.Semitones());
}

void TransposerTests::MovesNotesAndTheirEnds()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(2);

    VerifyWords({ NoteOn1(0, 62) }, Rewrite(transposer, { NoteOn1(0, 60) }));
    VerifyWords({ PolyPressure1(0, 62, 30) }, Rewrite(transposer, { PolyPressure1(0, 60, 30) }));
    VerifyWords({ NoteOff1(0, 62) }, Rewrite(transposer, { NoteOff1(0, 60) }));

    transposer.SetSemitones(-5);

    VerifyWords({ NoteOn1(3, 55) }, Rewrite(transposer, { NoteOn1(3, 60) }));
    VerifyWords({ NoteOff1(3, 55) }, Rewrite(transposer, { NoteOff1(3, 60) }));
}

void TransposerTests::LeavesDrumsAlone()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(5);

    VerifyWords({ NoteOn1(9, 36) }, Rewrite(transposer, { NoteOn1(9, 36) }, ChannelTenDrums));
    VerifyWords({ NoteOff1(9, 36) }, Rewrite(transposer, { NoteOff1(9, 36) }, ChannelTenDrums));

    // Any channel the sequence put on a drum kit.
    uint16_t const channelThreeDrums = ChannelTenDrums | (1u << 2);

    VerifyWords({ NoteOn1(2, 38) }, Rewrite(transposer, { NoteOn1(2, 38) }, channelThreeDrums));

    // While the melodic channels beside them still move.
    VerifyWords({ NoteOn1(0, 65) }, Rewrite(transposer, { NoteOn1(0, 60) }, channelThreeDrums));
}

void TransposerTests::KeepsASoundingNoteAtItsPitch()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(2);

    VerifyWords({ NoteOn1(0, 62) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    // The key changes while the note sounds. Everything aimed at it still reaches it.
    transposer.SetSemitones(7);

    VerifyWords({ PolyPressure1(0, 62, 10) }, Rewrite(transposer, { PolyPressure1(0, 60, 10) }));
    VerifyWords({ NoteOff1(0, 62) }, Rewrite(transposer, { NoteOff1(0, 60) }));

    // Once it has ended, the new transposition applies.
    VerifyWords({ PolyPressure1(0, 67, 10) }, Rewrite(transposer, { PolyPressure1(0, 60, 10) }));
    VerifyWords({ NoteOn1(0, 67) }, Rewrite(transposer, { NoteOn1(0, 60) }));
    VerifyWords({ NoteOff1(0, 67) }, Rewrite(transposer, { NoteOff1(0, 60) }));

    // The same key on another channel is a different note.
    transposer.SetSemitones(0);
    VerifyWords({ NoteOn1(1, 60) }, Rewrite(transposer, { NoteOn1(1, 60) }));
}

void TransposerTests::EndsAKeyStruckAgainAtANewPitch()
{
    NoteTransposer transposer{};

    VerifyWords({ NoteOn1(0, 60) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    // Struck again at the same pitch: nothing extra, the same as without a transposer.
    VerifyWords({ NoteOn1(0, 60) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    // Struck again after the key changed. The note at the old pitch is ended first, because
    // nothing else would end it.
    transposer.SetSemitones(2);

    VerifyWords({ NoteOff1(0, 60), NoteOn1(0, 62) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    VerifyWords({ NoteOff1(0, 62) }, Rewrite(transposer, { NoteOff1(0, 60) }));
    VerifyWords({ NoteOff1(0, 62) }, Rewrite(transposer, { NoteOff1(0, 60) }));

    // The same for MIDI 2.0, with a MIDI 2.0 note off.
    transposer.SetSemitones(0);
    VerifyWords({ Midi2(0x9, 4, 50, 0), 0xC0000000u }, Rewrite(transposer, { Midi2(0x9, 4, 50, 0), 0xC0000000u }));

    transposer.SetSemitones(-1);
    VerifyWords(
        { Midi2(0x8, 4, 50, 0), 0x00000000u, Midi2(0x9, 4, 49, 0), 0xC0000000u },
        Rewrite(transposer, { Midi2(0x9, 4, 50, 0), 0xC0000000u }));
}

void TransposerTests::SendsEveryEndOfARestruckKeyToItsPitch()
{
    NoteTransposer transposer{};

    // Struck twice before either ends, the way a file can.
    VerifyWords({ NoteOn1(0, 60) }, Rewrite(transposer, { NoteOn1(0, 60) }));
    VerifyWords({ NoteOn1(0, 60) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    // The key changes before the ends arrive. Both still reach the pitch the notes started at,
    // so a receiver that gave each strike its own voice ends both.
    transposer.SetSemitones(5);

    VerifyWords({ NoteOff1(0, 60) }, Rewrite(transposer, { NoteOff1(0, 60) }));
    VerifyWords({ PolyPressure1(0, 60, 9) }, Rewrite(transposer, { PolyPressure1(0, 60, 9) }));
    VerifyWords({ NoteOff1(0, 60) }, Rewrite(transposer, { NoteOff1(0, 60) }));

    // Then the key is free, and the new transposition applies.
    VerifyWords({ NoteOff1(0, 65) }, Rewrite(transposer, { NoteOff1(0, 60) }));
    VerifyWords({ NoteOn1(0, 65) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    // A key whose start was out of range takes the next start that fits.
    transposer.SetSemitones(12);
    VERIFY_ARE_EQUAL(size_t{ 0 }, Rewrite(transposer, { NoteOn1(1, 120) }).size());

    transposer.SetSemitones(0);
    VerifyWords({ NoteOn1(1, 120) }, Rewrite(transposer, { NoteOn1(1, 120) }));
    VerifyWords({ NoteOff1(1, 120) }, Rewrite(transposer, { NoteOff1(1, 120) }));
}

void TransposerTests::DropsNotesOutOfRange()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(12);

    VERIFY_ARE_EQUAL(size_t{ 0 }, Rewrite(transposer, { NoteOn1(0, 120) }).size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, Rewrite(transposer, { PolyPressure1(0, 120, 5) }).size());

    // Its end goes nowhere either, even once the key is back in range.
    transposer.SetSemitones(0);
    VERIFY_ARE_EQUAL(size_t{ 0 }, Rewrite(transposer, { NoteOff1(0, 120) }).size());

    // After that the key plays normally.
    VerifyWords({ NoteOn1(0, 120) }, Rewrite(transposer, { NoteOn1(0, 120) }));
    VerifyWords({ NoteOff1(0, 120) }, Rewrite(transposer, { NoteOff1(0, 120) }));

    // And the bottom of the range. Only the note out of range is left out of an event.
    transposer.SetSemitones(-12);

    VerifyWords({ NoteOn1(0, 0) }, Rewrite(transposer, { NoteOn1(0, 5), NoteOn1(0, 12) }));
}

void TransposerTests::KeepsASoundingNoteWhenItsRestartIsOutOfRange()
{
    NoteTransposer transposer{};

    VerifyWords({ NoteOn1(0, 125) }, Rewrite(transposer, { NoteOn1(0, 125) }));

    // The restart cannot play, so the note already sounding is left for the key's next end.
    transposer.SetSemitones(5);

    VERIFY_ARE_EQUAL(size_t{ 0 }, Rewrite(transposer, { NoteOn1(0, 125) }).size());
    VerifyWords({ NoteOff1(0, 125) }, Rewrite(transposer, { NoteOff1(0, 125) }));
}

void TransposerTests::TreatsVelocityZeroAsAnEnd()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(3);

    VerifyWords({ NoteOn1(0, 63) }, Rewrite(transposer, { NoteOn1(0, 60) }));

    transposer.SetSemitones(0);

    // A MIDI 1.0 note on with no velocity ends the note, so it goes to where the note is.
    VerifyWords({ NoteOn1(0, 63, 0) }, Rewrite(transposer, { NoteOn1(0, 60, 0) }));

    // And the key is free again.
    VerifyWords({ NoteOn1(0, 60) }, Rewrite(transposer, { NoteOn1(0, 60) }));
}

void TransposerTests::MovesMidi2NotesAndPerNoteMessages()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(-2);

    VerifyWords(
        { Midi2(0x9, 5, 70, 0), 0xFFFF0000u },
        Rewrite(transposer, { Midi2(0x9, 5, 72, 0), 0xFFFF0000u }));

    // A MIDI 2.0 note on with no velocity is still a note, so this key stays sounding.
    VerifyWords(
        { Midi2(0x9, 5, 58, 0), 0x00000000u },
        Rewrite(transposer, { Midi2(0x9, 5, 60, 0), 0x00000000u }));

    transposer.SetSemitones(4);

    // Poly pressure, per-note controllers, per-note pitch bend and per-note management all
    // follow the note that is sounding.
    VerifyWords({ Midi2(0xA, 5, 70, 0), 0x80000000u }, Rewrite(transposer, { Midi2(0xA, 5, 72, 0), 0x80000000u }));
    VerifyWords({ Midi2(0x0, 5, 70, 7), 0x11111111u }, Rewrite(transposer, { Midi2(0x0, 5, 72, 7), 0x11111111u }));
    VerifyWords({ Midi2(0x1, 5, 70, 9), 0x22222222u }, Rewrite(transposer, { Midi2(0x1, 5, 72, 9), 0x22222222u }));
    VerifyWords({ Midi2(0x6, 5, 70, 0), 0x90000000u }, Rewrite(transposer, { Midi2(0x6, 5, 72, 0), 0x90000000u }));
    VerifyWords({ Midi2(0xF, 5, 70, 1), 0x00000000u }, Rewrite(transposer, { Midi2(0xF, 5, 72, 1), 0x00000000u }));

    VerifyWords({ Midi2(0x8, 5, 70, 0), 0x80000000u }, Rewrite(transposer, { Midi2(0x8, 5, 72, 0), 0x80000000u }));
    VerifyWords({ Midi2(0x8, 5, 58, 0), 0x80000000u }, Rewrite(transposer, { Midi2(0x8, 5, 60, 0), 0x80000000u }));
}

void TransposerTests::MovesMidi2PitchAttributes()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(3);

    // Attribute 3 is the pitch in 7.9 form: note 60 and a quarter.
    uint32_t const quarterAbove60 = (60u << 9) | 0x080u;
    uint32_t const quarterAbove63 = (63u << 9) | 0x080u;

    VerifyWords(
        { Midi2(0x9, 0, 63, 0x03), 0x7F000000u | quarterAbove63 },
        Rewrite(transposer, { Midi2(0x9, 0, 60, 0x03), 0x7F000000u | quarterAbove60 }));

    // Registered per-note controller 3 is the pitch in 7.25 form.
    VerifyWords(
        { Midi2(0x0, 0, 63, 0x03), (63u << 25) | 0x10u },
        Rewrite(transposer, { Midi2(0x0, 0, 60, 0x03), (60u << 25) | 0x10u }));

    // Other attributes and controllers are not pitches.
    VerifyWords(
        { Midi2(0x8, 0, 63, 0x02), 0x7F001234u },
        Rewrite(transposer, { Midi2(0x8, 0, 60, 0x02), 0x7F001234u }));

    // A pitch attribute pushed past the top is held there.
    transposer.SetSemitones(12);

    VerifyWords(
        { Midi2(0x9, 1, 127, 0x03), 0x7F00FFFFu },
        Rewrite(transposer, { Midi2(0x9, 1, 115, 0x03), 0x7F000000u | (126u << 9) }));
}

void TransposerTests::CopiesOtherPacketsUnchanged()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(1);

    // A control change, a note, a MIDI 2.0 program change, a utility message and a system
    // exclusive packet, as one event.
    std::vector<uint32_t> const input{
        Midi1(0xB, 0, 7, 100),
        NoteOn1(0, 60),
        Midi2(0xC, 0, 0, 1), 0x05000000u,
        0x00200000u,
        0x30164110u, 0x42120000u };

    std::vector<uint32_t> const expected{
        Midi1(0xB, 0, 7, 100),
        NoteOn1(0, 61),
        Midi2(0xC, 0, 0, 1), 0x05000000u,
        0x00200000u,
        0x30164110u, 0x42120000u };

    VerifyWords(expected, Rewrite(transposer, input));

    VERIFY_IS_TRUE(NoteTransposer::NamesAnyNote(input));
    VERIFY_IS_FALSE(NoteTransposer::NamesAnyNote(std::vector<uint32_t>{ Midi1(0xB, 0, 7, 100), 0x30164110u, 0x42120000u }));

    // A packet cut short goes out as it came, with the rest of the event.
    std::vector<uint32_t> const cutShort{ NoteOn1(0, 60), Midi2(0x9, 0, 60, 0) };

    VerifyWords({ NoteOn1(0, 61), Midi2(0x9, 0, 60, 0) }, Rewrite(transposer, cutShort));
}

void TransposerTests::ForgetsEverythingWhenCleared()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(2);

    VerifyWords({ NoteOn1(0, 62) }, Rewrite(transposer, { NoteOn1(0, 60) }));
    VERIFY_ARE_EQUAL(size_t{ 0 }, Rewrite(transposer, { NoteOn1(1, 127) }).size());

    // Everything was silenced. The transposition itself stays.
    transposer.Clear();
    VERIFY_ARE_EQUAL(2, transposer.Semitones());

    transposer.SetSemitones(0);

    VerifyWords({ NoteOff1(0, 60) }, Rewrite(transposer, { NoteOff1(0, 60) }));
    VerifyWords({ NoteOn1(1, 127) }, Rewrite(transposer, { NoteOn1(1, 127) }));
}

void TransposerTests::StartsNotesForTheEngine()
{
    NoteTransposer transposer{};
    transposer.SetSemitones(4);

    auto start = transposer.StartNote(0, 60, false);
    VERIFY_ARE_EQUAL(64, static_cast<int>(start.Note));
    VERIFY_ARE_EQUAL(-1, static_cast<int>(start.EndFirst));

    // Its end, arriving as a message, finds it.
    transposer.SetSemitones(0);
    VerifyWords({ NoteOff1(0, 64) }, Rewrite(transposer, { NoteOff1(0, 60) }));

    // Drums are not moved.
    transposer.SetSemitones(4);
    start = transposer.StartNote(9, 36, true);
    VERIFY_ARE_EQUAL(36, static_cast<int>(start.Note));

    // Out of range.
    start = transposer.StartNote(0, 126, false);
    VERIFY_ARE_EQUAL(-1, static_cast<int>(start.Note));

    // A key still sounding at another pitch is ended first.
    VerifyWords({ NoteOn1(2, 64) }, Rewrite(transposer, { NoteOn1(2, 60) }));
    transposer.SetSemitones(5);

    start = transposer.StartNote(2, 60, false);
    VERIFY_ARE_EQUAL(65, static_cast<int>(start.Note));
    VERIFY_ARE_EQUAL(64, static_cast<int>(start.EndFirst));
}
