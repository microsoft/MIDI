// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// What the receiving PC remembers about syncs.
//
// SessionApprovals: which layouts the person here has already said yes to from which paired PC,
// since Glass started. Held in memory only, so it ends when Glass closes. Unpairing a PC forgets
// its approvals at once. There is deliberately no lasting "always accept".
//
// SyncRecords: for each layout and sender, the file that was installed and its SHA-256, kept
// under %LOCALAPPDATA%. The next sync compares the file with it, so a copy changed on this PC is
// never replaced without saying so. Pch-free.

#include <sal.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace glass::sync
{
    // What identifies "the same layout" across syncs: its provenance id, or for a layout made
    // before layouts had one, the file name in the pack.
    std::wstring SyncLayoutKey(_In_ std::wstring const& layoutId, _In_ std::wstring const& primaryFileName);

    class SessionApprovals
    {
    public:
        static SessionApprovals& Current() noexcept;

        bool IsApproved(_In_ std::wstring const& layoutKey, _In_ std::wstring const& senderKey) const noexcept;
        void Approve(_In_ std::wstring const& layoutKey, _In_ std::wstring const& senderKey) noexcept;
        void ForgetSender(_In_ std::wstring const& senderKey) noexcept;
        void Clear() noexcept;

    private:
        mutable std::mutex m_lock{};
        std::set<std::pair<std::wstring, std::wstring>> m_approved{};
    };

    struct SyncRecord
    {
        std::wstring LayoutKey{};

        // The sender's device key hash, as hex.
        std::wstring SenderKey{};

        std::wstring InstalledPath{};
        std::wstring InstalledSha256{};
    };

    class SyncRecords
    {
    public:
        explicit SyncRecords(_In_ std::wstring filePath) noexcept : m_filePath(std::move(filePath)) {}

        std::optional<SyncRecord> Find(_In_ std::wstring const& layoutKey, _In_ std::wstring const& senderKey) const noexcept;

        bool Remember(_In_ SyncRecord const& record) noexcept;

        bool ForgetSender(_In_ std::wstring const& senderKey) noexcept;

    private:
        std::vector<SyncRecord> Load() const;
        bool Save(_In_ std::vector<SyncRecord> const& records) const noexcept;

        std::wstring m_filePath{};
    };

    enum class LocalCopyState
    {
        // This sender has never installed it here, so there's nothing to compare with.
        Unknown,

        // Exactly what the last sync from this sender installed.
        Unchanged,

        // Changed on this PC since, or another file now has its place.
        ChangedHere,
    };

    LocalCopyState CheckLocalCopy(
        _In_ SyncRecords const& records,
        _In_ std::wstring const& layoutKey,
        _In_ std::wstring const& senderKey,
        _In_ std::wstring const& existingPath) noexcept;
}
