// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "PendingInvitationItem.g.h"
#include "RemoteHostItem.g.h"
#include "HostConnectionItem.g.h"
#include "KnownClientItem.g.h"
#include "LocalHostItem.g.h"

// The pages refresh from the service on a timer, so every row type raises property changed
// rather than being replaced. Nothing here throws: a failing notification must never take
// down a UI callback.
#define MIDI_NETSETUP_OBSERVABLE_ITEM()                                                        \
    public:                                                                                    \
        winrt::event_token PropertyChanged(                                                    \
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)      \
        {                                                                                      \
            return m_propertyChanged.add(handler);                                             \
        }                                                                                      \
                                                                                               \
        void PropertyChanged(winrt::event_token const& token) noexcept                         \
        {                                                                                      \
            m_propertyChanged.remove(token);                                                   \
        }                                                                                      \
                                                                                               \
    private:                                                                                   \
        void RaisePropertyChanged(std::wstring_view const name) noexcept                       \
        {                                                                                      \
            try                                                                                \
            {                                                                                  \
                m_propertyChanged(                                                             \
                    *this,                                                                     \
                    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ name });       \
            }                                                                                  \
            catch (...)                                                                        \
            {                                                                                  \
            }                                                                                  \
        }                                                                                      \
                                                                                               \
        template <typename TValue>                                                             \
        bool UpdateField(TValue& field, TValue const& value, std::wstring_view const name) noexcept \
        {                                                                                      \
            if (field == value)                                                                \
            {                                                                                  \
                return false;                                                                  \
            }                                                                                  \
                                                                                               \
            field = value;                                                                     \
            RaisePropertyChanged(name);                                                        \
                                                                                               \
            return true;                                                                       \
        }                                                                                      \
                                                                                               \
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};

namespace winrt::midinetworksetup::implementation
{
    struct PendingInvitationItem : PendingInvitationItemT<PendingInvitationItem>
    {
        PendingInvitationItem() = default;

        winrt::hstring HostId() const noexcept { return m_hostId; }
        winrt::hstring RemoteName() const noexcept { return m_remoteName; }
        winrt::hstring RemoteProductInstanceId() const noexcept { return m_remoteProductInstanceId; }

