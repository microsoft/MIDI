// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointConnection.g.h"

#include "midi_service_interface.h"
//#include <functional>

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection>
    {
        MidiEndpointConnection() = default;

        static hstring GetDeviceSelector();

        hstring Id() { return _id; }
        hstring DeviceId() { return _deviceId; }
        bool IsConnected() { return _isConnected; }
        IMidiEndpointConnectionSettings Settings() { return _settings; }

        hstring Tag() { return _tag; }
        void Tag(hstring value) { _tag = value; }

        void SetSettings(IMidiEndpointConnectionSettings value) { _settings = value; }
        void SetDeviceId(hstring value) { _deviceId = value; }

    protected:
        hstring _id{};
        hstring _deviceId{};
        hstring _tag{};
        
        bool _isConnected{ false };

        IMidiEndpointConnectionSettings _settings{ nullptr };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
