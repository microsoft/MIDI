// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sequencer's own connection to a Novation Launchpad X, separate from the ones it records
// and plays through, so what the pads send drives the sequencer rather than a track.

#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "LaunchpadXController.h"

namespace midisequencer
{
    // Made with std::make_shared: the service's callbacks hold it weakly.
    class LaunchpadXEndpoint final : public launchpad::ILaunchpadOutput, public std::enable_shared_from_this<LaunchpadXEndpoint>
    {
    public:
        // Called on the service's thread. It must not block.
        using Handler = std::function<void(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount, _In_ uint64_t timestamp)>;

        explicit LaunchpadXEndpoint(_In_ midi2::MidiSession const& session);
        ~LaunchpadXEndpoint() override;

        LaunchpadXEndpoint(LaunchpadXEndpoint const&) = delete;
        LaunchpadXEndpoint& operator=(LaunchpadXEndpoint const&) = delete;

        // Connects to the endpoint. Blocks on the service, so never on the UI thread. False when
        // it can't be opened, such as when the device was just unplugged.
        bool Open(_In_ std::wstring const& endpointId, _In_ Handler handler);
        void Close() noexcept;

        std::wstring EndpointId() const;

        bool SendUmpPackets(_In_ std::span<const launchpad::UmpPacket> packets) noexcept override;

    private:
        void Deliver(_In_ midi2::MidiMessageReceivedEventArgs const& args) noexcept;

        midi2::MidiSession m_session{ nullptr };

        mutable std::mutex m_lock{};
        midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::event_token m_token{};
        std::wstring m_endpointId{};
        Handler m_handler{};
    };
}
