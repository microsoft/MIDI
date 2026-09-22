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
#include "OrphanedCustomizationItem.g.h"
#include "RelinkCandidateItem.g.h"
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

        winrt::hstring RowAccessibleName() const noexcept;
        winrt::hstring MonitorAccessibleName() const noexcept;
        winrt::hstring PanicAccessibleName() const noexcept;

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

        winrt::hstring RowAccessibleName() const noexcept;

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

        // What was stored when the dialog opened. An emptied box only means "put the generated
        // name back" if there was a custom name to remove, so saving must not send an entry for a
        // port the customer never touched.
        winrt::hstring InternalOriginalCustomName() const noexcept { return m_originalCustomName; }

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
        winrt::hstring m_originalCustomName{};
    };

    struct OrphanedCustomizationItem : OrphanedCustomizationItemT<OrphanedCustomizationItem>
    {
        OrphanedCustomizationItem() = default;

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring ContentSummary() const noexcept { return m_contentSummary; }
        winrt::hstring ProvenanceText() const noexcept { return m_provenanceText; }
        winrt::hstring StoredId() const noexcept { return m_storedId; }
        winrt::hstring TransportCode() const noexcept { return m_transportCode; }

        media::ImageSource Image() const noexcept;

        xaml::Visibility DescriptionVisibility() const noexcept
        {
            return m_description.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility ProvenanceVisibility() const noexcept
        {
            return m_provenanceText.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        void Update(
            winrt::hstring const& displayName,
            winrt::hstring const& description,
            winrt::hstring const& contentSummary,
            winrt::hstring const& provenanceText,
            winrt::hstring const& storedId,
            winrt::hstring const& transportCode,
            winrt::hstring const& imagePath) noexcept;

        midi2config::MidiServiceEndpointCustomization InternalCustomization() const noexcept { return m_customization; }
        void InternalSetCustomization(midi2config::MidiServiceEndpointCustomization const& value) noexcept { m_customization = value; }

        MIDI_SETTINGS_OBSERVABLE_ITEM()

        winrt::hstring m_displayName{};
        winrt::hstring m_description{};
        winrt::hstring m_contentSummary{};
        winrt::hstring m_provenanceText{};
        winrt::hstring m_storedId{};
        winrt::hstring m_transportCode{};
        winrt::hstring m_imagePath{};

        midi2config::MidiServiceEndpointCustomization m_customization{ nullptr };

        mutable media::ImageSource m_image{ nullptr };
    };

    struct RelinkCandidateItem : RelinkCandidateItemT<RelinkCandidateItem>
    {
        RelinkCandidateItem() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring ReasonText() const noexcept { return m_reasonText; }

        xaml::Visibility ReasonVisibility() const noexcept
        {
            return m_reasonText.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility WarningVisibility() const noexcept
        {
            return m_alreadyCustomized ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        void Update(
            winrt::hstring const& name,
            winrt::hstring const& endpointDeviceId,
            winrt::hstring const& reasonText,
            bool const alreadyCustomized) noexcept;

        MIDI_SETTINGS_OBSERVABLE_ITEM()

        winrt::hstring m_name{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_reasonText{};
        bool m_alreadyCustomized{ false };
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
    struct OrphanedCustomizationItem : OrphanedCustomizationItemT<OrphanedCustomizationItem, implementation::OrphanedCustomizationItem> {};
    struct RelinkCandidateItem : RelinkCandidateItemT<RelinkCandidateItem, implementation::RelinkCandidateItem> {};
    struct TransportChoice : TransportChoiceT<TransportChoice, implementation::TransportChoice> {};
    struct ConfigFileChoice : ConfigFileChoiceT<ConfigFileChoice, implementation::ConfigFileChoice> {};
}
