// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PlayQueue.h"

namespace midiplayer
{
    namespace
    {
        std::wstring FileNameOf(std::wstring const& path) noexcept
        {
            try
            {
                return std::filesystem::path{ path }.filename().wstring();
            }
            catch (...)
            {
                return path;
            }
        }

        std::wstring StemOf(std::wstring const& path) noexcept
        {
            try
            {
                return std::filesystem::path{ path }.stem().wstring();
            }
            catch (...)
            {
                return path;
            }
        }

        std::wstring WidenUtf8(std::string const& value) noexcept
        {
            if (value.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), required);

            return wide;
        }
    }

    _Use_decl_annotations_
    std::wstring PlayQueue::Add(std::wstring const& path) noexcept
    {
        QueueEntry entry{};

        entry.FilePath = path;
        entry.FileName = FileNameOf(path);

        midifile::MidiSequence sequence{};
        auto const result = midifile::ReadStandardMidiFile(path, sequence);

        entry.Status = result.Status;
        entry.Truncated = result.Truncated;

        if (result.Succeeded())
        {
            entry.DurationMicroseconds = sequence.DurationMicroseconds;
            entry.NoteCount = static_cast<uint32_t>(sequence.Notes.size());
            entry.TrackCount = static_cast<uint32_t>(sequence.Tracks.size());
            entry.EventCount = static_cast<uint32_t>(sequence.Events.size());
            entry.BeatsPerMinute = sequence.BeatsPerMinuteAtTick(0);
            entry.ChannelMask = sequence.UsedChannelMask;
            entry.Title = WidenUtf8(sequence.Title);
        }

        if (entry.Title.empty())
        {
            entry.Title = StemOf(path);
        }

        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        entry.Id = std::to_wstring(m_nextId++);

        auto const id = entry.Id;

        m_entries.push_back(std::move(entry));

        if (m_currentId.empty())
        {
            m_currentId = id;
        }

        return id;
    }

    _Use_decl_annotations_
    void PlayQueue::Remove(std::wstring const& id) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        auto const index = IndexOfUnderLock(id);

        if (!index.has_value())
        {
            return;
        }

        auto const wasCurrent = m_currentId == id;

        m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(index.value()));

        if (!wasCurrent)
        {
            return;
        }

        // The row that takes the removed one's place becomes current, so the queue keeps moving
        // in the direction the customer was going.
        if (m_entries.empty())
        {
            m_currentId.clear();
        }
        else
        {
            auto const next = index.value() < m_entries.size() ? index.value() : m_entries.size() - 1;
            m_currentId = m_entries[next].Id;
        }
    }

    void PlayQueue::Clear() noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        m_entries.clear();
        m_currentId.clear();
    }

    size_t PlayQueue::Count() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_entries.size();
    }

    bool PlayQueue::IsEmpty() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_entries.empty();
    }

    std::vector<QueueEntry> PlayQueue::Snapshot() const noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };
            return m_entries;
        }
        catch (...)
        {
            return {};
        }
    }

    std::wstring PlayQueue::CurrentId() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };
        return m_currentId;
    }

    std::optional<QueueEntry> PlayQueue::Current() const noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            auto const index = IndexOfUnderLock(m_currentId);

            if (!index.has_value())
            {
                return std::nullopt;
            }

            return m_entries[index.value()];
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    bool PlayQueue::SetCurrent(std::wstring const& id) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        if (!IndexOfUnderLock(id).has_value())
        {
            return false;
        }

        m_currentId = id;
        return true;
    }

    bool PlayQueue::MoveNext(bool wrap) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        if (m_entries.empty())
        {
            return false;
        }

        auto const current = IndexOfUnderLock(m_currentId);
        auto const start = current.value_or(0);

        for (size_t step = 1; step <= m_entries.size(); ++step)
        {
            auto const candidate = start + step;

            if (candidate >= m_entries.size() && !wrap)
            {
                return false;
            }

            auto const index = candidate % m_entries.size();

            if (index == start && step > 1)
            {
                return false;
            }

            if (m_entries[index].IsPlayable())
            {
                m_currentId = m_entries[index].Id;
                return true;
            }
        }

        return false;
    }

    bool PlayQueue::MovePrevious() noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        if (m_entries.empty())
        {
            return false;
        }

        auto const current = IndexOfUnderLock(m_currentId);

        if (!current.has_value())
        {
            return false;
        }

        for (size_t index = current.value(); index > 0; --index)
        {
            if (m_entries[index - 1].IsPlayable())
            {
                m_currentId = m_entries[index - 1].Id;
                return true;
            }
        }

        return false;
    }

    bool PlayQueue::HasNext(bool wrap) const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        auto const current = IndexOfUnderLock(m_currentId);

        if (!current.has_value() || m_entries.empty())
        {
            return false;
        }

        for (size_t index = current.value() + 1; index < m_entries.size(); ++index)
        {
            if (m_entries[index].IsPlayable())
            {
                return true;
            }
        }

        if (!wrap)
        {
            return false;
        }

        for (size_t index = 0; index < current.value(); ++index)
        {
            if (m_entries[index].IsPlayable())
            {
                return true;
            }
        }

        return false;
    }

    bool PlayQueue::HasPrevious() const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        auto const current = IndexOfUnderLock(m_currentId);

        if (!current.has_value())
        {
            return false;
        }

        for (size_t index = current.value(); index > 0; --index)
        {
            if (m_entries[index - 1].IsPlayable())
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    std::optional<size_t> PlayQueue::IndexOfUnderLock(std::wstring const& id) const noexcept
    {
        if (id.empty())
        {
            return std::nullopt;
        }

        for (size_t index = 0; index < m_entries.size(); ++index)
        {
            if (m_entries[index].Id == id)
            {
                return index;
            }
        }

        return std::nullopt;
    }

    std::wstring FormatDuration(uint64_t microseconds) noexcept
    {
        try
        {
            auto const totalSeconds = microseconds / 1000000;

            auto const hours = totalSeconds / 3600;
            auto const minutes = (totalSeconds % 3600) / 60;
            auto const seconds = totalSeconds % 60;

            if (hours > 0)
            {
                return std::format(L"{}:{:02}:{:02}", hours, minutes, seconds);
            }

            return std::format(L"{}:{:02}", minutes, seconds);
        }
        catch (...)
        {
            return L"0:00";
        }
    }
}
