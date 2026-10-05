// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SynthItem.g.h"

namespace winrt::midisoundfontsynth::implementation
{
    struct SynthItem : SynthItemT<SynthItem>
    {
        SynthItem() = default;

        winrt::hstring Id() const noexcept { return m_id; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring FileName() const noexcept { return m_fileName; }
        winrt::hstring FilePath() const noexcept { return m_filePath; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring VoicesText() const noexcept { return m_voicesText; }

        double VolumeDb() const noexcept { return m_volumeDb; }
        winrt::hstring VolumeText() const noexcept { return m_volumeText; }

        bool IsOn() const noexcept { return m_isOn; }
        bool IsBusy() const noexcept { return m_isBusy; }

        xaml::Visibility BusyVisibility() const noexcept { return m_busyVisibility; }
        xaml::Visibility WarningVisibility() const noexcept { return m_warningVisibility; }
        xaml::Visibility InUseVisibility() const noexcept { return m_inUseVisibility; }
        xaml::Visibility DetailVisibility() const noexcept { return m_detailVisibility; }

        winrt::hstring AccessibleName() const noexcept { return m_accessibleName; }
        winrt::hstring ToggleAccessibleName() const noexcept { return m_toggleAccessibleName; }
        winrt::hstring VolumeAccessibleName() const noexcept { return m_volumeAccessibleName; }
        winrt::hstring MoreAccessibleName() const noexcept { return m_moreAccessibleName; }

        winrt::event_token PropertyChanged(xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

        // Set by the window. Each raises a change only when the value really changed, so the
        // refresh timer can call them every tick without making the list redraw.
        void SetId(_In_ winrt::hstring const& value) { m_id = value; }
        void SetName(_In_ winrt::hstring const& value) { Set(m_name, value, L"Name"); }
        void SetFile(_In_ winrt::hstring const& path, _In_ winrt::hstring const& fileName);
        void SetDetailText(_In_ winrt::hstring const& value);
        void SetStatusText(_In_ winrt::hstring const& value) { Set(m_statusText, value, L"StatusText"); }
        void SetVoicesText(_In_ winrt::hstring const& value) { Set(m_voicesText, value, L"VoicesText"); }
        void SetVolume(_In_ double decibels, _In_ winrt::hstring const& text);
        void SetIsOn(_In_ bool value) { Set(m_isOn, value, L"IsOn"); }
        void SetIsBusy(_In_ bool value)
        {
            Set(m_isBusy, value, L"IsBusy");
            Set(m_busyVisibility, value ? xaml::Visibility::Visible : xaml::Visibility::Collapsed, L"BusyVisibility");
        }
        void SetWarning(_In_ bool value);
        void SetInUse(_In_ bool value);
        void SetAccessibleNames(
            _In_ winrt::hstring const& card,
            _In_ winrt::hstring const& toggle,
            _In_ winrt::hstring const& volume,
            _In_ winrt::hstring const& more);

    private:
        template <typename T>
        void Set(_Inout_ T& field, _In_ T const& value, _In_ wchar_t const* propertyName)
        {
            if (field == value)
            {
                return;
            }

            field = value;
            Raise(propertyName);
        }

        void Raise(_In_ wchar_t const* propertyName);

        winrt::hstring m_id{};
        winrt::hstring m_name{};
        winrt::hstring m_fileName{};
        winrt::hstring m_filePath{};
        winrt::hstring m_detailText{};
        winrt::hstring m_statusText{};
        winrt::hstring m_voicesText{};

        double m_volumeDb{ 0.0 };
        winrt::hstring m_volumeText{};

        bool m_isOn{ false };
        bool m_isBusy{ false };

        xaml::Visibility m_busyVisibility{ xaml::Visibility::Collapsed };
        xaml::Visibility m_warningVisibility{ xaml::Visibility::Collapsed };
        xaml::Visibility m_inUseVisibility{ xaml::Visibility::Collapsed };
        xaml::Visibility m_detailVisibility{ xaml::Visibility::Collapsed };

        winrt::hstring m_accessibleName{};
        winrt::hstring m_toggleAccessibleName{};
        winrt::hstring m_volumeAccessibleName{};
        winrt::hstring m_moreAccessibleName{};

        winrt::event<xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};
    };
}

namespace winrt::midisoundfontsynth::factory_implementation
{
    struct SynthItem : SynthItemT<SynthItem, implementation::SynthItem>
    {
    };
}
