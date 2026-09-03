// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    enum class ConnectResult : int32_t
    {
        Success = 0,
        ServiceUnavailable = 1,
        SessionFailed = 2,
        VirtualDeviceFailed = 3,
        ConnectionFailed = 4,
        NoEndpointChosen = 5
    };

    // Owns the session, the optional virtual device and the open connection.
    //
    // Connect and Disconnect block on the service over RPC, so they run on a background
    // thread. Everything else only writes to the service's shared memory queue and is meant
    // to be called straight from the UI thread, on every pointer move, with no marshaling.
    class MidiOutput
    {
    public:
        ~MidiOutput() noexcept;

        // blocking; never call these from the XAML thread
        ConnectResult ConnectVirtualDevice() noexcept;
        ConnectResult ConnectEndpoint(std::wstring const& endpointDeviceId) noexcept;
        void Disconnect() noexcept;

        bool IsConnected() const noexcept;

        // What other applications will see this app as. Empty unless a virtual device is up.
        winrt::hstring ClientEndpointDeviceId() const noexcept;

        void SendNoteOn(uint8_t group, uint8_t channel, uint8_t note, uint16_t velocity) noexcept;
        void SendNoteOff(uint8_t group, uint8_t channel, uint8_t note) noexcept;
        void SendPolyPressure(uint8_t group, uint8_t channel, uint8_t note, uint32_t pressure) noexcept;
        void SendChannelPressure(uint8_t group, uint8_t channel, uint32_t pressure) noexcept;
        void SendPerNoteController(uint8_t group, uint8_t channel, uint8_t note, uint8_t controllerIndex, uint32_t value) noexcept;
        void SendControlChange(uint8_t group, uint8_t channel, uint8_t controllerIndex, uint32_t value) noexcept;
        void SendPitchBend(uint8_t group, uint8_t channel, uint32_t value) noexcept;

        // CC 123, plus a pitch bend reset, for panic and for tearing down
        void SendAllNotesOff(uint8_t group, uint8_t channel) noexcept;

        // The virtual device always plays group 1; a chosen endpoint uses the customer's group.
        static constexpr uint8_t VirtualDeviceGroupIndex = 0;

    private:
        void SendChannelVoiceMessage(
            uint8_t group,
            uint8_t status,
            uint8_t channel,
            uint16_t index,
            uint32_t data) noexcept;

        void DisconnectUnderLock() noexcept;

        mutable std::shared_mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };
        winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::Windows::Devices::Midi2::Transports::Virtual::MidiVirtualDevice m_virtualDevice{ nullptr };
        winrt::hstring m_clientEndpointDeviceId{};
    };
}