        winrt::hstring Headline() const noexcept { return m_headline; }
        winrt::hstring Detail() const noexcept { return m_detail; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        void InternalInitialize(
            _In_ winrt::hstring const& hostId,
            _In_ winrt::hstring const& remoteName,
            _In_ winrt::hstring const& remoteProductInstanceId) noexcept
        {
            m_hostId = hostId;
            m_remoteName = remoteName;
            m_remoteProductInstanceId = remoteProductInstanceId;
        }

        void InternalUpdateText(
            _In_ winrt::hstring const& headline,
            _In_ winrt::hstring const& detail) noexcept
        {
            UpdateField(m_headline, headline, L"Headline");
            UpdateField(m_detail, detail, L"Detail");
        }

    private:
        winrt::hstring m_hostId{};
        winrt::hstring m_remoteName{};
        winrt::hstring m_remoteProductInstanceId{};
        winrt::hstring m_headline{};
        winrt::hstring m_detail{};
        bool m_isBusy{ false };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct RemoteHostItem : RemoteHostItemT<RemoteHostItem>
    {
        RemoteHostItem() = default;

        winrt::hstring MatchKey() const noexcept { return m_matchKey; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring SubtitleText() const noexcept { return m_subtitleText; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring AddressesText() const noexcept { return m_addressesText; }
        winrt::hstring DeviceId() const noexcept { return m_deviceId; }
        winrt::hstring ConnectAddress() const noexcept { return m_connectAddress; }
        uint16_t ConnectPort() const noexcept { return m_connectPort; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring StatisticsText() const noexcept { return m_statisticsText; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring ClientId() const noexcept { return m_clientId; }

        bool IsConnected() const noexcept { return m_isConnected; }
        bool IsConfigured() const noexcept { return m_isConfigured; }
        bool IsAdvertised() const noexcept { return m_isAdvertised; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept
        {
            if (UpdateField(m_isBusy, value, L"IsBusy"))
            {
                RaiseButtonVisibilities();
            }
        }

        winrt::Microsoft::UI::Xaml::Visibility ConnectVisibility() const noexcept
        {
            return (!m_isConfigured && !m_isBusy) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        // a configured entry which is not in a session can be re-armed rather than recreated
        winrt::Microsoft::UI::Xaml::Visibility RetryVisibility() const noexcept
        {
            return (m_isConfigured && !m_isConnected && !m_isBusy) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility DisconnectVisibility() const noexcept
        {
            return (m_isConfigured && !m_isBusy) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility NotAdvertisedVisibility() const noexcept
        {
            return m_isAdvertised ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        void InternalInitialize(_In_ winrt::hstring const& matchKey) noexcept
        {
            m_matchKey = matchKey;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& subtitleText,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& addressesText,
            _In_ winrt::hstring const& deviceId,
            _In_ winrt::hstring const& connectAddress,
            _In_ uint16_t const connectPort,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& statisticsText,
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& clientId,
            _In_ bool const isConnected,
            _In_ bool const isConfigured,
            _In_ bool const isAdvertised) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_subtitleText, subtitleText, L"SubtitleText");
            UpdateField(m_productInstanceId, productInstanceId, L"ProductInstanceId");
            UpdateField(m_addressesText, addressesText, L"AddressesText");
            UpdateField(m_deviceId, deviceId, L"DeviceId");
            UpdateField(m_connectAddress, connectAddress, L"ConnectAddress");
            UpdateField(m_connectPort, connectPort, L"ConnectPort");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_statisticsText, statisticsText, L"StatisticsText");
            UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId");
            UpdateField(m_clientId, clientId, L"ClientId");

            auto const connectedChanged = UpdateField(m_isConnected, isConnected, L"IsConnected");
            auto const configuredChanged = UpdateField(m_isConfigured, isConfigured, L"IsConfigured");

            if (UpdateField(m_isAdvertised, isAdvertised, L"IsAdvertised"))
            {
                RaisePropertyChanged(L"NotAdvertisedVisibility");
            }

            if (connectedChanged || configuredChanged)
            {
                RaiseButtonVisibilities();
            }
        }

    private:
        void RaiseButtonVisibilities() noexcept
        {
            RaisePropertyChanged(L"ConnectVisibility");
            RaisePropertyChanged(L"RetryVisibility");
            RaisePropertyChanged(L"DisconnectVisibility");
        }

        winrt::hstring m_matchKey{};
        winrt::hstring m_displayName{};
        winrt::hstring m_subtitleText{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_addressesText{};
        winrt::hstring m_deviceId{};
        winrt::hstring m_connectAddress{};
        uint16_t m_connectPort{ 0 };
        winrt::hstring m_statusText{};
        winrt::hstring m_statisticsText{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_clientId{};
        bool m_isConnected{ false };
        bool m_isConfigured{ false };
        bool m_isAdvertised{ false };
        bool m_isBusy{ false };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct HostConnectionItem : HostConnectionItemT<HostConnectionItem>
    {
        HostConnectionItem() = default;

        winrt::hstring MatchKey() const noexcept { return m_matchKey; }
        winrt::hstring HostId() const noexcept { return m_hostId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring AddressText() const noexcept { return m_addressText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }

        bool IsPendingApproval() const noexcept { return m_isPendingApproval; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        void InternalInitialize(
            _In_ winrt::hstring const& matchKey,
            _In_ winrt::hstring const& hostId,
            _In_ winrt::hstring const& productInstanceId) noexcept
        {
            m_matchKey = matchKey;
            m_hostId = hostId;
            m_productInstanceId = productInstanceId;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& addressText,
            _In_ winrt::hstring const& statusText,
            _In_ bool const isPendingApproval) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_addressText, addressText, L"AddressText");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_isPendingApproval, isPendingApproval, L"IsPendingApproval");
        }

    private:
        winrt::hstring m_matchKey{};
        winrt::hstring m_hostId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_addressText{};
        winrt::hstring m_statusText{};
        bool m_isPendingApproval{ false };
        bool m_isBusy{ false };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct KnownClientItem : KnownClientItemT<KnownClientItem>
    {
        KnownClientItem() = default;

        winrt::hstring MatchKey() const noexcept { return m_matchKey; }
        winrt::hstring HostId() const noexcept { return m_hostId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring DecisionText() const noexcept { return m_decisionText; }

        bool IsAllowed() const noexcept { return m_isAllowed; }

        void InternalInitialize(
            _In_ winrt::hstring const& matchKey,
            _In_ winrt::hstring const& hostId,
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& decisionText,
            _In_ bool const isAllowed) noexcept
        {
            m_matchKey = matchKey;
            m_hostId = hostId;
            m_displayName = displayName;
            m_productInstanceId = productInstanceId;
            m_decisionText = decisionText;
            m_isAllowed = isAllowed;
        }

    private:
        winrt::hstring m_matchKey{};
        winrt::hstring m_hostId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_decisionText{};
        bool m_isAllowed{ true };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct LocalHostItem : LocalHostItemT<LocalHostItem>
    {
        LocalHostItem();

        winrt::hstring HostId() const noexcept { return m_hostId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring AddressText() const noexcept { return m_addressText; }
        winrt::hstring PortText() const noexcept { return m_portText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring PolicyText() const noexcept { return m_policyText; }
        winrt::hstring ConnectionCountText() const noexcept { return m_connectionCountText; }
        winrt::hstring StartStopLabel() const noexcept { return m_startStopLabel; }

        bool HasStarted() const noexcept { return m_hasStarted; }
        bool CreatesMidi1Ports() const noexcept { return m_createsMidi1Ports; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        winrt::Microsoft::UI::Xaml::Visibility NoConnectionsVisibility() const noexcept
        {
            return m_connections.Size() == 0 ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility NoKnownClientsVisibility() const noexcept
        {
            return m_knownClients.Size() == 0 ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::HostConnectionItem> Connections() const noexcept
        {
            return m_connections;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::KnownClientItem> KnownClients() const noexcept
        {
            return m_knownClients;
        }

        void InternalInitialize(_In_ winrt::hstring const& hostId) noexcept
        {
            m_hostId = hostId;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& serviceInstanceName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& addressText,
            _In_ winrt::hstring const& portText,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& policyText,
            _In_ winrt::hstring const& connectionCountText,
            _In_ winrt::hstring const& startStopLabel,
            _In_ bool const hasStarted,
            _In_ bool const createsMidi1Ports) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_serviceInstanceName, serviceInstanceName, L"ServiceInstanceName");
            UpdateField(m_productInstanceId, productInstanceId, L"ProductInstanceId");
            UpdateField(m_addressText, addressText, L"AddressText");
            UpdateField(m_portText, portText, L"PortText");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_policyText, policyText, L"PolicyText");
            UpdateField(m_connectionCountText, connectionCountText, L"ConnectionCountText");
            UpdateField(m_startStopLabel, startStopLabel, L"StartStopLabel");
            UpdateField(m_hasStarted, hasStarted, L"HasStarted");
            UpdateField(m_createsMidi1Ports, createsMidi1Ports, L"CreatesMidi1Ports");
        }

        // the empty state placeholders are computed from the collections, so they only change
        // when something is added or removed
        void InternalRaiseEmptyStateChanged() noexcept
        {
            RaisePropertyChanged(L"NoConnectionsVisibility");
            RaisePropertyChanged(L"NoKnownClientsVisibility");
        }

    private:
        winrt::hstring m_hostId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_addressText{};
        winrt::hstring m_portText{};
        winrt::hstring m_statusText{};
        winrt::hstring m_policyText{};
        winrt::hstring m_connectionCountText{};
        winrt::hstring m_startStopLabel{};
        bool m_hasStarted{ false };
        bool m_createsMidi1Ports{ false };
        bool m_isBusy{ false };

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::HostConnectionItem> m_connections{
            winrt::single_threaded_observable_vector<midinetworksetup::HostConnectionItem>() };

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::KnownClientItem> m_knownClients{
            winrt::single_threaded_observable_vector<midinetworksetup::KnownClientItem>() };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };
}

namespace winrt::midinetworksetup::factory_implementation
{
    struct PendingInvitationItem : PendingInvitationItemT<PendingInvitationItem, implementation::PendingInvitationItem> {};
    struct RemoteHostItem : RemoteHostItemT<RemoteHostItem, implementation::RemoteHostItem> {};
    struct HostConnectionItem : HostConnectionItemT<HostConnectionItem, implementation::HostConnectionItem> {};
    struct KnownClientItem : KnownClientItemT<KnownClientItem, implementation::KnownClientItem> {};
    struct LocalHostItem : LocalHostItemT<LocalHostItem, implementation::LocalHostItem> {};
}
