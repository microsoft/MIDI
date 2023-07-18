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

#include "InternalMidiMessageReceiverHelper.h"
#include "InternalMidiMessageSenderHelper.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection,
        IMidiCallback>
    {
        MidiBidirectionalEndpointConnection() = default;
        ~MidiBidirectionalEndpointConnection();

        static hstring GetDeviceSelectorForBidirectional() { return L""; /* TODO*/ }

        bool SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump);
        bool SendUmpWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount);
        bool SendUmpBuffer(uint64_t timestamp, winrt::Windows::Foundation::IMemoryBuffer const& buffer);
        bool SendUmpBuffer(uint64_t timestamp, winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffset, uint32_t byteLength);


        STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position) override;
        
        winrt::event_token MessageReceived(winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return _messageReceivedEvent.add(handler);
        }

        void MessageReceived(winrt::event_token const& token) noexcept
        {
            _messageReceivedEvent.remove(token);
        }


        bool InternalStart(winrt::com_ptr<IMidiAbstraction> serviceAbstraction);


    private:
        com_ptr<IMidiBiDi> _endpointInterface;
        internal::InternalMidiMessageReceiverHelper _messageReceiverHelper;
        internal::InternalMidiMessageSenderHelper<IMidiBiDi> _messageSenderHelper;

        winrt::event<winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs>> _messageReceivedEvent;


        bool ActivateMidiStream(winrt::com_ptr<IMidiAbstraction> serviceAbstraction, const IID& iid, void** iface);


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
