// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostWatcher.h"
#include "Endpoints.Network.MidiNetworkAdvertisedHostWatcher.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{
    midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher MidiNetworkAdvertisedHostWatcher::Create()
    {
        throw hresult_not_implemented();
    }
    void MidiNetworkAdvertisedHostWatcher::Start()
    {
        throw hresult_not_implemented();
    }
    void MidiNetworkAdvertisedHostWatcher::Stop()
    {
        throw hresult_not_implemented();
    }
    collections::IMapView<winrt::hstring, midi2::Endpoints::Network::MidiNetworkAdvertisedHost> MidiNetworkAdvertisedHostWatcher::EnumeratedHosts()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Enumeration::DeviceWatcherStatus MidiNetworkAdvertisedHostWatcher::Status()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Added(
        foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, midi2::Endpoints::Network::MidiNetworkAdvertisedHostAddedEventArgs> const& handler)
    {
        UNREFERENCED_PARAMETER(handler);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Added(winrt::event_token const& token) noexcept
    {
        UNREFERENCED_PARAMETER(token);

    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Removed(
        foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, midi2::Endpoints::Network::MidiNetworkAdvertisedHostRemovedEventArgs> const& handler)
    {
        UNREFERENCED_PARAMETER(handler);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Removed(winrt::event_token const& token) noexcept
    {
        UNREFERENCED_PARAMETER(token);

    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Updated(
        foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, midi2::Endpoints::Network::MidiNetworkAdvertisedHostUpdatedEventArgs> const& handler)
    {
        UNREFERENCED_PARAMETER(handler);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Updated(winrt::event_token const& token) noexcept
    {
        UNREFERENCED_PARAMETER(token);

    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::EnumerationCompleted(
        foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler)
    {
        UNREFERENCED_PARAMETER(handler);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::EnumerationCompleted(winrt::event_token const& token) noexcept
    {
        UNREFERENCED_PARAMETER(token);

    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Stopped(
        foundation::TypedEventHandler<midi2::Endpoints::Network::MidiNetworkAdvertisedHostWatcher, winrt::Windows::Foundation::IInspectable> const& handler)
    {
        UNREFERENCED_PARAMETER(handler);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Stopped(winrt::event_token const& token) noexcept
    {
        UNREFERENCED_PARAMETER(token);

    }
}
