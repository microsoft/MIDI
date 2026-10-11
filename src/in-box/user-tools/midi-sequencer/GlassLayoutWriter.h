// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Makes a MIDI Glass layout for a sequence (design section 16): a pad for each launcher slot, a
// button for each scene, the transport, and mute, solo and arm for each track, all sending to the
// sequencer's remote control endpoint, found by its name. It follows the published format in
// docs/kb/midi-glass-layouts-for-agents.md and the remote control map in RemoteControl.h.
//
// Free of pch.h and XAML, so the unit tests use it unchanged. The window passes in every word a
// person reads, from its resources.

#include <sal.h>

#include <functional>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace midisequencer
{
    inline constexpr size_t GlassTracksPerPage = 8;
    inline constexpr size_t GlassScenesPerPage = 8;
    inline constexpr size_t GlassMaximumPages = 9;

    struct GlassLayoutText
    {
        std::wstring Name{};
        std::wstring Description{};

        // The remote control endpoint's name, as other apps see it.
        std::wstring EndpointName{};

        // Provenance: what made it, a new lowercase GUID, and when, in UTC.
        std::wstring Tool{};
        std::wstring Id{};
        std::wstring Created{};
        std::wstring Author{};
        std::wstring License{};

        std::wstring Play{};
        std::wstring Stop{};
        std::wstring Record{};
        std::wstring Loop{};
        std::wstring Metronome{};
        std::wstring GoToStart{};
        std::wstring StopAll{};
        std::wstring BackToTimeline{};
        std::wstring Mute{};
        std::wstring Solo{};
        std::wstring Arm{};
        std::wstring StopTrack{};
        std::wstring EmptySlot{};

        // Names that need a number in them.
        std::function<std::wstring(size_t scene)> SceneName{};
        std::function<std::wstring(size_t track)> TrackName{};
        std::function<std::wstring(size_t firstTrack, size_t lastTrack, size_t firstScene, size_t lastScene)> PageName{};

        // Short enough for a 120 pixel tab.
        std::function<std::wstring(size_t page)> PageTabName{};
    };

    // The layout as JSON text, ready to save as a .midilayout file. Tracks and scenes past what
    // fits on GlassMaximumPages pages are left out.
    std::wstring MakeGlassLayout(_In_ Sequence const& sequence, _In_ GlassLayoutText const& text);

    // How many tracks and scenes the layout covers.
    std::pair<size_t, size_t> GlassLayoutCoverage(_In_ Sequence const& sequence) noexcept;
}
