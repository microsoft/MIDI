// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The calculator's math, checked against the MIDI Association's own tables where it has them
// (M2-115 bit scaling) and against values worked out separately for the design comps.

#include "CalculatorMathTests.h"

#include <cmath>

#include "CalcMidi2.h"
#include "CalcPitch.h"
#include "CalcTempo.h"
#include "CalcTime.h"
#include "TapTempo.h"

using namespace midicalc;

namespace
{
    bool Near(double actual, double expected, double tolerance)
    {
        return std::abs(actual - expected) <= tolerance;
    }

    std::chrono::steady_clock::time_point At(int milliseconds)
    {
        return std::chrono::steady_clock::time_point{} + std::chrono::hours{ 1 } + std::chrono::milliseconds{ milliseconds };
    }
}

// ---------------------------------------------------------------- tap tempo

void CalculatorMathTests::TapTempoNeedsTwoTaps()
{
    midiapp::TapTempo tap{ 20.0, 300.0, 0.0 };
    VERIFY_IS_FALSE(tap.TapAt(At(0)).has_value());
    VERIFY_IS_TRUE(tap.TapAt(At(500)).has_value());
}

void CalculatorMathTests::TapTempoAveragesEvenTaps()
{
    midiapp::TapTempo tap{ 20.0, 300.0, 0.0 };
    std::optional<double> tempo{};

    for (int i = 0; i < 12; ++i)
    {
        tempo = tap.TapAt(At(i * 500));
    }

    VERIFY_IS_TRUE(tempo.has_value());
    VERIFY_IS_TRUE(Near(*tempo, 120.0, 1e-9));
    VERIFY_ARE_EQUAL(midiapp::TapTempo::MaximumTapsConsidered, tap.TapCount());
}

void CalculatorMathTests::TapTempoStartsOverAfterAPause()
{
    midiapp::TapTempo tap{ 20.0, 300.0, 0.0 };
    tap.TapAt(At(0));
    tap.TapAt(At(500));
    tap.TapAt(At(1000));

    // twice the beat is past what a hesitation looks like
    VERIFY_IS_FALSE(tap.TapAt(At(3000)).has_value());
    VERIFY_ARE_EQUAL(size_t{ 1 }, tap.TapCount());

    auto const tempo = tap.TapAt(At(3400));
    VERIFY_IS_TRUE(tempo.has_value());
    VERIFY_IS_TRUE(Near(*tempo, 150.0, 1e-9));
}

void CalculatorMathTests::TapTempoRoundsAndClamps()
{
    midiapp::TapTempo clock{ 20.0, 300.0, 0.5 };
    clock.TapAt(At(0));
    clock.TapAt(At(491));
    auto const rounded = clock.TapAt(At(982));
    VERIFY_IS_TRUE(Near(*rounded, 122.0, 1e-9));

    midiapp::TapTempo fast{ 20.0, 300.0, 0.5 };
    fast.TapAt(At(0));
    VERIFY_IS_TRUE(Near(*fast.TapAt(At(100)), 300.0, 1e-9));
}

void CalculatorMathTests::TapTempoReportsHowEvenTheTapsWere()
{
    midiapp::TapTempo tap{ 20.0, 300.0, 0.0 };
    tap.TapAt(At(0));
    tap.TapAt(At(500));
    VERIFY_IS_FALSE(tap.SpreadMilliseconds().has_value());

    tap.TapAt(At(1010));
    tap.TapAt(At(1500));
    VERIFY_IS_TRUE(Near(*tap.SpreadMilliseconds(), 10.0, 1e-6));
}

// ---------------------------------------------------------------- tempo and beats

void CalculatorMathTests::BeatCalculatorFindsTheTempo()
{
    auto const quarters = QuarterNotesIn(8, 0, Meter{ 4, 4 });
    VERIFY_IS_TRUE(Near(quarters, 32.0, 1e-12));

    auto const tempo = TempoForLength(quarters, 15.6);
    VERIFY_IS_TRUE(tempo.has_value());
    VERIFY_IS_TRUE(Near(*tempo, 123.0769230769, 1e-9));
}

