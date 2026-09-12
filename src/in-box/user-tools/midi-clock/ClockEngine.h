// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "BeatClockGenerator.h"

namespace midiclock
{
    enum class ClockStartResult : int32_t
    {
        Success = 0,
        ServiceUnavailable = 1,
        SessionFailed = 2,
        NoEndpointChosen = 3,
        ConnectionFailed = 4
    };

    struct ClockStartRequest
    {
        std::wstring Id{};
        std::wstring EndpointDeviceId{};
        double BeatsPerMinute{ 120.0 };
        int32_t PulsesPerQuarterNote{ 24 };

        // Already resolved against the endpoint, so the engine never has to look a device up.
        std::vector<uint8_t> GroupIndexes{};

        bool SendStartStop{ true };
    };

    // Owns the session, one connection per running clock, and the generator threads.
    //
    // Start and Stop block on the service over RPC, so they run on a background thread. The
    // generators do their own scheduling once started and do not touch this class.
    class ClockEngine
    {
    public:
        ~ClockEngine() noexcept;

        // blocking; never call these from the XAML thread
        //
        // Every request in one call is given the same origin timestamp, which is what puts a
        // "start all" in step: the connections are opened first, and only then is the shared
        // starting instant chosen. The result for each request is keyed by its id.
        std::map<std::wstring, ClockStartResult> Start(
            _In_ std::vector<ClockStartRequest> const& requests) noexcept;

        void Stop(_In_ std::wstring const& id) noexcept;
        void StopAll() noexcept;

        // Applies from the first pulse that has not been scheduled yet. Safe from the UI
        // thread: it touches the generator, not the service.
        void SetBeatsPerMinute(_In_ std::wstring const& id, _In_ double beatsPerMinute) noexcept;

        bool IsRunning(_In_ std::wstring const& id) const noexcept;
        std::vector<std::wstring> RunningIds() const noexcept;

    private:
        struct RunningClock
        {
            winrt::Windows::Devices::Midi2::MidiEndpointConnection Connection{ nullptr };
            std::unique_ptr<midiapp::BeatClockGenerator> Generator{};
        };

        // caller holds m_lock
        void StopUnderLock(_In_ std::wstring const& id) noexcept;

        mutable std::recursive_mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        std::map<std::wstring, RunningClock> m_running{};
    };
}
