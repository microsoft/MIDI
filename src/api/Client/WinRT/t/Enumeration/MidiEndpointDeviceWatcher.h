// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiEndpointDeviceWatcher.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointDeviceWatcher : MidiEndpointDeviceWatcherT<MidiEndpointDeviceWatcher>
    {
        MidiEndpointDeviceWatcher() = default;
        ~MidiEndpointDeviceWatcher();

        static midi2enum::MidiEndpointDeviceWatcher Create(_In_ midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;
        static midi2enum::MidiEndpointDeviceWatcher Create() noexcept;

        void Start();
        void Stop();

        enumeration::DeviceWatcherStatus Status();

        winrt::event_token Added(_In_ foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationAddedEventArgs> const& handler);
        void Added(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Removed(_In_ foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationRemovedEventArgs> const& handler);
        void Removed(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Updated(_In_ foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationUpdatedEventArgs> const& handler);
        void Updated(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token EnumerationCompleted(_In_ foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher,foundation::IInspectable> const& handler);
        void EnumerationCompleted(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Stopped(_In_ foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, foundation::IInspectable> const& handler);
        void Stopped(_In_ winrt::event_token const& token) noexcept;


        collections::IMapView<winrt::hstring, midi2enum::MidiEndpointDeviceInformation> EnumeratedEndpointDevices()
            { return m_enumeratedEndpointDevices.GetView(); }


    private:
        void InternalInitialize(
            _In_ midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters,
            _In_ enumeration::DeviceWatcher const& baseWatcher);


        // internal event handlers. We filter and then forward
        void OnDeviceAdded(
            _In_ enumeration::DeviceWatcher source, 
            _In_ enumeration::DeviceInformation args);

        void OnDeviceUpdated(
            _In_ enumeration::DeviceWatcher source,
            _In_ enumeration::DeviceInformationUpdate args);

        void OnDeviceRemoved(
            _In_ enumeration::DeviceWatcher source,
            _In_ enumeration::DeviceInformationUpdate args);

        void OnEnumerationCompleted(
            _In_ enumeration::DeviceWatcher source,
            _In_ foundation::IInspectable args);

        void OnStopped(
            _In_ enumeration::DeviceWatcher source,
            _In_ foundation::IInspectable args);


        midi2enum::MidiEndpointDeviceInformationFilters m_endpointFilter{ midi2enum::MidiEndpointDeviceInformationFilters::AllStandardEndpoints };


        winrt::event<foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationAddedEventArgs>> m_deviceAddedEvent;
        winrt::event<foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationUpdatedEventArgs>> m_deviceUpdatedEvent;
        winrt::event<foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, midi2enum::MidiEndpointDeviceInformationRemovedEventArgs>> m_deviceRemovedEvent;

        winrt::event<foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, foundation::IInspectable>> m_enumerationCompletedEvent;
        winrt::event<foundation::TypedEventHandler<midi2enum::MidiEndpointDeviceWatcher, foundation::IInspectable>> m_stoppedEvent;


        winrt::event_token m_deviceAddedEventRevokeToken;
        winrt::event_token m_deviceUpdatedEventRevokeToken;
        winrt::event_token m_deviceRemovedEventRevokeToken;
        winrt::event_token m_enumerationCompletedEventRevokeToken;
        winrt::event_token m_stoppedEventRevokeToken;

        collections::IMap<winrt::hstring, midi2enum::MidiEndpointDeviceInformation> m_enumeratedEndpointDevices =
            winrt::multi_threaded_map<winrt::hstring, midi2enum::MidiEndpointDeviceInformation>();

        enumeration::DeviceWatcher m_watcher{ nullptr };

    };
}

namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiEndpointDeviceWatcher : MidiEndpointDeviceWatcherT<MidiEndpointDeviceWatcher, implementation::MidiEndpointDeviceWatcher>
    {
    };
}
