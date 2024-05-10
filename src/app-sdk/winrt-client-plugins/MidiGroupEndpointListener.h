// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroupEndpointListener.g.h"


namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener>
    {
        MidiGroupEndpointListener() = default;

        winrt::guid Id() const noexcept { return m_id; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }

        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable const& value) noexcept { m_tag = value; }

        //midi2::MidiEndpointConnection Connection() const noexcept { return m_endpointConnection; }
        //void Connection(_In_ midi2::MidiEndpointConnection const& value) noexcept { m_endpointConnection = value; }

        collections::IVector<midi2::MidiGroup> IncludedGroups() { return m_includedGroups; }

        void Initialize(_In_ midi2::IMidiEndpointConnectionSource const& endpointConnection);
        void OnEndpointConnectionOpened();
        void Cleanup();


        void PreventFiringMainMessageReceivedEvent(_In_ bool const value) noexcept { m_preventFiringMainMessageReceivedEvent = value; }
        bool PreventFiringMainMessageReceivedEvent() noexcept { return m_preventFiringMainMessageReceivedEvent; }

        void PreventCallingFurtherListeners(_In_ bool const value) noexcept { m_preventCallingFurtherListeners = value; }
        bool PreventCallingFurtherListeners() noexcept { return m_preventCallingFurtherListeners; }

        winrt::event_token MessageReceived(
            _In_ foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource,
            _In_ midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }

        void MessageReceived(
            _In_ winrt::event_token const& token) noexcept
        {
            if (m_messageReceivedEvent) m_messageReceivedEvent.remove(token);
        }


        void ProcessIncomingMessage(
            _In_ MIDI_CPP_NAMESPACE::MidiMessageReceivedEventArgs const& args, 
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

        collections::IVector<midi2::MidiGroup> m_includedGroups
            { winrt::multi_threaded_vector<midi2::MidiGroup>() };

        winrt::event<foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;

    };
}
namespace MIDI_CPP_NAMESPACE::factory_implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener, implementation::MidiGroupEndpointListener>
    {
    };
}
