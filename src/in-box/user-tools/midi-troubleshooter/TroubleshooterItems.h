// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SessionConnectionItem.g.h"
#include "SessionItem.g.h"
#include "TransportItem.g.h"
#include "RegistryEntryItem.g.h"
#include "DriverDeviceItem.g.h"

// The service health pages refresh on a timer, so every row type raises property changed
// rather than being replaced. Nothing here throws: a failing notification must never take
// down a UI callback.
#define MIDI_TSHOOT_OBSERVABLE_ITEM()                                                          \
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

namespace winrt::miditroubleshooter::implementation
{
    struct SessionConnectionItem : SessionConnectionItemT<SessionConnectionItem>
    {
        SessionConnectionItem() = default;

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring CountText() const noexcept { return m_countText; }
        winrt::hstring ConnectedSinceText() const noexcept { return m_connectedSinceText; }

        void Update(
            winrt::hstring const& endpointDeviceId,
            winrt::hstring const& displayName,
            winrt::hstring const& countText,
            winrt::hstring const& connectedSinceText) noexcept;

        MIDI_TSHOOT_OBSERVABLE_ITEM()

        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_countText{};
        winrt::hstring m_connectedSinceText{};
    };

    struct SessionItem : SessionItemT<SessionItem>
    {
        SessionItem() = default;

        winrt::hstring SessionId() const noexcept { return m_sessionId; }
        winrt::hstring Title() const noexcept { return m_title; }
        winrt::hstring ProcessText() const noexcept { return m_processText; }
        winrt::hstring StartedText() const noexcept { return m_startedText; }
        winrt::hstring ConnectionCountText() const noexcept { return m_connectionCountText; }

        collections::IObservableVector<miditroubleshooter::SessionConnectionItem> Connections() const noexcept
        {
            return m_connections;
        }

        void Update(
            winrt::hstring const& sessionId,
            winrt::hstring const& title,
            winrt::hstring const& processText,
            winrt::hstring const& startedText,
            winrt::hstring const& connectionCountText) noexcept;

        MIDI_TSHOOT_OBSERVABLE_ITEM()

        winrt::hstring m_sessionId{};
        winrt::hstring m_title{};
        winrt::hstring m_processText{};
        winrt::hstring m_startedText{};
        winrt::hstring m_connectionCountText{};

        collections::IObservableVector<miditroubleshooter::SessionConnectionItem> m_connections{
            winrt::single_threaded_observable_vector<miditroubleshooter::SessionConnectionItem>() };
    };

    struct TransportItem : TransportItemT<TransportItem>
    {
        TransportItem() = default;

        winrt::hstring TransportId() const noexcept { return m_transportId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring CodeText() const noexcept { return m_codeText; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }
        winrt::hstring ModuleText() const noexcept { return m_moduleText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }

        xaml::Visibility OkVisibility() const noexcept { return m_okVisibility; }
        xaml::Visibility WarningVisibility() const noexcept { return m_warningVisibility; }
        xaml::Visibility ErrorVisibility() const noexcept { return m_errorVisibility; }

        xaml::Visibility DescriptionVisibility() const noexcept
        {
            return m_description.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility ModuleVisibility() const noexcept
        {
            return m_moduleText.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        enum class Severity
        {
            Ok = 0,
            Warning,
            Error
        };

        void Update(
            winrt::hstring const& transportId,
            winrt::hstring const& name,
            winrt::hstring const& codeText,
            winrt::hstring const& description,
            winrt::hstring const& detailText,
            winrt::hstring const& moduleText,
            winrt::hstring const& statusText,
            Severity severity) noexcept;

        MIDI_TSHOOT_OBSERVABLE_ITEM()

        winrt::hstring m_transportId{};
        winrt::hstring m_name{};
        winrt::hstring m_codeText{};
        winrt::hstring m_description{};
        winrt::hstring m_detailText{};
        winrt::hstring m_moduleText{};
        winrt::hstring m_statusText{};

        xaml::Visibility m_okVisibility{ xaml::Visibility::Visible };
        xaml::Visibility m_warningVisibility{ xaml::Visibility::Collapsed };
        xaml::Visibility m_errorVisibility{ xaml::Visibility::Collapsed };
    };

    struct RegistryEntryItem : RegistryEntryItemT<RegistryEntryItem>
    {
        RegistryEntryItem() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Value() const noexcept { return m_value; }
        winrt::hstring Comment() const noexcept { return m_comment; }

        xaml::Visibility OkVisibility() const noexcept { return m_okVisibility; }
        xaml::Visibility WarningVisibility() const noexcept { return m_warningVisibility; }
        xaml::Visibility ErrorVisibility() const noexcept { return m_errorVisibility; }

        void Initialize(
            winrt::hstring const& name,
            winrt::hstring const& value,
            winrt::hstring const& comment,
            uint32_t severity) noexcept;

        MIDI_TSHOOT_OBSERVABLE_ITEM()

        winrt::hstring m_name{};
        winrt::hstring m_value{};
        winrt::hstring m_comment{};

        xaml::Visibility m_okVisibility{ xaml::Visibility::Visible };
        xaml::Visibility m_warningVisibility{ xaml::Visibility::Collapsed };
        xaml::Visibility m_errorVisibility{ xaml::Visibility::Collapsed };
    };

    struct DriverDeviceItem : DriverDeviceItemT<DriverDeviceItem>
    {
        DriverDeviceItem() = default;

        winrt::hstring InstanceId() const noexcept { return m_instanceId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }
        winrt::hstring DriverText() const noexcept { return m_driverText; }
        winrt::hstring ProblemText() const noexcept { return m_problemText; }

        bool CanUseUniversalMidiPacketDriver() const noexcept { return m_canUseUmp; }
        bool CanUseClassicDriver() const noexcept { return m_canUseClassic; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        xaml::Visibility ProblemVisibility() const noexcept
        {
            return m_problemText.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        void Initialize(
            winrt::hstring const& instanceId,
            winrt::hstring const& name,
            winrt::hstring const& detailText,
            winrt::hstring const& driverText,
            winrt::hstring const& problemText,
            bool canUseUniversalMidiPacketDriver,
            bool canUseClassicDriver) noexcept;

        MIDI_TSHOOT_OBSERVABLE_ITEM()

        winrt::hstring m_instanceId{};
        winrt::hstring m_name{};
        winrt::hstring m_detailText{};
        winrt::hstring m_driverText{};
        winrt::hstring m_problemText{};

        bool m_canUseUmp{ false };
        bool m_canUseClassic{ false };
        bool m_isBusy{ false };
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct SessionConnectionItem : SessionConnectionItemT<SessionConnectionItem, implementation::SessionConnectionItem>
    {
    };

    struct SessionItem : SessionItemT<SessionItem, implementation::SessionItem>
    {
    };

    struct TransportItem : TransportItemT<TransportItem, implementation::TransportItem>
    {
    };

    struct RegistryEntryItem : RegistryEntryItemT<RegistryEntryItem, implementation::RegistryEntryItem>
    {
    };

    struct DriverDeviceItem : DriverDeviceItemT<DriverDeviceItem, implementation::DriverDeviceItem>
    {
    };
}
