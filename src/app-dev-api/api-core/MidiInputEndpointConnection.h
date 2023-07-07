// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiInputEndpointConnection.g.h"
#include "MidiEndpointConnection.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiInputEndpointConnection : MidiInputEndpointConnectionT<MidiInputEndpointConnection, Windows::Devices::Midi2::implementation::MidiEndpointConnection, ::IMidiCallback>
    {
        MidiInputEndpointConnection() = default;

//        uint32_t MmcssTaskId() { return _mmcssTaskId; }


        static hstring GetDeviceSelectorForInput();
        winrt::Windows::Foundation::Collections::IMap<hstring, winrt::Windows::Devices::Midi2::MidiMessageReader> MessageReaders();
        winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;


        void Start();

        // Internal constructor to spin this up. Internal because we want everything an app does to go through the session
        MidiInputEndpointConnection(hstring deviceId, bool useMmcss);

        // IMidiCallback support -----------------------------------------------------
        IFACEMETHOD(Callback)(PVOID Data, UINT Size, LONGLONG Position) override;

    private:
        //winrt::Windows::Devices::Midi2::MidiMessageReader _messageReader = nullptr;
        DWORD _mmcssTaskId{ 0 };


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiInputEndpointConnection : MidiInputEndpointConnectionT<MidiInputEndpointConnection, implementation::MidiInputEndpointConnection>
    {
    };
}
