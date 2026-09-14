// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace midiapp
{
    struct MidiForwardRoute
    {
        // Zero-based, as the UMP itself carries them. Callers working in group numbers convert
        // on the way in.
        uint8_t SourceGroupIndex{ 0 };
        uint8_t DestinationGroupIndex{ 0 };
    };

    // Forwards UMPs from one open connection to another, rewriting the group as it goes.
    //
    // Receiving and sending both go through the COM extensions, and the forward happens inline on
    // the service callback thread: no queue, no worker, no allocation once Start has run. That is
    // what keeps the hop transparent. The timestamp the service delivered is the timestamp sent
    // on, so the destination sees the original scheduling intent rather than the time this
    // process happened to wake up, and a batch that arrived together is sent together.
    class MidiMessageForwarder
    {
    public:
        MidiMessageForwarder() = default;
        ~MidiMessageForwarder();

        MidiMessageForwarder(MidiMessageForwarder const&) = delete;
        MidiMessageForwarder& operator=(MidiMessageForwarder const&) = delete;

        // Both connections must already be created. The source callback is registered here, so
        // this has to run before the source connection is opened, as the COM extensions require.
        // Returns an HRESULT so a caller can tell "no COM extensions" from "already open".
        HRESULT Start(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& source,
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& destination,
            _In_ MidiForwardRoute const& route);

        void Stop() noexcept;

        // Messages seen on the source group, whether or not the send succeeded.
        uint64_t MessagesForwarded() const noexcept { return m_messagesForwarded.load(std::memory_order_relaxed); }

        // Batches the destination refused. A non-zero value means messages were lost.
        uint64_t SendFailureCount() const noexcept { return m_sendFailureCount.load(std::memory_order_relaxed); }

        uint64_t MessagesReceived() const noexcept { return m_messagesReceived.load(std::memory_order_relaxed); }

    private:
        struct Callback;

        // Called from the service callback thread.
        void OnMessagesReceived(_In_ uint64_t timestamp, _In_ uint32_t wordCount, _In_ uint32_t const* messages) noexcept;

        void SendBuffered(_In_ uint64_t timestamp) noexcept;

        winrt::com_ptr<IMidiEndpointConnectionRaw> m_sourceRaw{ nullptr };
        winrt::com_ptr<IMidiEndpointConnectionRaw> m_destinationRaw{ nullptr };
        winrt::com_ptr<::IUnknown> m_callback{ nullptr };

        MidiForwardRoute m_route{};

        // Sized once in Start, then only written by the callback thread.
        std::vector<uint32_t> m_sendBuffer{};
        size_t m_sendBufferUsed{ 0 };

        std::atomic<uint64_t> m_messagesReceived{ 0 };
        std::atomic<uint64_t> m_messagesForwarded{ 0 };
        std::atomic<uint64_t> m_sendFailureCount{ 0 };
        std::atomic<bool> m_running{ false };
    };
}
