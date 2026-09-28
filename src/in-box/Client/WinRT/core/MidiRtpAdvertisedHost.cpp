// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpAdvertisedHost.h"
#include "Transports.Rtp.MidiRtpAdvertisedHost.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    _Use_decl_annotations_
    void MidiRtpAdvertisedHost::InternalInitialize(json::JsonObject const& source) noexcept
    {
        try
        {
            m_serviceInstanceName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY);
            m_hostName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_HOST_NAME_KEY);
            m_port = MidiRtpSdkJson::Unsigned<uint16_t>(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY);
            m_isThisPc = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_IS_THIS_PC_KEY);

            for (auto const& address : MidiRtpSdkJson::Strings(MidiRtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_IPV4_ADDRESSES_KEY)))
            {
                m_ipv4Addresses.Append(address);
                m_ipAddresses.Append(address);
            }

            for (auto const& address : MidiRtpSdkJson::Strings(MidiRtpSdkJson::Array(source, MIDI_CONFIG_JSON_RTP_MIDI_IPV6_ADDRESSES_KEY)))
            {
                m_ipv6Addresses.Append(address);
                m_ipAddresses.Append(address);
            }
        }
        catch (...)
        {
        }
    }
}
