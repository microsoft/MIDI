// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ClockItem.g.h"
#include "ClockStore.h"

namespace midiclock
{
    // Everything a tile shows, gathered in one place so a refresh is a single call rather than
    // a dozen setters which each raise a change notification.
    struct ClockRowData
    {
        std::wstring Id{};
        std::wstring DisplayName{};
        double BeatsPerMinute{ DefaultBeatsPerMinute };
        std::wstring EndpointName{};
        int32_t GroupIndex{ 0 };
        bool IsEndpointMissing{ false };
    };
}

namespace winrt::midiclock::implementation
{
    struct ClockItem : ClockItemT<ClockItem>
    {
        ClockItem() = default;

        void Update(::midiclock::ClockRowData const& data) noexcept;

        winrt::hstring Id() const noexcept { return m_id; }
        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring TempoText() const noexcept { return m_tempoText; }
        winrt::hstring DestinationText() const noexcept { return m_destinationText; }
        winrt::hstring StatusText() const noexcept;

        bool IsRunning() const noexcept { return m_isRunning; }
        void IsRunning(bool value) noexcept;

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool value) noexcept;

        bool IsStartStopEnabled() const noexcept { return !m_isBusy; }

        bool IsEndpointMissing() const noexcept { return m_isEndpointMissing; }

        bool IsSelected() const noexcept { return m_isSelected; }
        void IsSelected(bool value) noexcept;

        xaml::Visibility RunningVisibility() const noexcept
        {
            return m_isRunning ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility WarningVisibility() const noexcept
        {
            return m_isEndpointMissing ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        winrt::hstring StartStopLabel() const noexcept;
        winrt::hstring StartStopGlyph() const noexcept;
        winrt::hstring StartStopAccessibleName() const noexcept;
        winrt::hstring EditAccessibleName() const noexcept;
        winrt::hstring SelectAccessibleName() const noexcept;

        winrt::event_token PropertyChanged(
            xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        // A failing notification must never take down a UI callback.
        void RaisePropertyChanged(std::wstring_view const name) noexcept
        {
            try
            {
                m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ name });
            }
            catch (...)
            {
            }
        }

        void RaiseRunStateChanged() noexcept;

        winrt::hstring m_id{};
        winrt::hstring m_displayName{};
        winrt::hstring m_tempoText{};
        winrt::hstring m_destinationText{};

        bool m_isRunning{ false };
        bool m_isBusy{ false };
        bool m_isEndpointMissing{ false };
        bool m_isSelected{ false };

        winrt::event<xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};
    };
}

namespace winrt::midiclock::factory_implementation
{
    struct ClockItem : ClockItemT<ClockItem, implementation::ClockItem>
    {
    };
}