void CalculatorMathTests::BeatCalculatorFindsTheLengthAndTheBars()
{
    VERIFY_IS_TRUE(Near(*SecondsForLength(QuarterNotesIn(16, 0, Meter{ 4, 4 }), 120.0), 32.0, 1e-12));
    VERIFY_IS_TRUE(Near(*BarsInLength(32.0, 120.0, Meter{ 4, 4 }), 16.0, 1e-12));
    VERIFY_IS_TRUE(Near(*BarsInLength(3599.9964, 120.0, Meter{ 4, 4 }), 1799.9982, 1e-9));
}

void CalculatorMathTests::BeatsFollowTheMetersBottomNumber()
{
    VERIFY_IS_TRUE(Near(QuarterNotesPerBar(Meter{ 6, 8 }), 3.0, 1e-12));
    VERIFY_IS_TRUE(Near(QuarterNotesIn(2, 3, Meter{ 6, 8 }), 7.5, 1e-12));
    VERIFY_IS_TRUE(Near(QuarterNotesPerBar(Meter{ 7, 4 }), 7.0, 1e-12));
}

void CalculatorMathTests::RejectsMetersAndTemposOutOfRange()
{
    VERIFY_IS_FALSE(IsValidMeter(Meter{ 4, 3 }));
    VERIFY_IS_FALSE(IsValidMeter(Meter{ 0, 4 }));
    VERIFY_IS_FALSE(TempoForLength(32.0, 0.0).has_value());
    VERIFY_IS_FALSE(TempoForLength(32.0, 0.001).has_value());
    VERIFY_IS_FALSE(SecondsForLength(4.0, 0.0).has_value());
    VERIFY_IS_FALSE(ChangeTempo(120.0, 0.5).has_value());
}

void CalculatorMathTests::NoteLengthsAt120()
{
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::Whole, NoteFeel::Straight, 120.0), 2000.0, 1e-9));
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::Quarter, NoteFeel::Straight, 120.0), 500.0, 1e-9));
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::Quarter, NoteFeel::Dotted, 120.0), 750.0, 1e-9));
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::Quarter, NoteFeel::Triplet, 120.0), 333.3333333, 1e-6));
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::Eighth, NoteFeel::Dotted, 120.0), 375.0, 1e-9));
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::SixtyFourth, NoteFeel::Dotted, 120.0), 46.875, 1e-9));
    VERIFY_IS_TRUE(Near(NoteMilliseconds(NoteValue::SixtyFourth, NoteFeel::Triplet, 120.0), 20.8333333, 1e-6));
}

void CalculatorMathTests::NoteLengthsAsHertzAndSamples()
{
    VERIFY_IS_TRUE(Near(MillisecondsToHertz(500.0), 2.0, 1e-12));
    VERIFY_IS_TRUE(Near(MillisecondsToHertz(0.0), 0.0, 1e-12));
    VERIFY_IS_TRUE(Near(MillisecondsToSamples(500.0, 48000.0), 24000.0, 1e-9));
    VERIFY_IS_TRUE(Near(SamplesToMilliseconds(128.0, 48000.0), 2.6666666667, 1e-9));
    VERIFY_IS_TRUE(Near(SamplesToMilliseconds(1536000.0, 48000.0), 32000.0, 1e-9));
}

void CalculatorMathTests::TempoChangeGivesSpeedLengthAndPitch()
{
    auto const change = ChangeTempo(120.0, 128.0);
    VERIFY_IS_TRUE(change.has_value());
    VERIFY_IS_TRUE(Near(change->Percent, 6.6666666667, 1e-9));
    VERIFY_IS_TRUE(Near(change->LengthRatio, 0.9375, 1e-12));
    VERIFY_IS_TRUE(Near(change->Semitones, 1.1173128527, 1e-9));
    VERIFY_IS_TRUE(Near(*TempoAfterPercent(120.0, 6.6666666667), 128.0, 1e-6));
}

// ---------------------------------------------------------------- notes and pitch

