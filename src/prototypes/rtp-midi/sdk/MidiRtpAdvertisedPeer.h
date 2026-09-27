// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once
#include "MidiRtpAdvertisedPeer.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    struct MidiRtpAdvertisedPeer : MidiRtpAdvertisedPeerT<MidiRtpAdvertisedPeer>
    {
        MidiRtpAdvertisedPeer() = default;

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring HostName() const noexcept { return m_hostName; }
        uint16_t Port() const noexcept { return m_port; }
        collections::IVectorView<winrt::hstring> IPv4Addresses() const noexcept { return m_ipv4Addresses; }
        collections::IVectorView<winrt::hstring> IPv6Addresses() const noexcept { return m_ipv6Addresses; }
        bool IsThisPc() const noexcept { return m_isThisPc; }

        void InternalInitialize(_In_ json::JsonObject const& source) noexcept
        {
            m_serviceInstanceName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_hostName = RtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_HOST_NAME_KEY);
            m_port = RtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY);
            m_ipv4Addresses = RtpSdkJson::Strings(RtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_IPV4_ADDRESSES_KEY));
            m_ipv6Addresses = RtpSdkJson::Strings(RtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_IPV6_ADDRESSES_KEY));
            m_isThisPc = RtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_IS_THIS_PC_KEY);
        }

    private:
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_hostName{};
        uint16_t m_port{ 0 };
        collections::IVectorView<winrt::hstring> m_ipv4Addresses{ winrt::single_threaded_vector<winrt::hstring>().GetView() };
        collections::IVectorView<winrt::hstring> m_ipv6Addresses{ winrt::single_threaded_vector<winrt::hstring>().GetView() };
        bool m_isThisPc{ false };
    };
}
