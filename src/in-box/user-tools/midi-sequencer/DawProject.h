// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// DAWproject import and export (design section 12). A .dawproject file is a zip holding
// project.xml and metadata.xml, from the open format at https://github.com/bitwig/dawproject.
// No XAML: the tests compile this unchanged. XML is read with XmlLite, which refuses DTDs, and a
// file from anywhere is capped in size, depth and count before any of it is used.

#include <sal.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    enum class DawProjectStatus : uint8_t
    {
        Read,

        // Not a zip, or a zip without project.xml.
        NotAProject,

        // The zip or the XML in it can't be read.
        Damaged,

        // Bigger, deeper or busier than an import allows.
        TooLarge,
    };

    // What the file held that a sequence has no place for, so the import can say what it left out.
    struct DawProjectLeftOut
    {
        size_t AudioTracks{ 0 };
        size_t AudioClips{ 0 };
        size_t Plugins{ 0 };

        // Automation of mixer and plug-in settings, and of anything a sequence has no lane for.
        size_t OtherAutomation{ 0 };

        // Per-note expressions, such as a note's own pitch or pressure.
        size_t NoteExpressions{ 0 };
    };

    struct ImportedDawProject
    {
        DawProjectStatus Status{ DawProjectStatus::Read };

        std::vector<TempoPoint> Tempo{};
        std::vector<MeterChange> Meter{};
        std::vector<Tag> Tags{};
        std::vector<Scene> Scenes{};

        // The top level, in order. Folders hold the rest. Every track has a slot for each scene.
        std::vector<Track> Tracks{};
        std::vector<Clip> Clips{};

        std::wstring Title{};
        std::wstring Artist{};
        std::wstring Copyright{};

        // The app that wrote the file.
        std::wstring Application{};

        // The instrument each track played in that app, by track id: a hint for choosing a device.
        std::vector<std::pair<std::wstring, std::wstring>> Instruments{};

        DawProjectLeftOut LeftOut{};
    };

    // Reads only project.xml and metadata.xml: audio and plug-in files in the zip are never opened.
    ImportedDawProject ImportDawProject(_In_ std::span<uint8_t const> file);
    ImportedDawProject ImportDawProjectFile(_In_ std::filesystem::path const& path);

    struct DawProjectText
    {
        std::wstring Application{};
        std::wstring Version{};
        std::wstring Title{};
        std::wstring Artist{};
        std::wstring Copyright{};

        // A track's comment, from the name of the device it plays to. DAWproject has no place for
        // MIDI devices, so this is where the name goes.
        std::function<std::wstring(std::wstring const& device)> PlayedOn{};
    };

    struct DawProjectExport
    {
        std::vector<uint8_t> Bytes{};
        bool Succeeded{ false };

        // Messages the format has no place for: system exclusive, per-note messages and the like
        // in clips, and start-up messages. Controllers, pitch bend, pressure and program changes
        // in clips go out as automation.
        size_t SkippedMessages{ 0 };

        // RPN and NRPN lanes, which it has no target for.
        size_t SkippedLanes{ 0 };
    };

    // Patterns and generators go out as the notes they play, and linked clips as copies.
    DawProjectExport ExportDawProject(_In_ Sequence const& sequence, _In_ DawProjectText const& text);
}
