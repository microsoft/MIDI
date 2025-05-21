#pragma once
#include "Endpoints.Network.MidiNetworkAdvertisedHostWatcher.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkAdvertisedHostWatcher : MidiNetworkAdvertisedHostWatcherT<MidiNetworkAdvertisedHostWatcher>
    {
        MidiNetworkAdvertisedHostWatcher() = default;
        ~MidiNetworkAdvertisedHostWatcher();

        static network::MidiNetworkAdvertisedHostWatcher Create() noexcept;

        void Start();
        void Stop();

        enumeration::DeviceWatcherStatus Status();

        winrt::event_token Added(_In_ foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostAddedEventArgs> const& handler);
        void Added(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Removed(_In_ foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostRemovedEventArgs> const& handler);
        void Removed(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Updated(_In_ foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostUpdatedEventArgs> const& handler);
        void Updated(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token EnumerationCompleted(_In_ foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler);
        void EnumerationCompleted(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Stopped(_In_ foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler);
        void Stopped(_In_ winrt::event_token const& token) noexcept;

        collections::IMapView<winrt::hstring, network::MidiNetworkAdvertisedHost> EnumeratedHosts()
        {
            return m_enumeratedHosts.GetView();
        }

    private:
        void InternalInitialize(
            _In_ enumeration::DeviceWatcher const& baseWatcher);


        // internal event handlers
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


        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostAddedEventArgs>> m_deviceAddedEvent;
        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostUpdatedEventArgs>> m_deviceUpdatedEvent;
        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostRemovedEventArgs>> m_deviceRemovedEvent;

        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable>> m_enumerationCompletedEvent;
        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable>> m_stoppedEvent;


        winrt::event_token m_deviceAddedEventRevokeToken{ };
        winrt::event_token m_deviceUpdatedEventRevokeToken{ };
        winrt::event_token m_deviceRemovedEventRevokeToken{ };
        winrt::event_token m_enumerationCompletedEventRevokeToken{ };
        winrt::event_token m_stoppedEventRevokeToken{ };

        collections::IMap<winrt::hstring, network::MidiNetworkAdvertisedHost> m_enumeratedHosts =
            winrt::multi_threaded_map<winrt::hstring, network::MidiNetworkAdvertisedHost>();

        enumeration::DeviceWatcher m_watcher{ nullptr };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkAdvertisedHostWatcher : MidiNetworkAdvertisedHostWatcherT<MidiNetworkAdvertisedHostWatcher, implementation::MidiNetworkAdvertisedHostWatcher>
    {
    };
}
