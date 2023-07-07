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
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, ::IMidiCallback>
    {
        MidiEndpointConnection() = default;

        static hstring GetDeviceSelector();
        hstring DeviceId();

        winrt::Windows::Devices::Midi2::MidiMessageReader GetMessageReader();
        winrt::Windows::Devices::Midi2::MidiMessageWriter GetMessageWriter();
        winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;


        // Internal constructor to spin this up. Internal because we want everything an app does to go through the session
        MidiEndpointConnection(hstring deviceId, bool useMmcss);

        // internal method to start listening for new messages
        void Start();
        
        void SetOptions(MidiEndpointConnectOptions value) { _options = value; }

        // IMidiCallback support -----------------------------------------------------
        IFACEMETHOD(Callback)(PVOID Data, UINT Size, LONGLONG Position) override;


    private:
        hstring _deviceId;
        
        bool _useMmcss = true;
        DWORD _mmcssTaskId{ 0 };

        MidiEndpointConnectOptions _options;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
