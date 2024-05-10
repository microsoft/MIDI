// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;
        ~MidiSession();

        static midi2::MidiSession TryCreate(
            _In_ hstring const& sessionName
        ) noexcept;


        winrt::guid Id() const noexcept { return m_id; }
        winrt::hstring Name() const noexcept { return m_name; }
        bool IsOpen() const noexcept { return m_isOpen; }

        bool UpdateName(_In_ winrt::hstring const& newName) noexcept;

        foundation::Collections::IMapView<winrt::guid, midi2::MidiEndpointConnection> Connections() { return m_connections.GetView(); }

        midi2::MidiEndpointConnection TryCreateEndpointConnection(
            _In_ winrt::hstring const& endpointDeviceId
        ) noexcept;

        midi2::MidiEndpointConnection TryCreateEndpointConnection(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ bool const autoReconnect
        ) noexcept;

        midi2::MidiEndpointConnection TryCreateEndpointConnection(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ bool const autoReconnect,
            _In_ midi2::IMidiEndpointConnectionSettings const& settings
        ) noexcept;


        void DisconnectEndpointConnection(
            _In_ winrt::guid const& endpointConnectionId
        ) noexcept;

        void Close() noexcept;   // via IClosable


        midi2::MidiEndpointConnection TryCreateVirtualDeviceAndConnection(
            _In_ midi2::MidiVirtualEndpointDeviceDefinition const& deviceDefinition
        ) noexcept;


        // internal to the API
        void SetName(_In_ winrt::hstring value) { m_name = value; }

        _Success_(return == true)
        bool InternalStart();

    private:
        // this helps us clean up when/if a client crashes or otherwise
        // doesn't clean up its session before closing
      //  VOID* m_serviceRpcContextHandle{ nullptr };

        bool m_isOpen{ false };
        winrt::guid m_id{};
        winrt::hstring m_name{};

        bool m_useMmcss{ false };
        DWORD m_mmcssTaskId{ 0 };

        // everything in the session needs to use this service abstraction because the
        // context handle is tied to it.
        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction;
        winrt::com_ptr<IMidiSessionTracker> m_sessionTracker;

        // key is the unique guid for the connection
        collections::IMap<winrt::guid, midi2::MidiEndpointConnection>
            m_connections{ winrt::multi_threaded_map<winrt::guid, midi2::MidiEndpointConnection>() };

        // this is a vector instead of a map because there can me more than one
        // open connection for a single endpoint device id
        std::vector<winrt::com_ptr<midi2::implementation::MidiEndpointConnection>>
            m_connectionsForAutoReconnect{ };


        enumeration::DeviceWatcher m_autoReconnectDeviceWatcher{ nullptr };

        void DeviceAddedHandler(_In_ enumeration::DeviceWatcher source, _In_ enumeration::DeviceInformation args);
        void DeviceUpdatedHandler(_In_ enumeration::DeviceWatcher source, _In_ enumeration::DeviceInformationUpdate args);
        void DeviceRemovedHandler(_In_ enumeration::DeviceWatcher source, _In_ enumeration::DeviceInformationUpdate args);

        enumeration::DeviceWatcher::Added_revoker m_autoReconnectDeviceWatcherAddedRevoker;
        enumeration::DeviceWatcher::Updated_revoker m_autoReconnectDeviceWatcherUpdatedRevoker;
        enumeration::DeviceWatcher::Removed_revoker m_autoReconnectDeviceWatcherRemovedRevoker;

        _Success_(return == true)
        bool StartEndpointWatcher() noexcept;

        _Success_(return == true)
        bool StopEndpointWatcher() noexcept;

    };
}
namespace MIDI_CPP_NAMESPACE::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
