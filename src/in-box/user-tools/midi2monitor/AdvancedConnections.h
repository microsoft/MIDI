// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageCapture.h"

namespace midi2monitor
{
    // Advanced mode's connections: one for each endpoint being listened to, all in one session.
    //
    // The SDK's session and connection calls block on the service, and blocking the STA UI
    // thread can deadlock it, so every open and close happens on a thread of this class's own.
    // The window says which endpoints it wants with Desired, and the thread opens and closes
    // connections until that's what is open. Wanting a different set while the thread is busy is
    // fine: it catches up.
    //
    // Shutdown doesn't wait. The thread closes what is open and then ends on its own, holding the
    // capture object until then, so a closing window never waits on the service.
    class AdvancedConnections
    {
    public:
        struct Wanted
        {
            std::wstring EndpointDeviceId{};
            uint16_t Slot{ 0xFFFF };
        };

        // Called on the connection thread with an endpoint that wouldn't open.
        using FailureHandler = std::function<void(std::wstring const& endpointDeviceId)>;

        AdvancedConnections() noexcept = default;
        ~AdvancedConnections() noexcept;

        AdvancedConnections(AdvancedConnections const&) = delete;
        AdvancedConnections& operator=(AdvancedConnections const&) = delete;

        void Start(
            _In_ winrt::com_ptr<MessageCapture> const& capture,
            _In_ winrt::hstring const& sessionName,
            _In_ FailureHandler failureHandler) noexcept;

        // Everything not in the list is closed. An empty list closes every connection.
        void Desired(_In_ std::vector<Wanted> wanted) noexcept;

        // How many connections are open right now.
        size_t OpenCount() const noexcept;

        void Shutdown() noexcept;

    private:
        struct State;

        static void ThreadBody(_In_ std::shared_ptr<State> state) noexcept;

        std::shared_ptr<State> m_state{};
    };
}
