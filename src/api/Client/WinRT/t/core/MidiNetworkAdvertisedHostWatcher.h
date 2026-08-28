#pragma once
#include "Transports.Network.MidiNetworkAdvertisedHostWatcher.g.h"

#include "midi_dnssd_browser.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    struct MidiNetworkAdvertisedHostWatcher : MidiNetworkAdvertisedHostWatcherT<MidiNetworkAdvertisedHostWatcher>
    {
        MidiNetworkAdvertisedHostWatcher() = default;
        ~MidiNetworkAdvertisedHostWatcher();

        static network::MidiNetworkAdvertisedHostWatcher Create() noexcept;

        void Start() noexcept;
        void Stop() noexcept;

        bool IsStarted() noexcept;

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

        collections::IMapView<winrt::hstring, network::MidiNetworkAdvertisedHost> EnumeratedHosts() noexcept
        {
            return m_enumeratedHosts.GetView();
        }

    private:
        network::MidiNetworkAdvertisedHost BuildHost(
            _In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service) noexcept;

        void OnServiceAdded(_In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service) noexcept;
        void OnServiceUpdated(_In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service, _In_ uint32_t const changedFields) noexcept;
        void OnServiceRemoved(_In_ std::wstring const& fullName, _In_ std::wstring const& deviceId) noexcept;

        // mDNS never finishes. This raises EnumerationCompleted once, after a settling period,
        // so a one-shot caller like midimdnsinfo has something to wait on.
        void ScheduleEnumerationCompleted() noexcept;

        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostAddedEventArgs>> m_deviceAddedEvent;
        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostUpdatedEventArgs>> m_deviceUpdatedEvent;
        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostRemovedEventArgs>> m_deviceRemovedEvent;

        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable>> m_enumerationCompletedEvent;
        winrt::event<foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable>> m_stoppedEvent;

        collections::IMap<winrt::hstring, network::MidiNetworkAdvertisedHost> m_enumeratedHosts =
            winrt::multi_threaded_map<winrt::hstring, network::MidiNetworkAdvertisedHost>();

        ::WindowsMidiServicesInternal::MidiDnssdBrowser m_browser;

        std::atomic<bool> m_enumerationCompletedRaised{ false };
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Network::factory_implementation
{
    struct MidiNetworkAdvertisedHostWatcher : MidiNetworkAdvertisedHostWatcherT<MidiNetworkAdvertisedHostWatcher, implementation::MidiNetworkAdvertisedHostWatcher>
    {
    };
}