void CalculatorMathTests::MiddleCIsC3LikeTheApi()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"C3" }, NoteName(60, DefaultMiddleCOctave, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"A3" }, NoteName(69, DefaultMiddleCOctave, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C-2" }, NoteName(0, DefaultMiddleCOctave, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"G8" }, NoteName(127, DefaultMiddleCOctave, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C\u266F3" }, NoteName(61, DefaultMiddleCOctave, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"B\u266D3" }, NoteName(70, DefaultMiddleCOctave, true));
    VERIFY_ARE_EQUAL(std::wstring{ L"A3 (69)" }, NoteNameWithNumber(69, DefaultMiddleCOctave));
    VERIFY_IS_TRUE(NoteName(128, DefaultMiddleCOctave, false).empty());

    // the API's default
    VERIFY_ARE_EQUAL(-2, NoteOctave(0, 3));
    VERIFY_ARE_EQUAL(3, NoteOctave(60, 3));
}

void CalculatorMathTests::MiddleCCanBeAnotherOctave()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"C4" }, NoteName(60, 4, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"A4" }, NoteName(69, 4, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C5" }, NoteName(60, 5, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"C-1" }, NoteName(0, 4, false));
    VERIFY_ARE_EQUAL(std::wstring{ L"G9" }, NoteName(127, 4, false));
    VERIFY_ARE_EQUAL(4, NoteOctave(60, 4));
    VERIFY_ARE_EQUAL(5, NoteOctave(60, 5));

    // out of the API's range counts as the default
    VERIFY_ARE_EQUAL(std::wstring{ L"C3" }, NoteName(60, 9, false));
}

void CalculatorMathTests::ParsesNotesWithTheChosenMiddleC()
{
    VERIFY_ARE_EQUAL(69, *ParseNote(L"A3", 3));
    VERIFY_ARE_EQUAL(69, *ParseNote(L"A4", 4));
    VERIFY_ARE_EQUAL(69, *ParseNote(L"69", 4));
    VERIFY_ARE_EQUAL(69, *ParseNote(L"A3 (69)", 4));
    VERIFY_ARE_EQUAL(70, *ParseNote(L"Bb3", 3));
    VERIFY_ARE_EQUAL(66, *ParseNote(L"F\u266F3", 3));
    VERIFY_ARE_EQUAL(0, *ParseNote(L"C-2", 3));
    VERIFY_IS_FALSE(ParseNote(L"H3", 3).has_value());
    VERIFY_IS_FALSE(ParseNote(L"G#8", 3).has_value());
    VERIFY_IS_FALSE(ParseNote(L"", 3).has_value());
}

void CalculatorMathTests::EqualTemperamentFrequencies()
{
    Tuning const tuning{};
    VERIFY_IS_TRUE(Near(NoteFrequency(69, tuning), 440.0, 1e-9));
    VERIFY_IS_TRUE(Near(NoteFrequency(60, tuning), 261.6255653006, 1e-9));
    VERIFY_IS_TRUE(Near(NoteFrequency(40, tuning), 82.4068892282, 1e-9));
    VERIFY_IS_TRUE(Near(NoteFrequency(69.5, tuning), 452.8929841231, 1e-9));

    Tuning const baroque{ 415.0, Temperament::Equal, 0 };
    VERIFY_IS_TRUE(Near(NoteFrequency(69, baroque), 415.0, 1e-9));
}

void CalculatorMathTests::FrequencyToNearestNote()
{
    auto const nearest = NearestNoteToFrequency(450.0, Tuning{});
    VERIFY_IS_TRUE(nearest.has_value());
    VERIFY_ARE_EQUAL(69, nearest->Note);
    VERIFY_IS_TRUE(Near(nearest->Cents, 38.9057732309, 1e-9));
    VERIFY_IS_TRUE(Near(FractionalNote(450.0, 440.0), 69.3890577323, 1e-9));

    auto const flat = NearestNoteToFrequency(430.0, Tuning{});
    VERIFY_ARE_EQUAL(69, flat->Note);
    VERIFY_IS_TRUE(flat->Cents < 0.0);

    VERIFY_IS_FALSE(NearestNoteToFrequency(0.0, Tuning{}).has_value());
    VERIFY_IS_FALSE(NearestNoteToFrequency(40000.0, Tuning{}).has_value());
}

