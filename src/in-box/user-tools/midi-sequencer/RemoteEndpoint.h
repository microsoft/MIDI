// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sequencer's own MIDI endpoint, for remote control (design section 16): a virtual device
// made with Windows MIDI Services. Other apps and devices send to it, and the sequencer sends what's
// playing back on it, so a surface such as a MIDI Glass layout can light up.

#include <functional>
#include <mutex>
#include <string>

namespace midisequencer
{
    class RemoteEndpoint
    {
    public:
        // Called on the service's thread. It must not block.
        using Handler = std::function<void(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount)>;

        explicit RemoteEndpoint(_In_ midi2::MidiSession const& session);
        ~RemoteEndpoint();

        RemoteEndpoint(RemoteEndpoint const&) = delete;
        RemoteEndpoint& operator=(RemoteEndpoint const&) = delete;

        // Makes the endpoint. The names show in other apps and in MIDI Settings. Blocks on the
        // service, so never on the UI thread. False when Windows MIDI Services can't make it.
        // connected is called, on the service's thread, when an app starts using the endpoint.
        bool Open(
            _In_ std::wstring const& name,
            _In_ std::wstring const& description,
            _In_ std::wstring const& blockName,
            _In_ Handler handler,
            _In_ std::function<void()> connected);

        void Close() noexcept;

        bool IsOpen() const noexcept;

        // False when the SDK on this PC can't say when an app starts using the endpoint.
        bool KnowsWhenAppsConnect() const noexcept;

        // Sends now. Messages are dropped while the endpoint isn't open.
        void Send(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept;

    private:
        void Deliver(_In_ midi2::MidiMessageReceivedEventArgs const& args) noexcept;

        midi2::MidiSession m_session{ nullptr };

        mutable std::mutex m_lock{};
        midi2virtual::MidiVirtualDevice m_device{ nullptr };
        midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::event_token m_token{};
        winrt::event_token m_inUseToken{};
        Handler m_handler{};
        std::function<void()> m_connected{};
    };
}
