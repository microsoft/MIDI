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
#include "midi_service_interface.h"

#include "MidiMessageReceivedEventArgs.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

#include "InternalMidiMessageReceiverHelper.h"
#include "InternalMidiMessageSenderHelper.h"

#include <pch.h>

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection,
        IMidiCallback>
    {
        MidiBidirectionalEndpointConnection() = default;
        ~MidiBidirectionalEndpointConnection();

        static hstring GetDeviceSelectorForBidirectional() { return L""; /* TODO*/ }

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiEndpointMessageListener> MessageListeners() { return m_messageListeners; }


        bool SendUmp(_In_ winrt::Windows::Devices::Midi2::IMidiUmp const& ump);

        bool SendUmpWords(_In_ internal::MidiTimestamp const timestamp, _In_ uint32_t const word0);
        bool SendUmpWords(_In_ internal::MidiTimestamp const timestamp, _In_ uint32_t const word0, _In_ uint32_t const word1);
        bool SendUmpWords(_In_ internal::MidiTimestamp const timestamp, _In_ uint32_t const word0, _In_ uint32_t const word1, _In_ uint32_t const word2);
        bool SendUmpWords(_In_ internal::MidiTimestamp const timestamp, _In_ uint32_t const word0, _In_ uint32_t const word1, _In_ uint32_t const word2, _In_ uint32_t const word3);

        bool SendUmpWordArray(_In_ internal::MidiTimestamp const timestamp, _In_ array_view<uint32_t const> words, _In_ uint32_t const wordCount);

        bool SendUmpBuffer(_In_ internal::MidiTimestamp timestamp, _In_ winrt::Windows::Foundation::IMemoryBuffer const& buffer, _In_ uint32_t byteOffset, _In_ uint32_t byteLength);


        STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position) override;
        
        winrt::event_token MessageReceived(winrt::Windows::Foundation::TypedEventHandler<_In_ IInspectable, _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }

        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            m_messageReceivedEvent.remove(token);
        }


        bool InternalStart(_In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction);


    private:
        com_ptr<IMidiBiDi> m_endpointInterface;
        internal::InternalMidiMessageReceiverHelper m_messageReceiverHelper;
        internal::InternalMidiMessageSenderHelper<IMidiBiDi> m_messageSenderHelper;

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiEndpointMessageListener>
            m_messageListeners{ winrt::single_threaded_vector<winrt::Windows::Devices::Midi2::IMidiEndpointMessageListener>() };


        winrt::event<winrt::Windows::Foundation::TypedEventHandler<IInspectable, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;


        bool ActivateMidiStream(_In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction, _In_ const IID& iid, _Out_ void** iface);


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
