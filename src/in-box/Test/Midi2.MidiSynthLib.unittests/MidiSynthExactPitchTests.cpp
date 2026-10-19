// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthExactPitchTests.h"
#include "MidiSynthTestRig.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;
using namespace MidiSynthTestRig;

namespace
{
    constexpr uint8_t AcousticGrandPiano = 0;
    constexpr uint8_t ChurchOrgan = 19;
    constexpr uint8_t StandardKit = 0;

    constexpr uint8_t DrumChannel = 9;
    constexpr uint8_t AcousticBassDrum = 36;
    constexpr uint8_t AcousticSnare = 38;

    constexpr uint8_t NoAttribute = 0x00;
    constexpr uint8_t AttributePitch79 = 0x03;

    // Seven bits of note number and nine of fraction.
    constexpr uint16_t Pitch79(_In_ uint32_t note, _In_ uint32_t fraction) noexcept
    {
        return static_cast<uint16_t>((note << 9) | fraction);
    }

    // Two renders this far apart are different sounds; identical renders differ by nothing at all.
    constexpr double DifferentSound = 1e-3;
    constexpr double SameSound = 1e-6;
}

// The sender in Appendix C.3 plays Middle C as note 0, then note 1, and so on, each with
// Pitch 7.9 = 60.0. Each of those must sound exactly like a plain Middle C. The piano is recorded in
// several ranges, so a region picked by the note number would be another recording, stretched by
// up to several octaves.
void MidiSynthExactPitchTests::TestNoteIndexDoesNotChooseTheSample()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig plain(*collection);
    plain.Engine().ProgramChange(0, AcousticGrandPiano);
    plain.NoteOn(0, 60, NoAttribute, 0);
    const auto middleC = plain.Render(0.25);

    VERIFY_IS_TRUE(Peak(middleC) > 0.01, L"the plain note is heard");

    for (const uint8_t index : { uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 21 }, uint8_t{ 108 }, uint8_t{ 127 } })
    {
        Rig rotated(*collection);
        rotated.Engine().ProgramChange(0, AcousticGrandPiano);
        rotated.NoteOn(0, index, AttributePitch79, Pitch79(60, 0));

        const double difference = LargestDifference(middleC, rotated.Render(0.25));

        Log::Comment(String().Format(L"note index %u at pitch 60.0: largest difference %.7f", index, difference));

        VERIFY_IS_TRUE(difference < SameSound, L"a note index plays the same sound as the plain note at that pitch");
    }

    // A pitch between keys takes the region of the key below it: the top seven bits of the pitch.
    Rig betweenKeysOn60(*collection);
    betweenKeysOn60.Engine().ProgramChange(0, AcousticGrandPiano);
    betweenKeysOn60.NoteOn(0, 60, AttributePitch79, Pitch79(60, 128));
    const auto expected = betweenKeysOn60.Render(0.25);

    Rig betweenKeysOn0(*collection);
    betweenKeysOn0.Engine().ProgramChange(0, AcousticGrandPiano);
    betweenKeysOn0.NoteOn(0, 0, AttributePitch79, Pitch79(60, 128));

    VERIFY_IS_TRUE(LargestDifference(expected, betweenKeysOn0.Render(0.25)) < SameSound,
        L"a fractional pitch plays the same sound whatever the note index");

    VERIFY_IS_TRUE(LargestDifference(expected, middleC) > DifferentSound,
        L"and the fraction is still heard");
}

