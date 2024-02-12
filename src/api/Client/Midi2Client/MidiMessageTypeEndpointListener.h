// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageTypeEndpointListener.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageTypeEndpointListener : MidiMessageTypeEndpointListenerT<MidiMessageTypeEndpointListener>
    {
        MidiMessageTypeEndpointListener() = default;

        winrt::guid Id() const noexcept { return m_id; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ hstring const& value) { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) { m_enabled = value; }

        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable const& value) { m_tag = value; }

        //midi2::MidiEndpointConnection Connection() const noexcept { return m_endpointConnection; }
        //void Connection(_In_ midi2::MidiEndpointConnection const& value) noexcept { m_endpointConnection = value; }


        collections::IVector<midi2::MidiMessageType> IncludeMessageTypes() { return m_includedMessageTypes; }

        winrt::event_token MessageReceived(
            _In_ foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource,
            _In_ midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }

        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            if (m_messageReceivedEvent) m_messageReceivedEvent.remove(token);
        }

        void PreventFiringMainMessageReceivedEvent(_In_ bool const value) noexcept { m_preventFiringMainMessageReceivedEvent = value; }
        bool PreventFiringMainMessageReceivedEvent() noexcept { return m_preventFiringMainMessageReceivedEvent; }

        void PreventCallingFurtherListeners(_In_ bool const value) noexcept { m_preventCallingFurtherListeners = value; }
        bool PreventCallingFurtherListeners() noexcept { return m_preventCallingFurtherListeners; }


        void Initialize(_In_ midi2::IMidiEndpointConnectionSource const& endpointConnection);
        void OnEndpointConnectionOpened();
        void Cleanup();

        void ProcessIncomingMessage(
            _In_ midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent);


    private:
        winrt::guid m_id{ foundation::GuidHelper::CreateNewGuid() };                         // plugin id
        winrt::hstring m_name{};
        bool m_enabled{ true };
        foundation::IInspectable m_tag{ nullptr };
        midi2::MidiEndpointConnection m_endpointConnection{ nullptr };

        bool m_preventCallingFurtherListeners{ false };
        bool m_preventFiringMainMessageReceivedEvent{ false };

        foundation::Collections::IVector<midi2::MidiMessageType>
            m_includedMessageTypes{ winrt::multi_threaded_vector<midi2::MidiMessageType>() };

        winrt::event<foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageTypeEndpointListener : MidiMessageTypeEndpointListenerT<MidiMessageTypeEndpointListener, implementation::MidiMessageTypeEndpointListener>
    {
    };
}
