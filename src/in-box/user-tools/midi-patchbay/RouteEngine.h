// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "PatchModel.h"
#include "SendQueue.h"

namespace midipatchbay
{
    // One hop in the flattened routing table. The UI turns patches plus the live endpoint list
    // into these; the engine knows nothing about patches, which is what keeps the routing layer
    // free of anything a future API would not want.
    struct RoutePlanEntry
    {
        std::wstring ConnectionId{};
        std::wstring SourceEndpointDeviceId{};
        std::wstring DestinationEndpointDeviceId{};
        int32_t SourceGroupIndex{ AllGroups };
        int32_t DestinationGroupIndex{ AllGroups };

        MessageFilter Filter{};
        MessageTransform Transform{};

        // A multiple of MIDI 1.0 wire speed, 0 for no limit
        uint32_t SendSpeedLimit{ 0 };

        // From the patch: each send waits until the service has taken it
        bool WaitForSendComplete{ false };
    };

    struct RouteStats
    {
        uint64_t MessagesForwarded{ 0 };
        uint64_t SendFailures{ 0 };

        // Only a connection with a sending speed, or in a patch that waits for each send to
        // complete, holds messages back
        uint64_t MessagesWaiting{ 0 };
        uint64_t MessagesDropped{ 0 };

        bool IsActive{ false };
    };

    // Owns the session, the open connections and the forwarding.
    //
    // Receiving and sending both go through the COM extensions and the forward happens inline on
    // the service callback thread: no queue, no worker, no allocation once the plan is applied.
    // The timestamp the service delivered is the timestamp sent on, so a message scheduled for
    // the future stays scheduled rather than being flattened to "now" by the hop.
    //
    // The exception is a connection that has to hold messages back: one with a sending speed, or
    // in a patch that waits for each send to complete. The callback thread must not wait for
    // either, so it queues them, and a send thread for that destination sends them on.
    //
    // Everything here must be called from a background thread. The SDK's session and connection
    // calls block on the service, and blocking the STA UI thread hangs the app.
    class RouteEngine
    {
    public:
        static RouteEngine& Current() noexcept;

        ~RouteEngine() noexcept;

        // Replaces the whole routing table. A plan identical to the running one is a no-op, so
        // an unrelated device arriving does not interrupt connections that did not change.
        void Apply(_In_ std::vector<RoutePlanEntry> plan) noexcept;

        void Shutdown() noexcept;

        std::unordered_map<std::wstring, RouteStats> Stats() const noexcept;

        winrt::hstring LastErrorMessage() const noexcept;

        size_t ActiveRouteCount() const noexcept;

        // Plays one note on an endpoint so a mapping can be heard. Reuses the open connection
        // when the patch is already routing, and otherwise opens one for the length of the note.
        // Blocks for the duration, so it has to be called from a background thread.
        bool SendTestNote(
            _In_ std::wstring const& endpointDeviceId,
            _In_ int32_t groupIndex,
            _In_ uint8_t noteIndex) noexcept;

    private:
        RouteEngine() noexcept = default;

        struct DestinationSender;

        struct Target
        {
            winrt::com_ptr<IMidiEndpointConnectionRaw> Destination{ nullptr };

            int32_t SourceGroupIndex{ AllGroups };
            int32_t DestinationGroupIndex{ AllGroups };

            // Copied when the plan is applied, then only read by the callback thread.
            MessageFilter Filter{};
            MessageTransform Transform{};

            std::wstring ConnectionId{};

            // Sized when the plan is applied, then only touched by the callback thread.
            std::vector<uint32_t> SendBuffer{};
            size_t SendBufferUsed{ 0 };
            uint32_t SendBufferMessages{ 0 };

            // Set when this connection holds messages back. The callback thread adds to the
            // queue, and the sender takes from it.
            std::unique_ptr<SendQueue> Queue{};
            DestinationSender* Sender{ nullptr };

            std::atomic<uint64_t> MessagesForwarded{ 0 };
            std::atomic<uint64_t> SendFailures{ 0 };
            std::atomic<uint64_t> MessagesDropped{ 0 };
        };

        struct SourceHub;

        // A destination connection, and whether its sends wait to complete. Each patch's setting
        // gets connections of its own, so one patch's setting never changes another's.
        using ConnectionKey = std::pair<std::wstring, bool>;

        void TearDownLocked() noexcept;

        static std::wstring BuildSignature(_In_ std::vector<RoutePlanEntry> const& plan) noexcept;

        mutable std::mutex m_lock{};

        midi2::MidiSession m_session{ nullptr };

        // Keyed by lowercased endpoint device id, so one endpoint is only ever opened once for
        // each setting even when several patches use it.
        std::map<ConnectionKey, midi2::MidiEndpointConnection> m_connections{};

        std::vector<winrt::com_ptr<SourceHub>> m_hubs{};

        std::vector<std::unique_ptr<DestinationSender>> m_senders{};

        std::wstring m_signature{};
        winrt::hstring m_lastError{};
        size_t m_activeRoutes{ 0 };
    };
}
