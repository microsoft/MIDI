// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiEndpointDeviceWatcher.g.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceWatcher : MidiEndpointDeviceWatcherT<MidiEndpointDeviceWatcher>
    {
        MidiEndpointDeviceWatcher() = default;
        ~MidiEndpointDeviceWatcher();

        static midi2::MidiEndpointDeviceWatcher Create(_In_ midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;
        static midi2::MidiEndpointDeviceWatcher Create() noexcept;

        void Start();
        void Stop();

        winrt::Windows::Devices::Enumeration::DeviceWatcherStatus Status();

        winrt::event_token Added(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, midi2::MidiEndpointDeviceInformationAddedEventArgs> const& handler)
        {
            return m_deviceAddedEvent.add(handler);
        }
        void Added(_In_ winrt::event_token const& token) noexcept
        {
            if (m_deviceAddedEvent) m_deviceAddedEvent.remove(token);
        }

        winrt::event_token Removed(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, midi2::MidiEndpointDeviceInformationRemovedEventArgs> const& handler)
        {
            return m_deviceRemovedEvent.add(handler);
        }
        void Removed(_In_ winrt::event_token const& token) noexcept
        {
            if (m_deviceRemovedEvent) m_deviceRemovedEvent.remove(token);
        }

        winrt::event_token Updated(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, midi2::MidiEndpointDeviceInformationUpdatedEventArgs> const& handler)
        {
            return m_deviceUpdatedEvent.add(handler);
        }
        void Updated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_deviceUpdatedEvent) m_deviceUpdatedEvent.remove(token);
        }

        winrt::event_token EnumerationCompleted(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, winrt::Windows::Foundation::IInspectable> const& handler)
        {
            return m_enumerationCompletedEvent.add(handler);
        }
        void EnumerationCompleted(_In_ winrt::event_token const& token) noexcept
        {
            if (m_enumerationCompletedEvent) m_enumerationCompletedEvent.remove(token);
        }

        winrt::event_token Stopped(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, winrt::Windows::Foundation::IInspectable> const& handler)
        {
            return m_stoppedEvent.add(handler);
        }
        void Stopped(_In_ winrt::event_token const& token) noexcept
        {
            if (m_stoppedEvent) m_stoppedEvent.remove(token);
        }


        collections::IMapView<winrt::hstring, midi2::MidiEndpointDeviceInformation> EnumeratedEndpointDevices() 
            { return m_enumeratedEndpointDevices.GetView(); }


    private:
        void InternalInitialize(
            _In_ midi2::MidiEndpointDeviceInformationFilters const& endpointFilters,
            _In_ winrt::Windows::Devices::Enumeration::DeviceWatcher const& baseWatcher);


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


        midi2::MidiEndpointDeviceInformationFilters m_endpointFilter{ MidiEndpointDeviceInformationFilters::AllStandardEndpoints };


        winrt::event<foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, midi2::MidiEndpointDeviceInformationAddedEventArgs>> m_deviceAddedEvent;
        winrt::event<foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, midi2::MidiEndpointDeviceInformationUpdatedEventArgs>> m_deviceUpdatedEvent;
        winrt::event<foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, midi2::MidiEndpointDeviceInformationRemovedEventArgs>> m_deviceRemovedEvent;

        winrt::event<foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, foundation::IInspectable>> m_enumerationCompletedEvent;
        winrt::event<foundation::TypedEventHandler<midi2::MidiEndpointDeviceWatcher, foundation::IInspectable>> m_stoppedEvent;


        winrt::event_token m_deviceAddedEventRevokeToken;
        winrt::event_token m_deviceUpdatedEventRevokeToken;
        winrt::event_token m_deviceRemovedEventRevokeToken;
        winrt::event_token m_enumerationCompletedEventRevokeToken;
        winrt::event_token m_stoppedEventRevokeToken;

        collections::IMap<winrt::hstring, midi2::MidiEndpointDeviceInformation> m_enumeratedEndpointDevices = 
            winrt::multi_threaded_map<winrt::hstring, midi2::MidiEndpointDeviceInformation>();

        enumeration::DeviceWatcher m_watcher{ nullptr };

    };
}

namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceWatcher : MidiEndpointDeviceWatcherT<MidiEndpointDeviceWatcher, implementation::MidiEndpointDeviceWatcher>
    {
    };
}
