// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"

#include "midi_service_interface.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;
        ~MidiSession();

        static winrt::Windows::Devices::Midi2::MidiSession CreateSession(hstring const& sessionName, winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings);

        hstring Id() { return m_id; }
        hstring Name() { return m_name; }
        bool IsOpen() { return m_isOpen; }
        winrt::Windows::Devices::Midi2::MidiSessionSettings Settings() { return m_settings; }


        winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection> Connections() { return m_connections.GetView(); }

        winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection ConnectOutputEndpoint(
            hstring const& deviceId, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings);

        winrt::Windows::Devices::Midi2::MidiInputEndpointConnection ConnectInputEndpoint(
            hstring const& deviceId, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings);

        winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection ConnectBidirectionalEndpoint(
            hstring const& deviceId, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings);

        void DisconnectEndpointConnection(hstring const& endpointConnectionId);

        void Close();   // via IClosable


        // internal to the API
        void SetName(hstring value) { m_name = value; }
        void SetSettings(MidiSessionSettings value) { m_settings = value; }

        bool InternalStart();

    private:
        bool m_isOpen{ false };
        hstring m_id{};
        hstring m_name{};
        MidiSessionSettings m_settings{ nullptr };

        bool m_useMmcss{ true };
        DWORD m_mmcssTaskId{ 0 };

        winrt::impl::com_ref<IMidiAbstraction> m_serviceAbstraction;

        winrt::Windows::Foundation::Collections::IMap<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>
            m_connections{ winrt::single_threaded_map<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>() };


        hstring NormalizeDeviceId(const hstring& deviceId);

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