void CalculatorMathTests::EveryTemperamentKeepsTheReferenceNote()
{
    for (int32_t kind = 0; kind < TemperamentCount; ++kind)
    {
        for (int32_t root = 0; root < 12; ++root)
        {
            Tuning const tuning{ 440.0, static_cast<Temperament>(kind), root };
            VERIFY_IS_TRUE(Near(NoteFrequency(69, tuning), 440.0, 1e-9));
            VERIFY_IS_TRUE(Near(NoteFrequency(57, tuning), 220.0, 1e-9));

            auto const cents = TemperamentCents(static_cast<Temperament>(kind));
            VERIFY_IS_TRUE(Near(cents[0], 0.0, 1e-9));

            for (size_t i = 1; i < 12; ++i)
            {
                VERIFY_IS_TRUE(cents[i] > cents[i - 1]);
            }
        }
    }
}

void CalculatorMathTests::JustIntonationHasPureThirdsAndFifths()
{
    Tuning const tuning{ 440.0, Temperament::JustIntonation, 0 };
    auto const c = NoteFrequency(60, tuning);
    VERIFY_IS_TRUE(Near(NoteFrequency(64, tuning) / c, 5.0 / 4.0, 1e-12));
    VERIFY_IS_TRUE(Near(NoteFrequency(67, tuning) / c, 3.0 / 2.0, 1e-12));
    VERIFY_IS_TRUE(Near(TemperamentCents(Temperament::JustIntonation)[4], 386.3137138648, 1e-9));
}

void CalculatorMathTests::MeantoneHasPureMajorThirds()
{
    auto const cents = TemperamentCents(Temperament::QuarterCommaMeantone);
    VERIFY_IS_TRUE(Near(cents[4], 386.3137138648, 1e-9));
    VERIFY_IS_TRUE(Near(cents[7], 696.5784284662, 1e-9));
    // the chain runs E-flat to G-sharp, so the minor third is three fifths down
    VERIFY_IS_TRUE(Near(cents[3], 310.2647146014, 1e-6));
}

void CalculatorMathTests::PythagoreanAndWerckmeisterTables()
{
    VERIFY_IS_TRUE(Near(TemperamentCents(Temperament::Pythagorean)[7], 701.9550008654, 1e-9));
    VERIFY_IS_TRUE(Near(TemperamentCents(Temperament::Pythagorean)[4], 407.8200034615, 1e-9));
    VERIFY_IS_TRUE(Near(TemperamentCents(Temperament::WerckmeisterIII)[4], 390.225, 1e-9));
    VERIFY_IS_TRUE(Near(TemperamentCents(Temperament::Equal)[11], 1100.0, 1e-12));
}

void CalculatorMathTests::IntervalsBetweenNotes()
{
    auto const fifth = IntervalBetween(60, 67);
    VERIFY_ARE_EQUAL(7, fifth.Semitones);
    VERIFY_ARE_EQUAL(7, fifth.NameIndex);
    VERIFY_ARE_EQUAL(0, fifth.Octaves);
    VERIFY_IS_TRUE(Near(fifth.EqualRatio, 1.4983070769, 1e-9));
    VERIFY_IS_TRUE(Near(fifth.PureMinusEqualCents, 1.9550008654, 1e-9));

    auto const down = IntervalBetween(67, 48);
    VERIFY_ARE_EQUAL(-19, down.Semitones);
    VERIFY_ARE_EQUAL(7, down.NameIndex);
    VERIFY_ARE_EQUAL(1, down.Octaves);
}

// ---------------------------------------------------------------- MIDI 2.0 values

