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

#include "midi_service_interface.h"

#include "MidiMessageReceivedEventArgs.h"

#define MIDI_ENDPOINT_DEVICE_AQS_FILTER L"System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"

#define MIDI_MAX_ALLOWED_SCHEDULER_SECONDS_INTO_FUTURE 20

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, IMidiCallback> 
    {
        MidiEndpointConnection() { m_maxAllowedTimestampOffset = ::Windows::Devices::Midi2::Internal::Shared::GetMidiTimestampFrequency() * MIDI_MAX_ALLOWED_SCHEDULER_SECONDS_INTO_FUTURE; }
        ~MidiEndpointConnection();

        static hstring GetDeviceSelector() noexcept { return MIDI_ENDPOINT_DEVICE_AQS_FILTER; }


        static bool SendMessageSucceeded(_In_ midi2::MidiSendMessageResult const sendResult) { return (sendResult & midi2::MidiSendMessageResult::Succeeded) == midi2::MidiSendMessageResult::Succeeded; }
        static bool SendMessageFailed(_In_ midi2::MidiSendMessageResult const sendResult) { return (sendResult & midi2::MidiSendMessageResult::Failed) == midi2::MidiSendMessageResult::Failed; }



        winrt::guid ConnectionId() const noexcept { return m_connectionId; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }


        bool IsOpen() const noexcept { return m_isOpen; }
        midi2::IMidiEndpointConnectionSettings Settings() const noexcept { return m_settings; }

        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable value) noexcept { m_tag = value; }


        bool InternalInitialize(
            _In_ winrt::guid sessionId,
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ winrt::guid const connectionId,
            _In_ winrt::hstring const endpointDeviceId);


        midi2::MidiSendMessageResult SendMessagePacket(
            _In_ midi2::IMidiUniversalPacket const& ump) noexcept;


        midi2::MidiSendMessageResult SendMessageStruct(
            _In_ internal::MidiTimestamp timestamp,
            _In_ midi2::MidiMessageStruct const& message,
            _In_ uint8_t wordCount) noexcept;


        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0) noexcept;

        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1) noexcept;

        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2) noexcept;

        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2,
            _In_ uint32_t const word3) noexcept;

        midi2::MidiSendMessageResult SendMessageWordArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ winrt::array_view<uint32_t const> words,
            _In_ uint32_t const startIndex,
            _In_ uint8_t const wordCount) noexcept;

        midi2::MidiSendMessageResult SendMessageBuffer(
            _In_ internal::MidiTimestamp timestamp,
            _In_ foundation::IMemoryBuffer const& buffer,
            _In_ uint32_t byteOffset,
            _In_ uint8_t byteLength) noexcept;


        midi2::MidiSendMessageResult SendMessagesWordList(
            _In_ internal::MidiTimestamp timestamp, 
            _In_ collections::IVectorView<uint32_t> const& words) noexcept;

        midi2::MidiSendMessageResult SendMessagesWordArray(
            _In_ internal::MidiTimestamp timestamp, 
            _In_ winrt::array_view<uint32_t const> words) noexcept;


        midi2::MidiSendMessageResult SendMessagePacketList(
            _In_ collections::IVectorView<IMidiUniversalPacket> const& messages) noexcept;


        _Success_(return == true)
        bool Open();

        void InternalClose();

        STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG timestamp, _In_ LONGLONG) override;

        winrt::event_token MessageReceived(_In_ foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }

        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            if (m_messageReceivedEvent) m_messageReceivedEvent.remove(token);
        }


        winrt::Windows::Foundation::Collections::IVectorView<midi2::IMidiEndpointMessageProcessingPlugin> MessageProcessingPlugins() const noexcept
        {
            return m_messageProcessingPlugins.GetView();
        }

        void AddMessageProcessingPlugin(_In_ midi2::IMidiEndpointMessageProcessingPlugin const& plugin);

        void RemoveMessageProcessingPlugin(_In_ winrt::guid id);

    private:
        uint64_t m_maxAllowedTimestampOffset{};


        winrt::guid m_sessionId{};
        winrt::guid m_connectionId{};
        winrt::hstring m_endpointDeviceId{};
        winrt::Windows::Foundation::IInspectable m_tag{ nullptr };

        bool m_isOpen{ false };
        bool m_closeHasBeenCalled{ false };

        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction{ nullptr };
        winrt::com_ptr<IMidiBiDi> m_endpointAbstraction{ nullptr };


        winrt::event<foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;

        //midi2::MidiEndpointConnectionOptions m_options;

        foundation::Collections::IVector<midi2::IMidiEndpointMessageProcessingPlugin>
            m_messageProcessingPlugins{ winrt::multi_threaded_vector<midi2::IMidiEndpointMessageProcessingPlugin>() };

        midi2::IMidiEndpointConnectionSettings m_settings{ nullptr };


        bool ValidateUmp(_In_ uint32_t word0, _In_ uint8_t wordCount) noexcept;


        _Success_(return == true)
        midi2::MidiSendMessageResult SendUmpInternal(
            _In_ winrt::com_ptr<IMidiBiDi> endpoint,
            _In_ midi2::IMidiUniversalPacket const& ump);

        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageRaw(
            _In_ winrt::com_ptr<IMidiBiDi> endpoint,
            _In_ void* data,
            _In_ uint8_t sizeInBytes,
            _In_ internal::MidiTimestamp timestamp);

        _Success_(return != nullptr)
        void* GetUmpDataPointer(
            _In_ midi2::IMidiUniversalPacket const& ump,
            _Out_ uint8_t & dataSizeOut);

        _Success_(return == true)
        bool ActivateMidiStream(
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ const IID & iid,
            _Out_ void** iface) noexcept;

        void InitializePlugins() noexcept;
        void CallOnConnectionOpenedOnPlugins() noexcept;
        void CleanupPlugins() noexcept;


        midi2::MidiSendMessageResult SendMessageResultFromHRESULT(_In_ HRESULT hr);


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
