// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "QueueItem.g.h"
#include "PlayQueue.h"

namespace winrt::midiplayer::implementation
{
    struct QueueItem : QueueItemT<QueueItem>
    {
        QueueItem() = default;

        void Update(_In_ ::midiplayer::QueueEntry const& entry) noexcept;

        winrt::hstring Id() const noexcept { return m_id; }
        winrt::hstring FilePath() const noexcept { return m_filePath; }
        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring FileName() const noexcept { return m_fileName; }
        winrt::hstring DurationText() const noexcept { return m_durationText; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }

        bool IsCurrent() const noexcept { return m_isCurrent; }
        void IsCurrent(bool value) noexcept;

        bool IsPlayable() const noexcept { return m_isPlayable; }

        xaml::Visibility CurrentVisibility() const noexcept
        {
            return m_isCurrent ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility WarningVisibility() const noexcept
        {
            return m_isPlayable ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        winrt::hstring RemoveAccessibleName() const noexcept;

        winrt::event_token PropertyChanged(xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        void RaisePropertyChanged(_In_ winrt::hstring const& name) noexcept;

        winrt::hstring m_id{};
        winrt::hstring m_filePath{};
        winrt::hstring m_displayName{};
        winrt::hstring m_fileName{};
        winrt::hstring m_durationText{};
        winrt::hstring m_detailText{};

        bool m_isCurrent{ false };
        bool m_isPlayable{ true };

        winrt::event<xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};
    };
}

namespace winrt::midiplayer::factory_implementation
{
    struct QueueItem : QueueItemT<QueueItem, implementation::QueueItem>
    {
    };
}
