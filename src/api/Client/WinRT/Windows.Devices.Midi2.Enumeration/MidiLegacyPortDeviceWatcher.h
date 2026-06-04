// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Legacy.MidiLegacyPortDeviceWatcher.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceWatcher : MidiLegacyPortDeviceWatcherT<MidiLegacyPortDeviceWatcher>
    {
        MidiLegacyPortDeviceWatcher() = default;
        ~MidiLegacyPortDeviceWatcher();

        static legacy::MidiLegacyPortDeviceWatcher Create() noexcept;
        static legacy::MidiLegacyPortDeviceWatcher CreateForFlow(_In_ midi2enum::Midi1PortFlow const flow) noexcept;

        void Start() noexcept;
        void Stop() noexcept;

        enumeration::DeviceWatcherStatus Status() noexcept { return m_watcher ? m_watcher.Status() : enumeration::DeviceWatcherStatus::Created; }

        winrt::event_token Added(_In_ foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationAddedEventArgs> const& handler);
        void Added(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Updated(_In_ foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationUpdatedEventArgs> const& handler);
        void Updated(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Removed(_In_ foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationRemovedEventArgs> const& handler);
        void Removed(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token EnumerationCompleted(_In_ foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, foundation::IInspectable> const& handler);
        void EnumerationCompleted(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Stopped(_In_ foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, foundation::IInspectable> const& handler);
        void Stopped(_In_ winrt::event_token const& token) noexcept;

        collections::IMapView<winrt::hstring, legacy::MidiLegacyPortDeviceInformation> EnumeratedPorts()
        {
            return m_enumeratedPorts.GetView();
        }


        legacy::MidiLegacyPortDeviceInformation GetEnumeratedPortForNumber(_In_ uint32_t const portNumber, _In_ midi2enum::Midi1PortFlow const flow) const noexcept;

        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForFlow(_In_ midi2enum::Midi1PortFlow const flow) const noexcept;

        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForParent(_In_ winrt::hstring const& parentDeviceInstanceId) const noexcept;
        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForParent(_In_ winrt::hstring const& parentDeviceInstanceId, _In_ midi2enum::Midi1PortFlow const flow) const noexcept;

        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForEndpoint(_In_ winrt::hstring const& endpointDeviceId) const noexcept;
        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForEndpoint(_In_ winrt::hstring const& endpointDeviceId, _In_ midi2enum::Midi1PortFlow const flow) const noexcept;

        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForName(_In_ winrt::hstring const& portName) const noexcept;
        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForName(_In_ winrt::hstring const& portName, _In_ midi2enum::Midi1PortFlow const flow) const noexcept;

        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForDriverDeviceInterfaceId(_In_ winrt::hstring const& driverDeviceInterfaceId) const noexcept;
        collections::IVectorView<legacy::MidiLegacyPortDeviceInformation> GetEnumeratedPortsForDriverDeviceInterfaceId(_In_ winrt::hstring const& driverDeviceInterfaceId, _In_ midi2enum::Midi1PortFlow const flow) const noexcept;


        uint32_t CountSourcePorts() const noexcept { return m_countSourcePorts; }
        uint32_t CountDestinationPorts() const noexcept { return m_countDestinationPorts; }

        void InternalInitialize(_In_ enumeration::DeviceWatcher const& baseWatcher) noexcept;

    private:
        uint32_t m_countSourcePorts{ 0 };
        uint32_t m_countDestinationPorts{ 0 };


        // internal event handlers for the base device watcher we wrap
        void OnDeviceAdded(
            _In_ enumeration::DeviceWatcher const& source,
            _In_ enumeration::DeviceInformation const& args) noexcept;

        void OnDeviceUpdated(
            _In_ enumeration::DeviceWatcher const& source,
            _In_ enumeration::DeviceInformationUpdate const& args) noexcept;

        void OnDeviceRemoved(
            _In_ enumeration::DeviceWatcher const& source,
            _In_ enumeration::DeviceInformationUpdate const& args) noexcept;

        void OnEnumerationCompleted(
            _In_ enumeration::DeviceWatcher const& source,
            _In_ foundation::IInspectable const& args) noexcept;

        void OnStopped(
            _In_ enumeration::DeviceWatcher const& source,
            _In_ foundation::IInspectable const& args) noexcept;


        winrt::event<foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationAddedEventArgs>> m_deviceAddedEvent;
        winrt::event<foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationUpdatedEventArgs>> m_deviceUpdatedEvent;
        winrt::event<foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, legacy::MidiLegacyPortDeviceInformationRemovedEventArgs>> m_deviceRemovedEvent;

        winrt::event<foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, foundation::IInspectable>> m_enumerationCompletedEvent;
        winrt::event<foundation::TypedEventHandler<legacy::MidiLegacyPortDeviceWatcher, foundation::IInspectable>> m_stoppedEvent;


        winrt::event_token m_deviceAddedEventRevokeToken;
        winrt::event_token m_deviceUpdatedEventRevokeToken;
        winrt::event_token m_deviceRemovedEventRevokeToken;
        winrt::event_token m_enumerationCompletedEventRevokeToken;
        winrt::event_token m_stoppedEventRevokeToken;

        collections::IMap<winrt::hstring, legacy::MidiLegacyPortDeviceInformation> m_enumeratedPorts =
            winrt::multi_threaded_map<winrt::hstring, legacy::MidiLegacyPortDeviceInformation>();

        enumeration::DeviceWatcher m_watcher{ nullptr };


    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::factory_implementation
{
    struct MidiLegacyPortDeviceWatcher : MidiLegacyPortDeviceWatcherT<MidiLegacyPortDeviceWatcher, implementation::MidiLegacyPortDeviceWatcher>
    {
    };
}
