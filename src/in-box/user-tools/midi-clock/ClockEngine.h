// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "BeatClockGenerator.h"
#include "TimeCodeGenerator.h"
#include "ClockStore.h"

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

        int32_t ClockRatioNumerator{ 1 };
        int32_t ClockRatioDenominator{ 1 };
        double SwingPercent{ 50.0 };
        int32_t SwingSubdivision{ 2 };

        // Shifts this clock against the others in the same start. The engine widens the shared
        // lead to cover the earliest of them, so a negative offset still lands in the future.
        double OffsetMilliseconds{ 0.0 };

        ClockKind Kind{ ClockKind::BeatClock };

        // Time code only.
        midiapp::MidiTimeCodeFrameRate FrameRate{ midiapp::MidiTimeCodeFrameRate::Frames30 };
        midiapp::MidiTimeCodePosition StartTimeCode{};
        bool SendFullFrameMessages{ true };
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
        void SetClockRatio(_In_ std::wstring const& id, _In_ int32_t numerator, _In_ int32_t denominator) noexcept;
        void SetSwingPercent(_In_ std::wstring const& id, _In_ double swingPercent) noexcept;

        bool IsRunning(_In_ std::wstring const& id) const noexcept;
        std::vector<std::wstring> RunningIds() const noexcept;

        // Where a running time code clock has reached. False when the id is not a running time
        // code clock, which includes every beat clock.
        bool TryGetTimeCodePosition(
            _In_ std::wstring const& id,
            _Out_ midiapp::MidiTimeCodePosition& position) const noexcept;

    private:
        // One or the other, never both. A clock is a beat clock or it is time code.
        struct RunningClock
        {
            winrt::Windows::Devices::Midi2::MidiEndpointConnection Connection{ nullptr };
            std::unique_ptr<midiapp::BeatClockGenerator> Generator{};
            std::unique_ptr<midiapp::TimeCodeGenerator> TimeCode{};
        };

        // caller holds m_lock
        void StopUnderLock(_In_ std::wstring const& id) noexcept;

        mutable std::recursive_mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        std::map<std::wstring, RunningClock> m_running{};
    };
}
