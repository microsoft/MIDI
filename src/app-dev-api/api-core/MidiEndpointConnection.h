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

        hstring DeviceId() { return _deviceId; }
        bool IsConnected() { return _isConnected; }


        void SetOptions(MidiEndpointConnectOptions value) { _options = value; }
        void SetUseMmcss(bool value) { _useMmcss = value; }
        void SetDeviceId(hstring value) { _deviceId = value; }

    protected:
        hstring _deviceId;
        
        bool _useMmcss{ true };
        DWORD _mmcssTaskId{ 0 };

        bool _isConnected{ false };

        MidiEndpointConnectOptions _options{ nullptr };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
