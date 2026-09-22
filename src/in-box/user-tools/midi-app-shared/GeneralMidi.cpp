// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, like the rest of the file model layer.

#include "GeneralMidi.h"

namespace midiapp
{
    namespace
    {
        // ==========================================================================================
        // BEGIN NON-LOCALIZED SPECIFICATION NAMES. See the explanation in GeneralMidi.h before
        // moving any of this to a .resw: a translated fallback would disagree with the device
        // supplied names it sits alongside, and with the instrument's own front panel.
        // ==========================================================================================

        // General MIDI System Level 1, RP-003, in program number order.
        wchar_t const* const ProgramNames[128]
        {
            L"Acoustic Grand Piano", L"Bright Acoustic Piano", L"Electric Grand Piano", L"Honky-tonk Piano",
            L"Electric Piano 1", L"Electric Piano 2", L"Harpsichord", L"Clavi",
            L"Celesta", L"Glockenspiel", L"Music Box", L"Vibraphone",
            L"Marimba", L"Xylophone", L"Tubular Bells", L"Dulcimer",
            L"Drawbar Organ", L"Percussive Organ", L"Rock Organ", L"Church Organ",
            L"Reed Organ", L"Accordion", L"Harmonica", L"Tango Accordion",
            L"Acoustic Guitar (nylon)", L"Acoustic Guitar (steel)", L"Electric Guitar (jazz)", L"Electric Guitar (clean)",
            L"Electric Guitar (muted)", L"Overdriven Guitar", L"Distortion Guitar", L"Guitar harmonics",
            L"Acoustic Bass", L"Electric Bass (finger)", L"Electric Bass (pick)", L"Fretless Bass",
            L"Slap Bass 1", L"Slap Bass 2", L"Synth Bass 1", L"Synth Bass 2",
            L"Violin", L"Viola", L"Cello", L"Contrabass",
            L"Tremolo Strings", L"Pizzicato Strings", L"Orchestral Harp", L"Timpani",
            L"String Ensemble 1", L"String Ensemble 2", L"SynthStrings 1", L"SynthStrings 2",
            L"Choir Aahs", L"Voice Oohs", L"Synth Voice", L"Orchestra Hit",
            L"Trumpet", L"Trombone", L"Tuba", L"Muted Trumpet",
            L"French Horn", L"Brass Section", L"SynthBrass 1", L"SynthBrass 2",
            L"Soprano Sax", L"Alto Sax", L"Tenor Sax", L"Baritone Sax",
            L"Oboe", L"English Horn", L"Bassoon", L"Clarinet",
            L"Piccolo", L"Flute", L"Recorder", L"Pan Flute",
            L"Blown Bottle", L"Shakuhachi", L"Whistle", L"Ocarina",
            L"Lead 1 (square)", L"Lead 2 (sawtooth)", L"Lead 3 (calliope)", L"Lead 4 (chiff)",
            L"Lead 5 (charang)", L"Lead 6 (voice)", L"Lead 7 (fifths)", L"Lead 8 (bass + lead)",
            L"Pad 1 (new age)", L"Pad 2 (warm)", L"Pad 3 (polysynth)", L"Pad 4 (choir)",
            L"Pad 5 (bowed)", L"Pad 6 (metallic)", L"Pad 7 (halo)", L"Pad 8 (sweep)",
            L"FX 1 (rain)", L"FX 2 (soundtrack)", L"FX 3 (crystal)", L"FX 4 (atmosphere)",
            L"FX 5 (brightness)", L"FX 6 (goblins)", L"FX 7 (echoes)", L"FX 8 (sci-fi)",
            L"Sitar", L"Banjo", L"Shamisen", L"Koto",
            L"Kalimba", L"Bag pipe", L"Fiddle", L"Shanai",
            L"Tinkle Bell", L"Agogo", L"Steel Drums", L"Woodblock",
            L"Taiko Drum", L"Melodic Tom", L"Synth Drum", L"Reverse Cymbal",
            L"Guitar Fret Noise", L"Breath Noise", L"Seashore", L"Bird Tweet",
            L"Telephone Ring", L"Helicopter", L"Applause", L"Gunshot"
        };

        // The sixteen families General MIDI groups the programs into, eight programs each.
        wchar_t const* const FamilyNames[16]
        {
            L"Piano", L"Chromatic Percussion", L"Organ", L"Guitar",
            L"Bass", L"Strings", L"Ensemble", L"Brass",
            L"Reed", L"Pipe", L"Synth Lead", L"Synth Pad",
            L"Synth Effects", L"Ethnic", L"Percussive", L"Sound Effects"
        };

        struct DrumKit
        {
            uint8_t Program;
            wchar_t const* Name;
        };

        // Only the programs which select a named kit; anything else falls back to the standard kit.
        constexpr DrumKit DrumKits[]
        {
            {   0, L"Standard Kit" },
            {   8, L"Room Kit" },
            {  16, L"Power Kit" },
            {  24, L"Electronic Kit" },
            {  25, L"TR-808 Kit" },
            {  32, L"Jazz Kit" },
            {  40, L"Brush Kit" },
            {  48, L"Orchestra Kit" },
            {  56, L"Sound FX Kit" }
        };

        // ==========================================================================================
        // END NON-LOCALIZED SPECIFICATION NAMES.
        // ==========================================================================================
    }

    _Use_decl_annotations_
    std::wstring GeneralMidiProgramName(uint8_t programNumber) noexcept
    {
        try
        {
            if (programNumber > 127)
            {
                return {};
            }

            return ProgramNames[programNumber];
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring GeneralMidiDrumKitName(uint8_t programNumber) noexcept
    {
        try
        {
            wchar_t const* name = DrumKits[0].Name;

            for (auto const& kit : DrumKits)
            {
                if (kit.Program == programNumber)
                {
                    name = kit.Name;
                    break;
                }
            }

            return name;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring GeneralMidiFamilyName(uint8_t programNumber) noexcept
    {
        try
        {
            if (programNumber > 127)
            {
                return {};
            }

            return FamilyNames[programNumber / 8];
        }
        catch (...)
        {
            return {};
        }
    }
}
