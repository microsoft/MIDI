// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The engine's output in the app: one Windows MIDI Services connection per endpoint, shared by
// every track that plays to it.
//
// Connections are opened by Prepare, off the engine's thread, because opening one can take tens
// of milliseconds. Send never opens anything and never blocks for long: a message for an endpoint
// that isn't open (unplugged, or not prepared yet) is counted and dropped, the same way a missing
// device stays silent until it's back.

#include <sal.h>

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Midi2.h>

#include "PlaybackEngine.h"

namespace midisequencer
{
    class SessionEngineOutput final : public IEngineOutput
    {
    public:
        explicit SessionEngineOutput(_In_ winrt::Windows::Devices::Midi2::MidiSession const& session);
        ~SessionEngineOutput();

        // Which endpoint device a name means on this PC. The app matches by name, the way MIDI
        // Glass and MIDI Patchbay do; the default uses the id saved in the sequence.
        void SetResolver(_In_ std::function<std::wstring(EndpointRef const&)> resolver);

        // Opens a connection to each endpoint not already open, and closes the ones no longer
        // needed. Call it off the engine's thread when the sequence's destinations change.
        void Prepare(_In_ std::vector<EndpointRef> const& endpoints);

        void CloseAll() noexcept;

        void Send(
            _In_ EndpointRef const& endpoint,
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept override;

        uint64_t DroppedCount() const noexcept { return m_dropped.load(); }

    private:
        std::wstring Resolve(_In_ EndpointRef const& endpoint) const;

        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        std::function<std::wstring(EndpointRef const&)> m_resolver{};

        mutable std::mutex m_lock{};

        // Keyed by endpoint name and saved id together, which is what the engine passes.
        std::map<std::pair<std::wstring, std::wstring>, winrt::Windows::Devices::Midi2::MidiEndpointConnection> m_connections{};

        std::atomic<uint64_t> m_dropped{ 0 };
    };
}