void CalculatorMathTests::MinCenterMaxMatchesTheSpecTables()
{
    // M2-115 Table 6, 7 to 32 bits
    VERIFY_ARE_EQUAL(0x00000000u, ScaleUpMinCenterMax(0, 7, 32));
    VERIFY_ARE_EQUAL(0x0A000000u, ScaleUpMinCenterMax(5, 7, 32));
    VERIFY_ARE_EQUAL(0x3C000000u, ScaleUpMinCenterMax(30, 7, 32));
    VERIFY_ARE_EQUAL(0x80000000u, ScaleUpMinCenterMax(64, 7, 32));
    VERIFY_ARE_EQUAL(0x8C30C30Cu, ScaleUpMinCenterMax(70, 7, 32));
    VERIFY_ARE_EQUAL(0xC1041041u, ScaleUpMinCenterMax(96, 7, 32));
    VERIFY_ARE_EQUAL(0xF1C71C71u, ScaleUpMinCenterMax(120, 7, 32));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, ScaleUpMinCenterMax(127, 7, 32));

    // M2-115 Table 7, 16 to 32 bits
    VERIFY_ARE_EQUAL(0x00050000u, ScaleUpMinCenterMax(5, 16, 32));
    VERIFY_ARE_EQUAL(0x80000000u, ScaleUpMinCenterMax(32768, 16, 32));
    VERIFY_ARE_EQUAL(0x9C403880u, ScaleUpMinCenterMax(40000, 16, 32));
    VERIFY_ARE_EQUAL(0xC0008001u, ScaleUpMinCenterMax(49152, 16, 32));
    VERIFY_ARE_EQUAL(0xFDE8FBD1u, ScaleUpMinCenterMax(65000, 16, 32));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, ScaleUpMinCenterMax(65535, 16, 32));

    // the comps' velocity example
    VERIFY_ARE_EQUAL(0xC924u, ScaleUpMinCenterMax(100, 7, 16));
    VERIFY_ARE_EQUAL(0xC9249249u, ScaleUpMinCenterMax(100, 7, 32));
    VERIFY_ARE_EQUAL(12873u, ScaleUpMinCenterMax(100, 7, 14));

    // one bit goes to the ends
    VERIFY_ARE_EQUAL(0xFFFFu, ScaleUpMinCenterMax(1, 1, 16));
    VERIFY_ARE_EQUAL(0u, ScaleUpMinCenterMax(0, 1, 16));
}

void CalculatorMathTests::MinCenterMaxDownscaleCutsBits()
{
    VERIFY_ARE_EQUAL(100u, ScaleDownMinCenterMax(0xC924, 16, 7));
    VERIFY_ARE_EQUAL(100u, ScaleDownMinCenterMax(0xC9249249u, 32, 7));
    VERIFY_ARE_EQUAL(127u, ScaleDownMinCenterMax(0xFFFFFFFFu, 32, 7));

    for (uint32_t v = 0; v < 128; ++v)
    {
        VERIFY_ARE_EQUAL(v, ScaleDownMinCenterMax(ScaleUpMinCenterMax(v, 7, 32), 32, 7));
    }
}

void CalculatorMathTests::ZeroExtensionMatchesTheSpecTables()
{
    // M2-115 Table 9
    VERIFY_ARE_EQUAL(5120u, ScaleUpZeroExtension(10, 7, 16));
    VERIFY_ARE_EQUAL(32768u, ScaleUpZeroExtension(64, 7, 16));
    VERIFY_ARE_EQUAL(44544u, ScaleUpZeroExtension(87, 7, 16));
    VERIFY_ARE_EQUAL(65024u, ScaleUpZeroExtension(127, 7, 16));

    // M2-115 Table 10
    VERIFY_ARE_EQUAL(10u, ScaleDownZeroExtension(5120, 16, 7));
    VERIFY_ARE_EQUAL(11u, ScaleDownZeroExtension(5631, 16, 7));
    VERIFY_ARE_EQUAL(64u, ScaleDownZeroExtension(32768, 16, 7));
    VERIFY_ARE_EQUAL(87u, ScaleDownZeroExtension(44544, 16, 7));
    VERIFY_ARE_EQUAL(87u, ScaleDownZeroExtension(44730, 16, 7));
    VERIFY_ARE_EQUAL(88u, ScaleDownZeroExtension(44800, 16, 7));
    VERIFY_ARE_EQUAL(127u, ScaleDownZeroExtension(65024, 16, 7));
    VERIFY_ARE_EQUAL(127u, ScaleDownZeroExtension(65535, 16, 7));

    // a 32-bit value near the top can't wrap when half a step is added
    VERIFY_ARE_EQUAL(0xFFFFu, ScaleDownZeroExtension(0xFFFFFFFFu, 32, 16));
}

