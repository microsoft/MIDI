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
#include "midi_service_interface.h"
#include "MidiMessageReceivedEventArgs.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection,
        IMidiCallback>
    {
        MidiBidirectionalEndpointConnection() = default;
        ~MidiBidirectionalEndpointConnection();

        static hstring GetDeviceSelectorForBidirectional() { return L""; /* TODO*/ }

        //winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageListener> PreFilterListeners();
        //winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> Filters();
        //void FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value);

        uint32_t ReceiveBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive);

        uint32_t SendBuffer(internal::MidiTimestamp timestamp, winrt::Windows::Foundation::IMemoryBuffer const& midiData, uint32_t byteOffset, uint32_t length);
        bool SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump);
        bool SendWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount);



        bool Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection);

 
        STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position) override;
        
        //inline winrt::event_token MessagesReceived(winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Devices::Midi2::IMidiInputConnection, winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
        inline winrt::event_token MessageReceived(winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return _messageReceivedEvent.add(handler);
        }

        inline void MessageReceived(winrt::event_token const& token) noexcept
        {
            _messageReceivedEvent.remove(token);
        }

    private:
        //winrt::event<winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Devices::Midi2::IMidiInputConnection, winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs>> _messagesReceivedEvent;
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs>> _messageReceivedEvent;

        com_ptr<IMidiBiDi> _bidiEndpoint;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