// On a drum kit the note number picks the drum, so it keeps doing that with a pitch attached, and the
// pitch only tunes the drum. That holds for a kit chosen by bank select on a melodic channel too.
void MidiSynthExactPitchTests::TestDrumKitKeepsTheNoteNumber()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    auto render = [&](uint8_t channel, bool kitByBankSelect, uint8_t note, uint8_t attributeType, uint16_t attributeData)
    {
        Rig rig(*collection);

        if (kitByBankSelect)
        {
            // General MIDI 2 addressing: MSB 120 is a drum kit on any channel.
            rig.Engine().SetBankSelectMode(BankSelectMode::GeneralMidi2);
            rig.Engine().ProgramChangeWithBank(channel, 120, 0, StandardKit);
        }
        else
        {
            rig.Engine().ProgramChange(channel, StandardKit);
        }

        rig.NoteOn(channel, note, attributeType, attributeData);

        return rig.Render(0.25);
    };

    struct Case
    {
        uint8_t Channel;
        bool KitByBankSelect;
        const wchar_t* Name;
    };

    for (const auto& test : { Case{ DrumChannel, false, L"drum channel" }, Case{ 0, true, L"kit by bank select" } })
    {
        const auto snare = render(test.Channel, test.KitByBankSelect, AcousticSnare, NoAttribute, 0);
        const auto kick = render(test.Channel, test.KitByBankSelect, AcousticBassDrum, NoAttribute, 0);

        const auto snareAtItsOwnPitch = render(test.Channel, test.KitByBankSelect, AcousticSnare, AttributePitch79, Pitch79(AcousticSnare, 0));
        const auto snareAtKickPitch = render(test.Channel, test.KitByBankSelect, AcousticSnare, AttributePitch79, Pitch79(AcousticBassDrum, 0));

        Log::Comment(String().Format(L"%s: snare peak %.3f, kick peak %.3f", test.Name, Peak(snare), Peak(kick)));

        VERIFY_IS_TRUE(Peak(snare) > 0.01 && Peak(kick) > 0.01, L"both drums are heard");

        VERIFY_IS_TRUE(LargestDifference(snare, snareAtItsOwnPitch) < SameSound,
            L"a drum at its own pitch is the plain drum");

        VERIFY_IS_TRUE(LargestDifference(kick, snareAtKickPitch) > DifferentSound,
            L"the note number, not the pitch, picks the drum");

        VERIFY_IS_TRUE(LargestDifference(snare, snareAtKickPitch) > DifferentSound,
            L"the pitch still tunes the drum");
    }
}

// Note Off and the per-note messages address the note index. Two notes on one pitch are two notes,
// and the key the pitch falls on is not a note at all.
void MidiSynthExactPitchTests::TestNoteOffAddressesTheNoteIndex()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    Rig rig(*collection);
    rig.Engine().ProgramChange(0, ChurchOrgan);

    rig.NoteOn(0, 0, AttributePitch79, Pitch79(60, 0));
    rig.NoteOn(0, 1, AttributePitch79, Pitch79(60, 0));
    (void)rig.Render(0.1);

    VERIFY_ARE_EQUAL(2u, rig.Engine().ActiveVoiceCount(), L"two note indexes on one pitch are two notes");

    rig.NoteOff(0, 60);
    (void)rig.Render(1.0);

    VERIFY_ARE_EQUAL(2u, rig.Engine().ActiveVoiceCount(), L"a Note Off for the pitch's key ends neither");

    rig.NoteOff(0, 0);
    (void)rig.Render(4.0);

    VERIFY_ARE_EQUAL(1u, rig.Engine().ActiveVoiceCount(), L"a Note Off for one index ends only that note");

    rig.NoteOff(0, 1);
    (void)rig.Render(4.0);

    VERIFY_ARE_EQUAL(0u, rig.Engine().ActiveVoiceCount());
}

// Pitch 7.9 reaches 127 and 511/512. Everything up there is a pitch to play, not something to flatten
// onto note 127.
void MidiSynthExactPitchTests::TestTopOfThePitchRange()
{
    const auto* const collection = RequireSoundSet();

    if (collection == nullptr)
    {
        return;
    }

    auto render = [&](uint16_t pitch)
    {
        Rig rig(*collection);
        rig.Engine().ProgramChange(0, ChurchOrgan);
        rig.NoteOn(0, 60, AttributePitch79, pitch);

        return rig.Render(0.25);
    };

    const auto at127 = render(Pitch79(127, 0));
    const auto atHalf = render(Pitch79(127, 256));
    const auto atTop = render(Pitch79(127, 511));

    VERIFY_IS_TRUE(Peak(at127) > 0.001, L"note 127 is heard");

    VERIFY_IS_TRUE(std::all_of(atTop.begin(), atTop.end(), [](float sample) { return std::isfinite(sample); }),
        L"the highest pitch renders finite samples");

    VERIFY_IS_TRUE(LargestDifference(at127, atHalf) > DifferentSound, L"127.5 is not flattened to 127");
    VERIFY_IS_TRUE(LargestDifference(atHalf, atTop) > DifferentSound, L"nor is the top of the range");
}