void CalculatorMathTests::Pitch79AndPitch725()
{
    auto const note = FractionalNote(450.0, 440.0);
    VERIFY_ARE_EQUAL(uint16_t{ 0x8AC7 }, Pitch79FromNote(note));
    VERIFY_ARE_EQUAL(0x8AC73293u, Pitch725FromNote(note));

    VERIFY_IS_TRUE(Near(NoteFromPitch79(0x8AC7), 69.0 + 199.0 / 512.0, 1e-12));
    VERIFY_IS_TRUE(Near(NoteFrequency(NoteFromPitch79(0x8AC7), Tuning{}), 449.9899705153, 1e-6));
    VERIFY_IS_TRUE(Near((NoteFromPitch725(0x8AC73293) - 69.0) * 100.0, 38.9057725668, 1e-9));

    VERIFY_ARE_EQUAL(uint16_t{ 69u << 9 }, Pitch79FromNote(69.0));
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, Pitch79FromNote(-3.0));
    VERIFY_ARE_EQUAL(uint16_t{ 0xFFFF }, Pitch79FromNote(200.0));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, Pitch725FromNote(128.0));
}

void CalculatorMathTests::Pitch79WidensTo725WithZeros()
{
    VERIFY_ARE_EQUAL(0x8AC70000u, ScaleUpZeroExtension(0x8AC7, 16, 32));
    VERIFY_ARE_EQUAL(0x8AC7u, ScaleDownZeroExtension(0x8AC73293u, 32, 16));
}

void CalculatorMathTests::PitchBendInBothProtocols()
{
    auto const cents = (FractionalNote(450.0, 440.0) - 69.0) * 100.0;

    VERIFY_ARE_EQUAL(0x98E65267u, PitchBend32FromCents(cents, 200.0));
    VERIFY_ARE_EQUAL(uint16_t{ 9785 }, PitchBend14FromCents(cents, 200.0));
    VERIFY_ARE_EQUAL(0x98E4C726u, ScaleUpMinCenterMax(9785, 14, 32));

    VERIFY_ARE_EQUAL(PitchBend32Center, PitchBend32FromCents(0.0, 200.0));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, PitchBend32FromCents(200.0, 200.0));
    VERIFY_ARE_EQUAL(0u, PitchBend32FromCents(-200.0, 200.0));
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, PitchBend32FromCents(900.0, 200.0));
    VERIFY_ARE_EQUAL(uint16_t{ 0x3FFF }, PitchBend14FromCents(200.0, 200.0));
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, PitchBend14FromCents(-200.0, 200.0));

    VERIFY_IS_TRUE(Near(CentsFromPitchBend32(0x98E65267u, 200.0), cents, 1e-6));
    VERIFY_IS_TRUE(Near(CentsFromPitchBend14(9785, 200.0), cents, 0.02));
    VERIFY_IS_TRUE(Near(CentsFromPitchBend32(0, 200.0), -200.0, 1e-12));
}

void CalculatorMathTests::BendRangeAsRpnData()
{
    VERIFY_ARE_EQUAL(0x04000000u, PitchBendRangeRpnData(2, 0));
    VERIFY_ARE_EQUAL(0x18000000u, PitchBendRangeRpnData(12, 0));
    VERIFY_ARE_EQUAL((2u << 25) | (50u << 18), PitchBendRangeRpnData(2, 50));
}

void CalculatorMathTests::UmpTiming()
{
    VERIFY_IS_TRUE(Near(JrTicksToMilliseconds(0x1234), 149.12, 1e-9));
    VERIFY_IS_TRUE(Near(JrTicksToMilliseconds(1), 0.032, 1e-12));
    VERIFY_IS_TRUE(Near(JrTicksToMilliseconds(0xFFFF) / 1000.0, 2.09712, 1e-9));
    VERIFY_IS_TRUE(Near(*DeltaClockstampMilliseconds(480, 960, 120.0), 250.0, 1e-9));
    VERIFY_IS_FALSE(DeltaClockstampMilliseconds(480, 0, 120.0).has_value());
}

