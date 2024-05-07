// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

#include "MidiEndpointConnection.g.h"

#define MIDI_ENDPOINT_DEVICE_AQS_FILTER L"System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"

#define MIDI_MAX_ALLOWED_SCHEDULER_SECONDS_INTO_FUTURE 20

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, IMidiCallback> 
    {
        MidiEndpointConnection() { m_maxAllowedTimestampOffset = internal::GetMidiTimestampFrequency() * MIDI_MAX_ALLOWED_SCHEDULER_SECONDS_INTO_FUTURE; }
        ~MidiEndpointConnection();

        static hstring GetDeviceSelector() noexcept { return MIDI_ENDPOINT_DEVICE_AQS_FILTER; }


        static bool SendMessageSucceeded(_In_ midi2::MidiSendMessageResults const sendResults) { return (sendResults & midi2::MidiSendMessageResults::Succeeded) == midi2::MidiSendMessageResults::Succeeded; }
        static bool SendMessageFailed(_In_ midi2::MidiSendMessageResults const sendResults) { return (sendResults & midi2::MidiSendMessageResults::Failed) == midi2::MidiSendMessageResults::Failed; }

        winrt::guid ConnectionId() const noexcept { return m_connectionId; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }

        bool IsOpen() const noexcept { return m_isOpen; }
        bool IsAutoReconnectEnabled() const noexcept { return m_autoReconnect; }

        midi2::IMidiEndpointConnectionSettings Settings() const noexcept { return m_settings; }

        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable value) noexcept { m_tag = value; }


        midi2::MidiSendMessageResults SendSingleMessagePacket(
            _In_ midi2::IMidiUniversalPacket const& ump) noexcept;


        midi2::MidiSendMessageResults SendSingleMessageStruct(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t wordCount,
            _In_ midi2::MidiMessageStruct const& message
        ) noexcept;


        midi2::MidiSendMessageResults SendSingleMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0) noexcept;

        midi2::MidiSendMessageResults SendSingleMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1) noexcept;

        midi2::MidiSendMessageResults SendSingleMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2) noexcept;

        midi2::MidiSendMessageResults SendSingleMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2,
            _In_ uint32_t const word3) noexcept;

        midi2::MidiSendMessageResults SendSingleMessageWordArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const startIndex,
            _In_ uint8_t const wordCount,
            _In_ winrt::array_view<uint32_t const> words
            ) noexcept;

        midi2::MidiSendMessageResults SendSingleMessageBuffer(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const byteOffset,
            _In_ uint8_t const byteCount,
            _In_ foundation::IMemoryBuffer const& buffer
            ) noexcept;


        midi2::MidiSendMessageResults SendMultipleMessagesWordList(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ collections::IIterable<uint32_t> const& words) noexcept;

        midi2::MidiSendMessageResults SendMultipleMessagesWordArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const startIndex,
            _In_ uint32_t const wordCount,
            _In_ winrt::array_view<uint32_t const> words) noexcept;


        midi2::MidiSendMessageResults SendMultipleMessagesPacketList(
            _In_ collections::IIterable<IMidiUniversalPacket> const& messages) noexcept;

        //midi2::MidiSendMessageResult SendMultipleMessagesPacketArray(
        //    _In_ winrt::array_view<IMidiUniversalPacket> messages) noexcept;


        midi2::MidiSendMessageResults SendMultipleMessagesStructList(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ collections::IIterable<MidiMessageStruct> const& messages) noexcept;

        midi2::MidiSendMessageResults SendMultipleMessagesStructArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const startIndex,
            _In_ uint32_t const messageCount,
            _In_ winrt::array_view<MidiMessageStruct const> messages) noexcept;

        midi2::MidiSendMessageResults SendMultipleMessagesBuffer(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const byteOffset,
            _In_ uint32_t const byteCount,
            _In_ foundation::IMemoryBuffer const& buffer
            ) noexcept;


        STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG timestamp, _In_ LONGLONG) override;

        winrt::event_token MessageReceived(_In_ foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }
        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            if (m_messageReceivedEvent) m_messageReceivedEvent.remove(token);
        }


        winrt::event_token EndpointDeviceDisconnected(_In_ foundation::TypedEventHandler<midi2::IMidiEndpointConnectionSource, foundation::IInspectable> const& handler)
        {
            return m_endpointDeviceDisconnectedEvent.add(handler);
        }
        void EndpointDeviceDisconnected(_In_ winrt::event_token const& token) noexcept
        {
            if (m_endpointDeviceDisconnectedEvent) m_endpointDeviceDisconnectedEvent.remove(token);
        }


        winrt::event_token EndpointDeviceReconnected(_In_ foundation::TypedEventHandler<midi2::IMidiEndpointConnectionSource, foundation::IInspectable> const& handler)
        {
            return m_endpointDeviceReconnectedEvent.add(handler);
        }
        void EndpointDeviceReconnected(_In_ winrt::event_token const& token) noexcept
        {
            if (m_endpointDeviceReconnectedEvent) m_endpointDeviceReconnectedEvent.remove(token);
        }



        collections::IVectorView<midi2::IMidiEndpointMessageProcessingPlugin> MessageProcessingPlugins() const noexcept
        {
            return m_messageProcessingPlugins.GetView();
        }

        void AddMessageProcessingPlugin(_In_ midi2::IMidiEndpointMessageProcessingPlugin const& plugin);
        void RemoveMessageProcessingPlugin(_In_ winrt::guid id);

        // public open method
        _Success_(return == true)
        bool Open();



        _Success_(return == true)
        bool InternalInitialize(
            _In_ winrt::guid sessionId,
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ winrt::guid const connectionId,
            _In_ winrt::hstring const endpointDeviceId,
            _In_ midi2::IMidiEndpointConnectionSettings connectionSettings,
            _In_ bool autoReconnect
        );


        void InternalClose();

        void InternalOnDeviceDisconnect();
        void InternalOnDeviceReconnect();

        bool InternalWasAlreadyOpened() { return m_wasAlreadyOpened; }

    private:
        midi2::IMidiEndpointConnectionSettings m_connectionSettings;
        bool m_autoReconnect{ false };


        uint64_t m_maxAllowedTimestampOffset{};


        winrt::guid m_sessionId{};
        winrt::guid m_connectionId{};
        winrt::hstring m_endpointDeviceId{};
        winrt::Windows::Foundation::IInspectable m_tag{ nullptr };

        bool m_isOpen{ false };
        bool m_wasAlreadyOpened { false };      // true if this was initially opened. This is to avoid reopen events when first opened
        bool m_closeHasBeenCalled{ false };

        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction{ nullptr };
        winrt::com_ptr<IMidiBiDi> m_endpointAbstraction{ nullptr };


        winrt::event<foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;

        winrt::event<foundation::TypedEventHandler<midi2::IMidiEndpointConnectionSource, foundation::IInspectable>> m_endpointDeviceDisconnectedEvent;
        winrt::event<foundation::TypedEventHandler<midi2::IMidiEndpointConnectionSource, foundation::IInspectable>> m_endpointDeviceReconnectedEvent;

        //midi2::MidiEndpointConnectionOptions m_options;

        foundation::Collections::IVector<midi2::IMidiEndpointMessageProcessingPlugin>
            m_messageProcessingPlugins{ winrt::multi_threaded_vector<midi2::IMidiEndpointMessageProcessingPlugin>() };

        midi2::IMidiEndpointConnectionSettings m_settings{ nullptr };


        bool ValidateUmp(_In_ uint32_t word0, _In_ uint8_t wordCount) noexcept;


        _Success_(return == true)
        midi2::MidiSendMessageResults SendUmpInternal(
            _In_ winrt::com_ptr<IMidiBiDi> endpoint,
            _In_ midi2::IMidiUniversalPacket const& ump);

        _Success_(return == true)
        midi2::MidiSendMessageResults SendMessageRaw(
            _In_ winrt::com_ptr<IMidiBiDi> endpoint,
            _In_ void* data,
            _In_ uint8_t sizeInBytes,
            _In_ internal::MidiTimestamp timestamp);

        _Success_(return != nullptr)
        void* GetUmpDataPointer(
            _In_ midi2::IMidiUniversalPacket const& ump,
            _Out_ uint8_t & dataSizeOut);
        
        midi2::MidiSendMessageResults SendMessageResultFromHRESULT(_In_ HRESULT hr);

        _Success_(return == true)
        bool ActivateMidiStream() noexcept;

        void InitializePlugins() noexcept;
        void CallOnConnectionOpenedOnPlugins() noexcept;
        void CleanupPlugins() noexcept;

        _Success_(return == true)
        bool InternalOpen();
        
        _Success_(return == true)
        bool DeactivateMidiStream(_In_ bool const force);





    };
}
namespace MIDI_CPP_NAMESPACE::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
