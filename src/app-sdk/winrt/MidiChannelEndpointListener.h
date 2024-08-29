// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "ClientPlugins.MidiChannelEndpointListener.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::ClientPlugins::implementation
{
    struct MidiChannelEndpointListener : MidiChannelEndpointListenerT<MidiChannelEndpointListener>
    {
        MidiChannelEndpointListener() = default;

        winrt::guid PluginId() const noexcept { return m_id; }

        winrt::hstring PluginName() const noexcept { return m_name; }
        void PluginName(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }


        foundation::IInspectable PluginTag() const noexcept { return m_tag; }
        void PluginTag(_In_ foundation::IInspectable const& value) { m_tag = value; }

        //midi2::MidiEndpointConnection Connection() const noexcept { return m_endpointConnection; }
        //void Connection(_In_ midi2::MidiEndpointConnection const& value) noexcept { m_endpointConnection = value; }


        midi2::MidiGroup IncludedGroup() const noexcept { return m_includedGroup; }
        void IncludedGroup(
            _In_ midi2::MidiGroup const& value) noexcept { m_includedGroup = value; }

        collections::IVector<midi2::MidiChannel> IncludedChannels() { return m_includedChannels; }


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

        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            if (m_messageReceivedEvent) m_messageReceivedEvent.remove(token);
        }


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


        midi2::MidiGroup m_includedGroup{ nullptr };

        collections::IVector<midi2::MidiChannel>
            m_includedChannels{ winrt::multi_threaded_vector<midi2::MidiChannel>() };

        winrt::event<foundation::TypedEventHandler<midi2::IMidiMessageReceivedEventSource, midi2::MidiMessageReceivedEventArgs>> m_messageReceivedEvent;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ClientPlugins::factory_implementation
{
    struct MidiChannelEndpointListener : MidiChannelEndpointListenerT<MidiChannelEndpointListener, implementation::MidiChannelEndpointListener>
    {
    };
}
