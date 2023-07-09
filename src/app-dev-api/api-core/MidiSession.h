// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"

#include "midi_service_interface.h";
#include "InternalMidiDeviceConnection.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Windows::Devices::Midi2::MidiSession CreateSession(hstring const& sessionName, winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings);

        hstring Id() { return _id; }
        hstring Name() { return _name; }
        bool IsOpen() { return _isOpen; }
        winrt::Windows::Devices::Midi2::MidiSessionSettings Settings() { return _settings; }


        winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection> Connections() { return _connections.GetView(); }

        winrt::Windows::Devices::Midi2::MidiOutputEndpointConnection ConnectOutputEndpoint(hstring const& deviceId, hstring const& tag, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings);
        winrt::Windows::Devices::Midi2::MidiInputEndpointConnection ConnectInputEndpoint(hstring const& deviceId, winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> const& incomingMessageFilters, winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& messageFilterStrategy, hstring const& tag, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings);
        winrt::Windows::Devices::Midi2::MidiBidirectionalEndpointConnection ConnectBidirectionalEndpoint(hstring const& deviceId, winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> const& incomingMessageFilters, winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& messageFilterStrategy, hstring const& tag /*, winrt::Windows::Devices::Midi2::IMidiEndpointConnectionSettings const& settings */ );
        void DisconnectEndpointConnectionInstance(hstring const& endpointConnectionId);
        void DisconnectAllConnectionsForEndpoint(hstring const& deviceId);

        void Close();   // via IClosable

        ~MidiSession();

        // internal to the API
        void SetName(hstring value) { _name = value; }
        void SetSettings(MidiSessionSettings value) { _settings = value; }

        bool Start();

    private:
        bool _isOpen;
        hstring _id;
        hstring _name;
        MidiSessionSettings _settings;

        bool _useMmcss{ true };
        DWORD _mmcssTaskId{ 0 };

        std::unordered_map<std::string, std::shared_ptr<internal::InternalMidiDeviceConnection>> _internalDeviceConnections{};


        winrt::impl::com_ref<IMidiAbstraction> _serviceAbstraction;

        winrt::Windows::Foundation::Collections::IMap<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>
            _connections{ winrt::single_threaded_map<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>() };


        hstring NormalizeDeviceId(const hstring& deviceId);
        bool ActivateMidiStream(const IID& iid, void** iface);

        template<class TInterface>
        std::shared_ptr<internal::InternalMidiDeviceConnection> GetOrCreateAndInitializeDeviceConnection(std::string normalizedDeviceId, winrt::com_ptr<TInterface> iface);

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
