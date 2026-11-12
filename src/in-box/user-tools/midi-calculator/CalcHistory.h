// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Answers the customer copied, used as a session value or pinned. Kept in
// %LOCALAPPDATA%\Microsoft\MIDI\Calculator\history.json so they survive a restart. The file is
// the calculator's own; nothing else reads it.

namespace midicalculator
{
    struct HistoryEntry
    {
        std::wstring Page{};
        std::wstring Question{};
        std::wstring Answer{};
        int64_t FileTime{ 0 };
        bool Pinned{ false };
    };

    class History
    {
    public:
        // The newest answers kept, not counting pinned ones, which stay until unpinned.
        static constexpr size_t MaximumEntries = 500;

        static History& Current() noexcept;

        void Load() noexcept;

        // Newest first. The same question and answer as the newest entry moves it to the top
        // rather than adding it again.
        void Add(_In_ std::wstring const& page, _In_ std::wstring const& question, _In_ std::wstring const& answer) noexcept;
        void TogglePin(_In_ size_t index) noexcept;
        void Remove(_In_ size_t index) noexcept;

        // Clears everything but the pinned entries.
        void Clear() noexcept;

        std::vector<HistoryEntry> const& Entries() const noexcept { return m_entries; }

        // Called after every change, so the History page can refresh.
        void Changed(_In_ std::function<void()> const& handler) noexcept;

    private:
        void Trim() noexcept;
        void Save() noexcept;
        void Notify() noexcept;

        std::vector<HistoryEntry> m_entries{};
        std::function<void()> m_changed{};
    };
}
