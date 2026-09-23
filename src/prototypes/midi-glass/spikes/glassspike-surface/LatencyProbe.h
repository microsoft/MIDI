// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

namespace gspike
{
    // The send half of the latency question. Opening is slow and blocks on the service, so it
    // happens on a background thread; sending is what the pointer handler is allowed to do.
    class LatencyProbe
    {
    public:
        // Never call this from the UI thread. MidiSession and CreateEndpointConnection block on
        // midisrv, and an STA thread that blocks cannot service the nested call.
        bool Open(std::wstring const& endpointDeviceId) noexcept;
        void Close() noexcept;

        bool IsOpen() const noexcept { return m_connection != nullptr; }
        std::wstring const& Status() const noexcept { return m_status; }
        std::wstring const& EndpointDeviceId() const noexcept { return m_endpointDeviceId; }

        // The hot path. Called from the pointer event handler. Returns the QPC microsecond count
        // taken immediately after the message left, or 0 if nothing was sent.
        int64_t SendControlChange(uint8_t group, uint8_t channel, uint8_t controller, float value) noexcept;

        static std::wstring DefaultEndpointDeviceId();

    private:
        winrt::midi2::MidiSession m_session{ nullptr };
        winrt::midi2::MidiEndpointConnection m_connection{ nullptr };
        std::wstring m_endpointDeviceId;
        std::wstring m_status{ L"not opened" };
        uint64_t m_sendImmediately{ 0 };
    };
}