void CalculatorMathTests::ManufacturerIds()
{
    VERIFY_ARE_EQUAL(uint16_t{ 0xA06B }, ManufacturerIdFromMidi1(Midi1ManufacturerId{ 0x00, 0x20, 0x6B, true }));
    VERIFY_ARE_EQUAL(uint16_t{ 0x0041 }, ManufacturerIdFromMidi1(Midi1ManufacturerId{ 0x41, 0, 0, false }));

    auto const three = Midi1FromManufacturerId(0xA06B);
    VERIFY_IS_TRUE(three.IsThreeBytes);
    VERIFY_ARE_EQUAL(uint8_t{ 0x00 }, three.Byte1);
    VERIFY_ARE_EQUAL(uint8_t{ 0x20 }, three.Byte2);
    VERIFY_ARE_EQUAL(uint8_t{ 0x6B }, three.Byte3);

    auto const one = Midi1FromManufacturerId(0x0041);
    VERIFY_IS_FALSE(one.IsThreeBytes);
    VERIFY_ARE_EQUAL(uint8_t{ 0x41 }, one.Byte1);
}

// ---------------------------------------------------------------- Windows time values

void CalculatorMathTests::FileTimeToOtherFormats()
{
    // 2026-11-12 00:30:00.123 UTC
    int64_t const fileTime = 134389170001230000;
    VERIFY_ARE_EQUAL(int64_t{ 639300402001230000 }, FileTimeToDotNetTicks(fileTime));
    VERIFY_ARE_EQUAL(fileTime, DotNetTicksToFileTime(639300402001230000));
    VERIFY_ARE_EQUAL(int64_t{ 1794443400123 }, FileTimeToUnixMilliseconds(fileTime));
    VERIFY_IS_TRUE(Near(FileTimeToUnixSeconds(fileTime), 1794443400.123, 1e-6));
    VERIFY_ARE_EQUAL(fileTime, UnixMillisecondsToFileTime(1794443400123));
    VERIFY_ARE_EQUAL(UnixEpochAsFileTime, UnixSecondsToFileTime(0.0));

    // before 1970 rounds down, not toward zero
    VERIFY_ARE_EQUAL(int64_t{ -1 }, FileTimeToUnixMilliseconds(UnixEpochAsFileTime - 1));
}

void CalculatorMathTests::TimestampsToSecondsKeepTheirDigits()
{
    VERIFY_IS_TRUE(Near(TicksToSeconds(2892714813833ull, 10000000), 289271.4813833, 1e-7));
    VERIFY_IS_TRUE(Near(TicksToSeconds(2500000, 10000000) * 1000.0, 250.0, 1e-9));
    VERIFY_IS_TRUE(Near(TicksToSeconds(5, 0), 0.0, 1e-12));
    VERIFY_ARE_EQUAL(2892717313833ull, *OffsetTimestamp(2892714813833ull, 0.25, 10000000));
    VERIFY_IS_FALSE(OffsetTimestamp(100, -1.0, 10000000).has_value());
}

void CalculatorMathTests::TimestampsAndClockTime()
{
    // the comps: "now" is 7:38:02.481 PM Eastern, 2026-11-12 00:38:02.481 UTC
    ClockPair const now{ 2892714813833ull, 134389174824810000, 10000000 };

    VERIFY_ARE_EQUAL(int64_t{ 134389170001230000 }, *FileTimeForTimestamp(2887891233833ull, now));
    VERIFY_ARE_EQUAL(2887891233833ull, *TimestampForFileTime(134389170001230000, now));

    // in the future works too
    VERIFY_ARE_EQUAL(now.FileTime + 10000000, *FileTimeForTimestamp(now.Timestamp + 10000000, now));

    // before the PC started there's no timestamp
    VERIFY_IS_FALSE(TimestampForFileTime(now.FileTime - 300000 * HundredNanosecondsPerSecond, now).has_value());
}

void CalculatorMathTests::SplitsSecondsIntoHoursAndMinutes()
{
    auto const split = SplitSeconds(289271.4813833);
    VERIFY_ARE_EQUAL(80ull, split.Hours);
    VERIFY_ARE_EQUAL(21u, split.Minutes);
    VERIFY_IS_TRUE(Near(split.Seconds, 11.4813833, 1e-6));

    auto const zero = SplitSeconds(-5.0);
    VERIFY_ARE_EQUAL(0ull, zero.Hours);
}
