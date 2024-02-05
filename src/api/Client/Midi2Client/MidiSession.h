// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"
//#include "MidiVirtualDeviceManager.h"

#include "midi_service_interface.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;
        ~MidiSession();

        static winrt::Windows::Devices::Midi2::MidiSession CreateSession(
            _In_ hstring const& sessionName, 
            _In_ winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings
        ) noexcept;

        static winrt::Windows::Devices::Midi2::MidiSession CreateSession(
            _In_ hstring const& sessionName
        ) noexcept;

        winrt::guid Id() const noexcept { return m_id; }
        winrt::hstring Name() const noexcept { return m_name; }
        bool IsOpen() const noexcept { return m_isOpen; }

        midi2::MidiSessionSettings Settings() const noexcept { return m_settings; }


        foundation::Collections::IMapView<winrt::guid, midi2::MidiEndpointConnection> Connections() { return m_connections.GetView(); }

        winrt::Windows::Devices::Midi2::MidiEndpointConnection CreateEndpointConnection(
            _In_ winrt::hstring const& endpointDeviceId
        ) noexcept;

        winrt::Windows::Devices::Midi2::MidiEndpointConnection CreateEndpointConnection(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ midi2::MidiEndpointConnectionOptions const& options
        ) noexcept;

        winrt::Windows::Devices::Midi2::MidiEndpointConnection CreateEndpointConnection(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ midi2::MidiEndpointConnectionOptions const& options,
            _In_ midi2::IMidiEndpointDefinedConnectionSettings const& settings
        ) noexcept;


        void DisconnectEndpointConnection(
            _In_ winrt::guid const& endpointConnectionId
        ) noexcept;

        void Close() noexcept;   // via IClosable

//        midi2::MidiVirtualDeviceManager VirtualDeviceManager() const noexcept { return m_virtualDeviceManager; }


        midi2::MidiEndpointConnection CreateVirtualDeviceAndConnection(
            _In_ midi2::MidiVirtualEndpointDeviceDefinition const& deviceDefinition
        ) noexcept;


        // internal to the API
        void SetName(_In_ winrt::hstring value) { m_name = value; }
        void SetSettings(_In_ midi2::MidiSessionSettings value) { m_settings = value; }

        bool InternalStart();

    private:
        bool m_isOpen{ false };
        winrt::guid m_id{};
        winrt::hstring m_name{};
        midi2::MidiSessionSettings m_settings{ nullptr };
 //       midi2::MidiVirtualDeviceManager m_virtualDeviceManager{ nullptr };

        bool m_useMmcss{ true };
        DWORD m_mmcssTaskId{ 0 };

        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction;
        winrt::com_ptr<IMidiSessionTracker> m_sessionTracker;

        collections::IMap<winrt::guid, midi2::MidiEndpointConnection>
            m_connections{ winrt::single_threaded_map<winrt::guid, midi2::MidiEndpointConnection>() };


        //winrt::hstring NormalizeDeviceId(_In_ const winrt::hstring& endpointDeviceId);

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
