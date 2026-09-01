// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Bluetooth.MidiBluetoothPeripheralStatus.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    struct MidiBluetoothPeripheralStatus : MidiBluetoothPeripheralStatusT<MidiBluetoothPeripheralStatus>
    {
        MidiBluetoothPeripheralStatus() = default;

        bool IsRunning() const noexcept { return m_isRunning; }
        bluetooth::MidiBluetoothProtocol Protocol() const noexcept { return m_protocol; }
        bluetooth::MidiBluetoothPeripheralClientPolicy ClientPolicy() const noexcept { return m_clientPolicy; }

        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> AllowedClients() const noexcept { return m_allowedClients; }
        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> DeniedClients() const noexcept { return m_deniedClients; }
        winrt::hstring AdvertisedName() const noexcept { return m_advertisedName; }
        uint32_t SubscribedClientCount() const noexcept { return m_subscribedClientCount; }
        bool IsClientConnected() const noexcept { return m_isClientConnected; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring EndpointDeviceInstanceId() const noexcept { return m_endpointDeviceInstanceId; }
        uint64_t MessagesReceived() const noexcept { return m_messagesReceived; }
        uint64_t MessagesSent() const noexcept { return m_messagesSent; }
        uint64_t PacketsReceived() const noexcept { return m_packetsReceived; }
        uint64_t PacketsSent() const noexcept { return m_packetsSent; }
        bluetooth::MidiBluetoothPeripheralClient ConnectedClient() const noexcept { return m_connectedClient; }

        void InternalInitializeFromJson(_In_ json::JsonObject const& peripheralJson) noexcept;

    private:
        bool m_isRunning{ false };
        bluetooth::MidiBluetoothProtocol m_protocol{ bluetooth::MidiBluetoothProtocol::Unknown };
        bluetooth::MidiBluetoothPeripheralClientPolicy m_clientPolicy{ bluetooth::MidiBluetoothPeripheralClientPolicy::RequireApproval };

        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> m_allowedClients{
            winrt::single_threaded_vector<bluetooth::MidiBluetoothRememberedClient>().GetView() };

        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> m_deniedClients{
            winrt::single_threaded_vector<bluetooth::MidiBluetoothRememberedClient>().GetView() };
        winrt::hstring m_advertisedName{};
        uint32_t m_subscribedClientCount{ 0 };
        bool m_isClientConnected{ false };
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_endpointDeviceInstanceId{};
        uint64_t m_messagesReceived{ 0 };
        uint64_t m_messagesSent{ 0 };
        uint64_t m_packetsReceived{ 0 };
        uint64_t m_packetsSent{ 0 };
        bluetooth::MidiBluetoothPeripheralClient m_connectedClient{ nullptr };
    };
}
