// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ImportPortItem.g.h"
#include "ImportDeviceItem.g.h"

namespace winrt::midiloopbacksetup::implementation
{
    struct ImportPortItem : ImportPortItemT<ImportPortItem>
    {
        ImportPortItem() = default;

        winrt::hstring PortName() const noexcept { return m_portName; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }
        winrt::hstring SourceDeviceName() const noexcept { return m_sourceDeviceName; }

        bool IsSelected() const noexcept { return m_isSelected; }
        void IsSelected(bool const value) noexcept { m_isSelected = value && m_isSelectable; }

        bool IsSelectable() const noexcept { return m_isSelectable; }
        winrt::hstring ConflictText() const noexcept { return m_conflictText; }

        xaml::Visibility ConflictVisibility() const noexcept
        {
            return m_conflictText.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        void InternalInitialize(
            _In_ winrt::hstring const& portName,
            _In_ winrt::hstring const& detailText,
            _In_ winrt::hstring const& sourceDeviceName,
            _In_ winrt::hstring const& conflictText) noexcept
        {
            m_portName = portName;
            m_detailText = detailText;
            m_sourceDeviceName = sourceDeviceName;
            m_conflictText = conflictText;

            m_isSelectable = conflictText.empty();
        }

    private:
        winrt::hstring m_portName{};
        winrt::hstring m_detailText{};
        winrt::hstring m_sourceDeviceName{};
        winrt::hstring m_conflictText{};

        bool m_isSelectable{ true };
        bool m_isSelected{ false };
    };


    struct ImportDeviceItem : ImportDeviceItemT<ImportDeviceItem>
    {
        ImportDeviceItem() = default;

        winrt::hstring DeviceName() const noexcept { return m_deviceName; }
        winrt::hstring ServiceText() const noexcept { return m_serviceText; }

        collections::IObservableVector<midiloopbacksetup::ImportPortItem> Ports() const noexcept { return m_ports; }

        void InternalInitialize(
            _In_ winrt::hstring const& deviceName,
            _In_ winrt::hstring const& serviceText) noexcept
        {
            m_deviceName = deviceName;
            m_serviceText = serviceText;
        }

    private:
        winrt::hstring m_deviceName{};
        winrt::hstring m_serviceText{};

        collections::IObservableVector<midiloopbacksetup::ImportPortItem> m_ports{
            winrt::single_threaded_observable_vector<midiloopbacksetup::ImportPortItem>() };
    };
}

namespace winrt::midiloopbacksetup::factory_implementation
{
    struct ImportPortItem : ImportPortItemT<ImportPortItem, implementation::ImportPortItem>
    {
    };

    struct ImportDeviceItem : ImportDeviceItemT<ImportDeviceItem, implementation::ImportDeviceItem>
    {
    };
}
