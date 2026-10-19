// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The connections a sequence records from. Each source endpoint is opened once and shared by
// every track that records from it. Messages arrive on the service's thread and go straight to
// the handler, which must be quick and thread safe: echo is sent from there so it isn't held up.

#include <sal.h>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace midisequencer
{
    class SourceMonitor
    {
    public:
        using Handler = std::function<void(std::wstring const& endpointId, uint64_t timestamp, uint32_t const* words, uint8_t wordCount)>;

        explicit SourceMonitor(_In_ midi2::MidiSession const& session);
        ~SourceMonitor();

        void SetHandler(_In_ Handler handler);

        // The groups of each source the sequencer doesn't record from, one bit each, keyed by the
        // ids given to Prepare. What arrives on one of them never reaches the handler.
        void SetGroupsOff(_In_ std::map<std::wstring, uint16_t> groupsOff);

        // Opens what's in the list and isn't open, and closes what's open and isn't in it. Blocks
        // on the service: never call it on the UI thread.
        void Prepare(_In_ std::vector<std::wstring> const& endpointIds);

        void CloseAll() noexcept;

    private:
        struct Source
        {
            midi2::MidiEndpointConnection Connection{ nullptr };
            winrt::event_token Token{};
        };

        void Deliver(_In_ std::wstring const& endpointId, _In_ midi2::MidiMessageReceivedEventArgs const& args) noexcept;

        midi2::MidiSession m_session{ nullptr };

        std::mutex m_handlerLock{};
        Handler m_handler{};
        std::map<std::wstring, uint16_t> m_groupsOff{};

        std::mutex m_lock{};
        std::map<std::wstring, Source> m_sources{};
    };
}
