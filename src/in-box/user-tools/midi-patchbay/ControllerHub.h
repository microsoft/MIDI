// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ControllerSteps.h"

namespace midipatchbay
{
    // Reads every Xbox controller through XInput, on one thread of its own, for as long as anything
    // listens. XInput keeps reading while another app is in front; Windows.Gaming.Input and
    // GameInput stop, and a patch has to keep routing while the customer plays.
    class ControllerHub
    {
    public:
        // Called on the hub's thread with every reading of the player listened to, a few
        // milliseconds apart whether anything changed or not, so a held button keeps time.
        // timestamp is a MIDI timestamp taken as the reading was.
        using Listener = std::function<void(
            uint8_t player,
            uint64_t nowMilliseconds,
            uint64_t timestamp,
            ControllerReading const& reading)>;

        static constexpr int32_t AnyPlayer = -1;

        static ControllerHub& Current();

        ~ControllerHub();

        ControllerHub(ControllerHub const&) = delete;
        ControllerHub& operator=(ControllerHub const&) = delete;

        // False when XInput isn't there to read from.
        bool IsAvailable() noexcept;

        // Returns 0 when it can't listen.
        uint64_t Listen(_In_ int32_t player, _In_ Listener listener) noexcept;

        // Once this returns the listener is never called again, so it must never be called from
        // a listener.
        void StopListening(_In_ uint64_t token) noexcept;

        // One reading now, on the caller's thread.
        ControllerReading Read(_In_ uint8_t player) noexcept;

    private:
        ControllerHub() noexcept;

        struct Subscription;

        void ThreadWorker() noexcept;

        std::mutex m_lock{};
        std::vector<std::shared_ptr<Subscription>> m_subscriptions{};
        uint64_t m_nextToken{ 1 };

        std::thread m_thread{};
        wil::unique_event_nothrow m_wake{};
        wil::unique_event_nothrow m_stop{};
    };
}
