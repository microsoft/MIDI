// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiBidirectionalEndpointConnection.g.h"
#include "MidiEndpointConnection.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, Windows::Devices::Midi2::implementation::MidiEndpointConnection, ::IMidiCallback>
    {
        MidiBidirectionalEndpointConnection() = default;

        static hstring GetDeviceSelectorForBidirectional();

//        uint32_t MmcssTaskId() { return _mmcssTaskId; }

        winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;

        // TODO: Consider we may want to make this a view and have add/remove methods in the endpoint
        // to better match the pattern from Session, and reduce the number of activatable classes that
        // folks need to put in their app config files.
        winrt::Windows::Foundation::Collections::IMap<hstring, winrt::Windows::Devices::Midi2::MidiMessageReader> MessageReaders() { return _messageReaders; }
        winrt::Windows::Devices::Midi2::MidiMessageWriter MessageWriter() { return _messageWriter; }

        bool Start();


        // IMidiCallback support -----------------------------------------------------
        IFACEMETHOD(Callback)(PVOID Data, UINT Size, LONGLONG Position) override;

    private:
        DWORD _mmcssTaskId{ 0 };

        winrt::Windows::Foundation::Collections::IMap<hstring, winrt::Windows::Devices::Midi2::MidiMessageReader>
            _messageReaders { winrt::single_threaded_map<hstring, winrt::Windows::Devices::Midi2::MidiMessageReader>() };

        winrt::Windows::Devices::Midi2::MidiMessageWriter _messageWriter = nullptr;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
