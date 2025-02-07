#pragma once
#include "Endpoints.Network.MidiNetworkAdvertisedHostWatcher.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    struct MidiNetworkAdvertisedHostWatcher : MidiNetworkAdvertisedHostWatcherT<MidiNetworkAdvertisedHostWatcher>
    {
        MidiNetworkAdvertisedHostWatcher() = default;

        static midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher Create();

        void Start();
        void Stop();

        collections::IMapView<hstring, midi2::Endpoints::Network::MidiNetworkAdvertisedHost> EnumeratedHosts();
        enumeration::DeviceWatcherStatus Status();

        winrt::event_token Added(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, midi2::Endpoints::Network::MidiNetworkAdvertisedHostAddedEventArgs> const& handler);
        void Added(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Removed(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, midi2::Endpoints::Network::MidiNetworkAdvertisedHostRemovedEventArgs> const& handler);
        void Removed(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Updated(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, midi2::Endpoints::Network::MidiNetworkAdvertisedHostUpdatedEventArgs> const& handler);
        void Updated(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token EnumerationCompleted(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler);
        void EnumerationCompleted(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token Stopped(_In_ winrt::Windows::Foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler);
        void Stopped(_In_ winrt::event_token const& token) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::factory_implementation
{
    struct MidiNetworkAdvertisedHostWatcher : MidiNetworkAdvertisedHostWatcherT<MidiNetworkAdvertisedHostWatcher, implementation::MidiNetworkAdvertisedHostWatcher>
    {
    };
}
