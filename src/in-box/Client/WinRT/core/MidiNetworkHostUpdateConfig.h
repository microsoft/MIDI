// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkHostUpdateConfig.g.h"

#include "..\..\..\Transport\UdpNetworkMidi2Transport\net2udp_transport_defs.h"
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkHostUpdateConfig : MidiNetworkHostUpdateConfigT<MidiNetworkHostUpdateConfig>
    {
        MidiNetworkHostUpdateConfig() = default;

        MidiNetworkHostUpdateConfig(_In_ winrt::guid const& hostId) { m_hostId = hostId; }

        winrt::guid TransportId() const noexcept { return internal::StringToGuid(MIDI_NETWORK_TRANSPORT_ID); }

        winrt::guid HostId() const noexcept { return m_hostId; }
        void HostId(_In_ winrt::guid const& value) noexcept { m_hostId = value; }

        bool CreateMidi1Ports() const noexcept { return m_createMidi1Ports.value_or(MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT); }
        void CreateMidi1Ports(_In_ bool const value) noexcept { m_createMidi1Ports = value; }

        uint8_t FallbackMidi1PortCount() const noexcept { return m_fallbackMidi1PortCount.value_or(static_cast<uint8_t>(MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT)); }
        void FallbackMidi1PortCount(_In_ uint8_t const value) noexcept { m_fallbackMidi1PortCount = value; }

        winrt::guid NetworkAdapterId() const noexcept { return m_networkAdapterId.value_or(winrt::guid{}); }
        void NetworkAdapterId(_In_ winrt::guid const& value) noexcept;

        winrt::hstring NetworkAdapterName() const noexcept { return m_networkAdapterName.value_or(winrt::hstring{}); }
        void NetworkAdapterName(_In_ winrt::hstring const& value) noexcept { m_networkAdapterName = internal::TrimmedHStringCopy(value); }

        bool AllowNetworkAdapterFallback() const noexcept { return m_allowNetworkAdapterFallback.value_or(true); }
        void AllowNetworkAdapterFallback(_In_ bool const value) noexcept { m_allowNetworkAdapterFallback = value; }

        network::MidiNetworkSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit.value_or(network::MidiNetworkSendSpeedLimit::Unlimited); }
        void SendSpeedLimit(_In_ network::MidiNetworkSendSpeedLimit const& value) noexcept { m_sendSpeedLimit = value; }

        bool ReduceSendSpeedAutomatically() const noexcept { return m_reduceSendSpeedAutomatically.value_or(false); }
        void ReduceSendSpeedAutomatically(_In_ bool const value) noexcept { m_reduceSendSpeedAutomatically = value; }

        json::JsonObject ConfigJson() const noexcept;

    private:
        winrt::guid m_hostId{};

        // Empty until set, so an update only touches what the caller changed. The merge into the
        // configuration file cannot delete a key, so writing a default would overwrite a value.
        std::optional<bool> m_createMidi1Ports{};
        std::optional<uint8_t> m_fallbackMidi1PortCount{};
        std::optional<winrt::guid> m_networkAdapterId{};
        std::optional<winrt::hstring> m_networkAdapterName{};
        std::optional<bool> m_allowNetworkAdapterFallback{};
        std::optional<network::MidiNetworkSendSpeedLimit> m_sendSpeedLimit{};
        std::optional<bool> m_reduceSendSpeedAutomatically{};

        // Set with the id, and how the service finds the adapter again under a new GUID
        winrt::hstring m_networkAdapterPhysicalAddress{};
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkHostUpdateConfig : MidiNetworkHostUpdateConfigT<MidiNetworkHostUpdateConfig, implementation::MidiNetworkHostUpdateConfig>
    {
    };
}
