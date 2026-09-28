// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Rtp.MidiRtpClientMatchCriteria.g.h"

#include "MidiRtpSdkJson.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpClientMatchCriteria : MidiRtpClientMatchCriteriaT<MidiRtpClientMatchCriteria>
    {
        MidiRtpClientMatchCriteria() = default;

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        void ServiceInstanceName(_In_ winrt::hstring const& value) noexcept { m_serviceInstanceName = internal::TrimmedHStringCopy(value); }

        winrt::hstring DirectHostNameOrIPAddress() const noexcept { return m_directHostNameOrIPAddress; }
        void DirectHostNameOrIPAddress(_In_ winrt::hstring const& value) noexcept { m_directHostNameOrIPAddress = internal::TrimmedHStringCopy(value); }

        uint16_t DirectPort() const noexcept { return m_directPort; }
        void DirectPort(_In_ uint16_t const value) noexcept { m_directPort = value; }

    private:
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_directHostNameOrIPAddress{};
        uint16_t m_directPort{ MIDI_RTP_SDK_DEFAULT_HOST_PORT };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::factory_implementation
{
    struct MidiRtpClientMatchCriteria : MidiRtpClientMatchCriteriaT<MidiRtpClientMatchCriteria, implementation::MidiRtpClientMatchCriteria>
    {
    };
}
