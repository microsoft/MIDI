// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "EndpointItem.g.h"
#include "Midi1PortItem.g.h"
#include "Midi1PortNameItem.g.h"
#include "TransportChoice.g.h"
#include "ConfigFileChoice.g.h"

// The endpoint watcher updates rows in place rather than rebuilding the collection, so every
// row type raises property changed. Nothing here throws: a failing notification must never
// take down a UI callback.
#define MIDI_SETTINGS_OBSERVABLE_ITEM()                                                        \
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

namespace winrt::midisettings::implementation
{
    struct EndpointItem : EndpointItemT<EndpointItem>
    {
        EndpointItem() = default;

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring TransportCode() const noexcept { return m_transportCode; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }

        media::ImageSource Image() const noexcept;

        xaml::Visibility DescriptionVisibility() const noexcept
        {
            return m_description.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility MonitorVisibility() const noexcept
        {
            return m_canMonitor ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        void Update(
            winrt::hstring const& endpointDeviceId,
            winrt::hstring const& name,
            winrt::hstring const& description,
            winrt::hstring const& transportCode,
            winrt::hstring const& detailText,
            winrt::hstring const& imagePath,
            bool const canMonitor) noexcept;

        MIDI_SETTINGS_OBSERVABLE_ITEM()

        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_name{};
        winrt::hstring m_description{};
        winrt::hstring m_transportCode{};
        winrt::hstring m_detailText{};
        winrt::hstring m_imagePath{};
        bool m_canMonitor{ false };

        mutable media::ImageSource m_image{ nullptr };
    };

    struct Midi1PortItem : Midi1PortItemT<Midi1PortItem>
    {
        Midi1PortItem() = default;

        winrt::hstring PortDeviceId() const noexcept { return m_portDeviceId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }

        void Update(
            winrt::hstring const& portDeviceId,
            winrt::hstring const& name,
            winrt::hstring const& detailText) noexcept;

        MIDI_SETTINGS_OBSERVABLE_ITEM()

        winrt::hstring m_portDeviceId{};
        winrt::hstring m_name{};
        winrt::hstring m_detailText{};
    };

    struct Midi1PortNameItem : Midi1PortNameItemT<Midi1PortNameItem>
    {
        Midi1PortNameItem() = default;

        uint8_t GroupIndex() const noexcept { return m_groupIndex; }
        winrt::hstring GroupNumberText() const noexcept { return m_groupNumberText; }
        winrt::hstring CurrentName() const noexcept { return m_currentName; }
        winrt::hstring LegacyCompatibleName() const noexcept { return m_legacyCompatibleName; }
        winrt::hstring NewStyleName() const noexcept { return m_newStyleName; }

        winrt::hstring CustomName() const noexcept { return m_customName; }
        void CustomName(winrt::hstring const& value) noexcept
        {
            UpdateField(m_customName, value, L"CustomName");
        }

        void Update(
            uint8_t const groupIndex,
            winrt::hstring const& currentName,
            winrt::hstring const& legacyCompatibleName,
            winrt::hstring const& newStyleName,
            winrt::hstring const& customName) noexcept;

        MIDI_SETTINGS_OBSERVABLE_ITEM()

        uint8_t m_groupIndex{ 0 };
        winrt::hstring m_groupNumberText{};
        winrt::hstring m_currentName{};
        winrt::hstring m_legacyCompatibleName{};
        winrt::hstring m_newStyleName{};
        winrt::hstring m_customName{};
    };

    struct TransportChoice : TransportChoiceT<TransportChoice>
    {
        TransportChoice() = default;

        TransportChoice(winrt::hstring const& displayName, winrt::hstring const& transportCode) :
            m_displayName(displayName),
            m_transportCode(transportCode)
        {
        }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring TransportCode() const noexcept { return m_transportCode; }

    private:
        winrt::hstring m_displayName{};
        winrt::hstring m_transportCode{};
    };

    struct ConfigFileChoice : ConfigFileChoiceT<ConfigFileChoice>
    {
        ConfigFileChoice() = default;

        ConfigFileChoice(winrt::hstring const& displayName, winrt::hstring const& fileName) :
            m_displayName(displayName),
            m_fileName(fileName)
        {
        }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring FileName() const noexcept { return m_fileName; }

    private:
        winrt::hstring m_displayName{};
        winrt::hstring m_fileName{};
    };
}

namespace winrt::midisettings::factory_implementation
{
    struct EndpointItem : EndpointItemT<EndpointItem, implementation::EndpointItem> {};
    struct Midi1PortItem : Midi1PortItemT<Midi1PortItem, implementation::Midi1PortItem> {};
    struct Midi1PortNameItem : Midi1PortNameItemT<Midi1PortNameItem, implementation::Midi1PortNameItem> {};
    struct TransportChoice : TransportChoiceT<TransportChoice, implementation::TransportChoice> {};
    struct ConfigFileChoice : ConfigFileChoiceT<ConfigFileChoice, implementation::ConfigFileChoice> {};
}
