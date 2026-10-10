// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Standard MIDI Files in and out, through the same reader and writer as the MIDI Player and the
// Sequencing API (src/in-box/Inc/midi_file_*), compiled in directly.

#include <sal.h>

#include <cstdint>
#include <string>
#include <vector>

#include "SequenceModel.h"
#include "MidiClipFile.h"

#include "midi_file_sequence.h"

namespace midisequencer
{
    struct StandardMidiFileImportOptions
    {
        // Bank, program, volume and pan at the very start become start-up messages.
        bool StartupFromFile{ true };

        // Markers and cue points become tags on the Tempo and meter track.
        bool MarkersAsTags{ true };
    };

    struct ImportedStandardMidiFile
    {
        std::vector<TempoPoint> Tempo{};
        std::vector<MeterChange> Meter{};
        std::vector<Tag> Tags{};

        // One track and one clip for each track in the file that plays something. Each track has
        // its clip placed at the start of its timeline.
        std::vector<Track> Tracks{};
        std::vector<Clip> Clips{};

        std::wstring Title{};
        std::wstring Copyright{};
    };

    // MIDI 1.0 notes are scaled up to 16-bit velocity (M2-115), so a MIDI 1.0 synth gets back
    // exactly what the file had. Ticks are scaled to 960 per quarter note.
    ImportedStandardMidiFile ImportStandardMidiFile(
        _In_ midifile::MidiSequence const& file,
        _In_ std::wstring const& fileName,
        _In_ StandardMidiFileImportOptions const& options = {});

    struct StandardMidiFileExport
    {
        std::vector<uint8_t> Bytes{};

        // Messages MIDI 1.0 has no way to say: per-note pitch bend and controllers, for example.
        uint32_t SkippedMessages{ 0 };

        bool Succeeded{ false };
    };

    // Format 1: a first track with tempo, meter and the sequence's tags, then one track for each
    // track, in the order they're drawn. An empty list of track ids means every track.
    StandardMidiFileExport ExportStandardMidiFile(
        _In_ Sequence const& sequence,
        _In_ std::vector<std::wstring> const& trackIds,
        _In_ ClipFileText const& text);
}
