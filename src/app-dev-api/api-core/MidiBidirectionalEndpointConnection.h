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
#include "InternalMidiDeviceConnection.h"
#include "midi_service_interface.h";
#include "MidiMessagesReceivedEventArgs.h";

#include "MidiUmp32.h";
#include "MidiUmp64.h";
#include "MidiUmp96.h";
#include "MidiUmp128.h";

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection,
        IMidiCallback>
    {
        MidiBidirectionalEndpointConnection() = default;

        static hstring GetDeviceSelectorForBidirectional() { return L""; /* TODO*/ }

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> Filters();
        winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy FilterStrategy();
        void FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value);

        uint32_t ReceiveBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive);

        uint32_t SendBuffer(internal::MidiTimestamp timestamp, winrt::Windows::Foundation::IMemoryBuffer const& midiData, uint32_t byteOffset, uint32_t length);
        void SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump);


        bool Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection);

 
        STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position) override;
        
        //inline winrt::event_token MessagesReceived(winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Devices::Midi2::IMidiInputConnection, winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
        inline winrt::event_token MessagesReceived(winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
        {
            return _messagesReceivedEvent.add(handler);
        }

        inline void MessagesReceived(winrt::event_token const& token) noexcept
        {
            _messagesReceivedEvent.remove(token);
        }

    private:
        //winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Devices::Midi2::IMidiInputConnection, winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs>> _messagesReceivedEvent;
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs>> _messagesReceivedEvent;

        com_ptr<IMidiBiDi> _bidiEndpoint;

        bool _tempMessagesReceivedFlag = false;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
