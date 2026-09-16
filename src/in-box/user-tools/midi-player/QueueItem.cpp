// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "QueueItem.h"
#include "QueueItem.g.cpp"

#include "StringResources.h"

namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    _Use_decl_annotations_
    void QueueItem::Update(::midiplayer::QueueEntry const& entry) noexcept
    {
        try
        {
            m_id = winrt::hstring{ entry.Id };
            m_filePath = winrt::hstring{ entry.FilePath };
            m_fileName = winrt::hstring{ entry.FileName };
            m_displayName = winrt::hstring{ entry.Title };
            m_isPlayable = entry.IsPlayable();

            if (m_isPlayable)
            {
                m_durationText = winrt::hstring{ ::midiplayer::FormatDuration(entry.DurationMicroseconds) };

                m_detailText = res::FormatString(
                    L"QueueItemDetailFormat",
                    entry.TrackCount,
                    entry.NoteCount,
                    static_cast<int32_t>(entry.BeatsPerMinute + 0.5));
            }
            else
            {
                m_durationText = winrt::hstring{};

                switch (entry.Status)
                {
                case midifile::ReadStatus::FileNotFound:
                    m_detailText = res::GetString(L"QueueItemFileNotFound");
                    break;

                case midifile::ReadStatus::AccessDenied:
                    m_detailText = res::GetString(L"QueueItemAccessDenied");
                    break;

                case midifile::ReadStatus::FileTooLarge:
                    m_detailText = res::GetString(L"QueueItemFileTooLarge");
                    break;

                default:
                    m_detailText = res::GetString(L"QueueItemNotPlayable");
                    break;
                }
            }

            RaisePropertyChanged(L"Id");
            RaisePropertyChanged(L"FilePath");
            RaisePropertyChanged(L"FileName");
            RaisePropertyChanged(L"DisplayName");
            RaisePropertyChanged(L"DurationText");
            RaisePropertyChanged(L"DetailText");
            RaisePropertyChanged(L"IsPlayable");
            RaisePropertyChanged(L"WarningVisibility");
            RaisePropertyChanged(L"RemoveAccessibleName");
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update a queue row.")
    }

    void QueueItem::IsCurrent(bool value) noexcept
    {
        if (m_isCurrent == value)
        {
            return;
        }

        m_isCurrent = value;

        RaisePropertyChanged(L"IsCurrent");
        RaisePropertyChanged(L"CurrentVisibility");
    }

    winrt::hstring QueueItem::RemoveAccessibleName() const noexcept
    {
        return res::FormatString(L"QueueRemoveAccessibleNameFormat", std::wstring{ m_displayName });
    }

    _Use_decl_annotations_
    void QueueItem::RaisePropertyChanged(winrt::hstring const& name) noexcept
    {
        try
        {
            m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ name });
        }
        catch (...)
        {
        }
    }
}
