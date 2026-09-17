// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "midi_file_sequence.h"
#include "midi_file_smf_reader.h"

namespace midiplayer
{
    // What the queue knows about a file without holding the whole sequence in memory. A sequence
    // is only kept for the file being played; the rest of the queue is this summary, which is why
    // a long queue costs very little.
    struct QueueEntry
    {
        std::wstring Id{};
        std::wstring FilePath{};
        std::wstring FileName{};
        std::wstring Title{};

        uint64_t DurationMicroseconds{ 0 };
        uint32_t NoteCount{ 0 };
        uint32_t TrackCount{ 0 };
        uint32_t EventCount{ 0 };
        uint16_t ChannelMask{ 0 };

        midifile::ReadStatus Status{ midifile::ReadStatus::Success };
        bool Truncated{ false };

        bool IsPlayable() const noexcept { return Status == midifile::ReadStatus::Success; }
    };

    class PlayQueue
    {
    public:
        // Reads the file to fill in the summary, so this blocks and belongs on a background
        // thread. A file which cannot be read is still added, showing why, rather than
        // disappearing without explanation.
        std::wstring Add(_In_ std::wstring const& path) noexcept;

        void Remove(_In_ std::wstring const& id) noexcept;
        void Clear() noexcept;

        size_t Count() const noexcept;
        bool IsEmpty() const noexcept;

        std::vector<QueueEntry> Snapshot() const noexcept;

        std::wstring CurrentId() const noexcept;
        std::optional<QueueEntry> Current() const noexcept;

        bool SetCurrent(_In_ std::wstring const& id) noexcept;

        // Both skip over files which could not be read, so a bad file in the middle of a queue
        // does not stop playback.
        bool MoveNext(bool wrap) noexcept;
        bool MovePrevious() noexcept;

        bool HasNext(bool wrap) const noexcept;
        bool HasPrevious() const noexcept;

    private:
        // caller holds m_lock
        std::optional<size_t> IndexOfUnderLock(_In_ std::wstring const& id) const noexcept;

        mutable std::recursive_mutex m_lock{};

        std::vector<QueueEntry> m_entries{};
        std::wstring m_currentId{};
        uint64_t m_nextId{ 1 };
    };

    // "4:07" or "1:02:03". Not localized: a clock reads the same everywhere and this is the form
    // every player uses.
    std::wstring FormatDuration(uint64_t microseconds) noexcept;
}
