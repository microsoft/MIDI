// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiSession.g.h"

#include <midi_timestamp.h>

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSession : MidiSessionT<MidiSession>
    {
        MidiSession() = default;

        static winrt::Windows::Devices::Midi2::MidiSession CreateNewSession(hstring const& sessionName, winrt::Windows::Devices::Midi2::MidiSessionSettings const& settings);

        hstring Id() { return _id; }
        bool IsOpen() { return _isOpen; }

        winrt::Windows::Foundation::Collections::IMapView<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection> Connections() { return _connections.GetView(); }

        winrt::Windows::Devices::Midi2::MidiEndpointConnection ConnectToEndpoint(hstring const& midiEndpointId, winrt::Windows::Devices::Midi2::MidiEndpointConnectOptions const& options);
        void DisconnectFromEndpoint(hstring const& midiEndpointId);

        uint64_t GetMidiTimestamp() { return ::Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp(); }
        uint64_t GetMidiTimestampFrequency() { return ::Windows::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency(); }


        void Close();   // via IClosable

        ~MidiSession();

        // internal to the API
        void SetIsOpen(bool value) { _isOpen = value; }
        void SetId(hstring value)  { _id = value; }
        void SetName(hstring value) { _name = value; }
        void SetSettings(MidiSessionSettings value) { _settings = value; }

    private:
        bool _isOpen;
        hstring _id;
        hstring _name;
        MidiSessionSettings _settings;

        winrt::Windows::Foundation::Collections::IMap<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>
            _connections{ winrt::single_threaded_map<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>() };



    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiSession : MidiSessionT<MidiSession, implementation::MidiSession>
    {
    };
}
