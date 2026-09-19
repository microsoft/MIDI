// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "PatchModel.h"

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
    };

    struct RouteStats
    {
        uint64_t MessagesForwarded{ 0 };
        uint64_t SendFailures{ 0 };
        bool IsActive{ false };
    };

    // Owns the session, the open connections and the forwarding.
    //
    // Receiving and sending both go through the COM extensions and the forward happens inline on
    // the service callback thread: no queue, no worker, no allocation once the plan is applied.
    // The timestamp the service delivered is the timestamp sent on, so a message scheduled for
    // the future stays scheduled rather than being flattened to "now" by the hop.
    //
    // Everything here must be called from a background thread. The SDK's session and connection
    // calls block on the service, and blocking the STA UI thread hangs the app.
    class RouteEngine
    {
    public:
        static RouteEngine& Current() noexcept;

        // Replaces the whole routing table. A plan identical to the running one is a no-op, so
        // an unrelated device arriving does not interrupt connections that did not change.
        void Apply(_In_ std::vector<RoutePlanEntry> plan) noexcept;

        void Shutdown() noexcept;

        std::unordered_map<std::wstring, RouteStats> Stats() const noexcept;

        winrt::hstring LastErrorMessage() const noexcept;

        size_t ActiveRouteCount() const noexcept;

    private:
        RouteEngine() noexcept = default;

        struct Target
        {
            winrt::com_ptr<IMidiEndpointConnectionRaw> Destination{ nullptr };

            int32_t SourceGroupIndex{ AllGroups };
            int32_t DestinationGroupIndex{ AllGroups };

            std::wstring ConnectionId{};

            // Sized when the plan is applied, then only touched by the callback thread.
            std::vector<uint32_t> SendBuffer{};
            size_t SendBufferUsed{ 0 };

            std::atomic<uint64_t> MessagesForwarded{ 0 };
            std::atomic<uint64_t> SendFailures{ 0 };
        };

        struct SourceHub;

        void TearDownLocked() noexcept;

        static std::wstring BuildSignature(_In_ std::vector<RoutePlanEntry> const& plan) noexcept;

        mutable std::mutex m_lock{};

        midi2::MidiSession m_session{ nullptr };

        // Keyed by lowercased endpoint device id, so one endpoint is only ever opened once even
        // when several patches use it.
        std::map<std::wstring, midi2::MidiEndpointConnection> m_connections{};

        std::vector<winrt::com_ptr<SourceHub>> m_hubs{};

        std::wstring m_signature{};
        winrt::hstring m_lastError{};
        size_t m_activeRoutes{ 0 };
    };
}
