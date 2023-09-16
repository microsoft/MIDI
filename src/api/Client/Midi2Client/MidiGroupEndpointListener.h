// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroupEndpointListener.g.h"

#include "string_util.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener>
    {
        MidiGroupEndpointListener() = default;

        winrt::hstring Id() const noexcept { return m_id; }
        void Id(_In_ winrt::hstring const& value) noexcept { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }

        foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ foundation::IInspectable const& value) noexcept { m_tag = value; }

        midi2::IMidiInputConnection InputConnection() const noexcept { return m_inputConnection; }
        void InputConnection(_In_ midi2::IMidiInputConnection const& value) noexcept { m_inputConnection = value; }

        collections::IVector<midi2::MidiGroup> IncludeGroups() { return m_includedGroups; }

        void Initialize();
        void OnEndpointConnectionOpened();
        void Cleanup();


        void PreventFiringMainMessageReceivedEvent(_In_ bool const value) noexcept { m_preventFiringMainMessageReceivedEvent = value; }
        bool PreventFiringMainMessageReceivedEvent() noexcept { return m_preventFiringMainMessageReceivedEvent; }

        void PreventCallingFurtherListeners(_In_ bool const value) noexcept { m_preventCallingFurtherListeners = value; }
        bool PreventCallingFurtherListeners() noexcept { return m_preventCallingFurtherListeners; }

        winrt::event_token MessageReceived(_In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }

        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            if (m_messageReceivedEvent) m_messageReceivedEvent.remove(token);
        }


        void ProcessIncomingMessage(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args, 
            _Out_ bool& skipFurtherListeners, 
            _Out_ bool& skipMainMessageReceivedEvent);


    private:
        winrt::hstring m_id{};
        winrt::hstring m_name{};
        bool m_enabled{ false };
        foundation::IInspectable m_tag{ nullptr };
        midi2::IMidiInputConnection m_inputConnection;

        bool m_preventCallingFurtherListeners{ false };
        bool m_preventFiringMainMessageReceivedEvent{ false };

        collections::IVector<midi2::MidiGroup> m_includedGroups
            { winrt::single_threaded_vector<midi2::MidiGroup>() };

        winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener, implementation::MidiGroupEndpointListener>
    {
    };
}
