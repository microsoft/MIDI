// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What the sequencer knows about the endpoints on this PC: which ones are here, which saved
// endpoint a name means, and what each group of a destination speaks. Windows translates MIDI 2.0
// to MIDI 1.0 only for some endpoints, and never because of a function block, so the sequencer
// works out the protocol per group itself (design section 5).
//
// Built on a background thread from the shared endpoint catalog, plus the function blocks of the
// endpoints the sequence actually plays to. Read from any thread, including the engine's.

#include "PlaybackEngine.h"

namespace midisequencer
{
    struct EndpointSummary
    {
        midiapp::LiveEndpoint Live{};

        // What each group speaks as a destination, from its function block or group terminal block.
        std::array<bool, 16> GroupSpeaksMidi2{};

        // How early to send to the device: set in MIDI Settings, or worked out by its transport.
        uint32_t OffsetMicroseconds{ 0 };

        bool Detailed{ false };
    };

    class EndpointDirectory
    {
    public:
        // Takes a new snapshot of the catalog, and looks up the function blocks of the endpoints
        // in `detail`. Blocks on the service: never call it on the UI thread.
        void Refresh(_In_ std::vector<std::wstring> const& detail);

        std::vector<EndpointSummary> Snapshot() const;

        // The live endpoint a saved one means: the saved id when it's here, otherwise the first
        // endpoint with the same name, so a sequence opened on another PC finds its devices.
        std::optional<EndpointSummary> Resolve(_In_ EndpointRef const& endpoint) const;
        std::wstring ResolveId(_In_ EndpointRef const& endpoint) const;

        DestinationInfo Lookup(_In_ EndpointRef const& endpoint, _In_ uint8_t group) const;

        // The built-in General MIDI Synth, when this PC has it.
        std::wstring SynthEndpointId() const;

        static EndpointRef MakeRef(_In_ midiapp::LiveEndpoint const& endpoint);

    private:
        mutable std::mutex m_lock{};
        std::vector<EndpointSummary> m_endpoints{};
        std::wstring m_synthId{};
    };
}
