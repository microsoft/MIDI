// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Network.MidiNetworkConfiguredHost.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkConfiguredHost : MidiNetworkConfiguredHostT<MidiNetworkConfiguredHost>
    {
        MidiNetworkConfiguredHost() = default;

        winrt::guid HostId() const noexcept { return m_hostId; }

        bool IsEnabled() const noexcept { return m_isEnabled; }
        bool HasStarted() const noexcept { return m_hasStarted; }

        winrt::hstring ActualPort() const noexcept { return m_actualPort; }
        winrt::hstring ActualAddress() const noexcept { return m_actualAddress; }

        winrt::hstring ConfiguredPort() const noexcept { return m_configuredPort; }
        bool AllowPortFallback() const noexcept { return m_allowPortFallback; }
        bool UsedPortFallback() const noexcept { return m_usedPortFallback; }

        winrt::guid NetworkAdapterId() const noexcept { return m_networkAdapterId; }
        winrt::hstring NetworkAdapterName() const noexcept { return m_networkAdapterName; }
        bool AllowNetworkAdapterFallback() const noexcept { return m_allowNetworkAdapterFallback; }
        bool IsNetworkAdapterMissing() const noexcept { return m_isNetworkAdapterMissing; }
        bool UsedNetworkAdapterFallback() const noexcept { return m_usedNetworkAdapterFallback; }

        winrt::hstring UmpEndpointName() const noexcept { return m_umpEndpointName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }

        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ActualServiceInstanceName() const noexcept { return m_actualServiceInstanceName; }
        bool ServiceInstanceNameWasChanged() const noexcept { return m_serviceInstanceNameWasChanged; }

        bool CreateMidi1Ports() const noexcept { return m_createMidi1Ports; }

        network::MidiNetworkRemoteClientPolicy RemoteClientPolicy() const noexcept { return m_remoteClientPolicy; }

        winrt::Windows::Foundation::Collections::IVectorView<network::MidiNetworkHostConnection> Connections() const noexcept
        {
            return m_connections.GetView();
        }

        network::MidiNetworkSendSpeedLimit SendSpeedLimit() const noexcept { return m_sendSpeedLimit; }
        bool ReduceSendSpeedAutomatically() const noexcept { return m_reduceSendSpeedAutomatically; }

        winrt::Windows::Foundation::Collections::IVectorView<network::MidiNetworkRemoteClientSettings> RemoteClientSettings() const noexcept
        {
            return m_remoteClientSettings.GetView();
        }

        void InternalAddRemoteClientSettings(_In_ network::MidiNetworkRemoteClientSettings const& settings) noexcept
        {
            if (settings != nullptr)
            {
                m_remoteClientSettings.Append(settings);
            }
        }

        void InternalSetSendSpeed(
            _In_ network::MidiNetworkSendSpeedLimit const sendSpeedLimit,
            _In_ bool const reduceSendSpeedAutomatically) noexcept
        {
            m_sendSpeedLimit = sendSpeedLimit;
            m_reduceSendSpeedAutomatically = reduceSendSpeedAutomatically;
        }

        void InternalInitialize(
            _In_ bool const isEnabled,
            _In_ winrt::guid const& hostId,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& serviceInstanceName,
            _In_ winrt::hstring const& actualServiceInstanceName,
            _In_ bool const serviceInstanceNameWasChanged,
            _In_ bool const hasStarted,
            _In_ winrt::hstring const& actualAddress,
            _In_ winrt::hstring const& actualPort,
            _In_ winrt::hstring const& configuredPort,
            _In_ bool const allowPortFallback,
            _In_ bool const usedPortFallback,
            _In_ bool const createMidi1Ports,
            _In_ network::MidiNetworkRemoteClientPolicy const remoteClientPolicy) noexcept
        {
            m_isEnabled = isEnabled;
            m_hostId = hostId;
            m_umpEndpointName = umpEndpointName;
            m_productInstanceId = productInstanceId;
            m_serviceInstanceName = serviceInstanceName;
            m_actualServiceInstanceName = actualServiceInstanceName;
            m_serviceInstanceNameWasChanged = serviceInstanceNameWasChanged;
            m_hasStarted = hasStarted;
            m_actualAddress = actualAddress;
            m_actualPort = actualPort;
            m_configuredPort = configuredPort;
            m_allowPortFallback = allowPortFallback;
            m_usedPortFallback = usedPortFallback;
            m_createMidi1Ports = createMidi1Ports;
            m_remoteClientPolicy = remoteClientPolicy;
        }

        void InternalAddConnection(_In_ network::MidiNetworkHostConnection const& connection) noexcept
        {
            if (connection != nullptr)
            {
                m_connections.Append(connection);
            }
        }

        void InternalSetNetworkAdapter(
            _In_ winrt::guid const& networkAdapterId,
            _In_ winrt::hstring const& networkAdapterName,
            _In_ bool const allowNetworkAdapterFallback,
            _In_ bool const isNetworkAdapterMissing,
            _In_ bool const usedNetworkAdapterFallback) noexcept
        {
            m_networkAdapterId = networkAdapterId;
            m_networkAdapterName = networkAdapterName;
            m_allowNetworkAdapterFallback = allowNetworkAdapterFallback;
            m_isNetworkAdapterMissing = isNetworkAdapterMissing;
            m_usedNetworkAdapterFallback = usedNetworkAdapterFallback;
        }

    private:
        bool m_isEnabled{ false };
        winrt::guid m_hostId{};
        winrt::hstring m_umpEndpointName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_serviceInstanceName{};
        bool m_hasStarted{ false };
        winrt::hstring m_actualAddress{};
        winrt::hstring m_actualPort{};
        winrt::hstring m_actualServiceInstanceName{};
        bool m_serviceInstanceNameWasChanged{ false };
        winrt::hstring m_configuredPort{};
        bool m_allowPortFallback{ true };
        bool m_usedPortFallback{ false };
        winrt::guid m_networkAdapterId{};
        winrt::hstring m_networkAdapterName{};
        bool m_allowNetworkAdapterFallback{ true };
        bool m_isNetworkAdapterMissing{ false };
        bool m_usedNetworkAdapterFallback{ false };
        bool m_createMidi1Ports{ false };
        network::MidiNetworkRemoteClientPolicy m_remoteClientPolicy{ network::MidiNetworkRemoteClientPolicy::AllowAny };

        network::MidiNetworkSendSpeedLimit m_sendSpeedLimit{ network::MidiNetworkSendSpeedLimit::Unlimited };
        bool m_reduceSendSpeedAutomatically{ false };

        winrt::Windows::Foundation::Collections::IVector<network::MidiNetworkHostConnection> m_connections{
            winrt::single_threaded_vector<network::MidiNetworkHostConnection>() };

        winrt::Windows::Foundation::Collections::IVector<network::MidiNetworkRemoteClientSettings> m_remoteClientSettings{
            winrt::single_threaded_vector<network::MidiNetworkRemoteClientSettings>() };
    };
}
