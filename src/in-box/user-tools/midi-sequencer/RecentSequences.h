// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sequences opened or saved on this PC, newest first, and the ones pinned so they never age
// out of the list. The list lives in the PC's settings, never in the sequence files, so pinning a
// sequence doesn't change it. No XAML here, so the tests compile it unchanged.

#include <sal.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace midisequencer
{
    struct RecentSequence
    {
        std::wstring Path{};

        // When it was last opened or saved, in seconds since 1970, UTC.
        int64_t UsedAt{ 0 };

        bool Pinned{ false };

        bool operator==(RecentSequence const&) const = default;
    };

    // How many sequences that aren't pinned the list keeps. The oldest go first.
    inline constexpr size_t MaximumRecentSequences = 100;

    // Pinned ones never age out, so they have a limit of their own.
    inline constexpr size_t MaximumPinnedSequences = 100;

    // Longer paths aren't kept, so a damaged setting can't grow without end.
    inline constexpr size_t MaximumRecentPathLength = 2048;

    // The same file: case doesn't matter, and / is the same as \.
    bool SameSequencePath(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept;

    // Puts the sequence at the top as just used, keeping its pin. The oldest sequences that
    // aren't pinned go when there are more than MaximumRecentSequences of them.
    void RememberSequence(_Inout_ std::vector<RecentSequence>& list, _In_ std::wstring const& path, _In_ int64_t now);

    // False when the sequence isn't in the list, or when as many as allowed are pinned already.
    bool PinSequence(_Inout_ std::vector<RecentSequence>& list, _In_ std::wstring_view path, _In_ bool pinned);

    void ForgetSequence(_Inout_ std::vector<RecentSequence>& list, _In_ std::wstring_view path);

    // Pinned first, then the rest, newest first in each.
    std::vector<RecentSequence> RecentSequencesForDisplay(_In_ std::vector<RecentSequence> const& list);

    // Anything that isn't a sequence entry is skipped. A sequence listed twice keeps its newest
    // entry, pinned if either was.
    std::vector<RecentSequence> ReadRecentSequences(_In_ std::wstring const& json) noexcept;
    std::wstring WriteRecentSequences(_In_ std::vector<RecentSequence> const& list);
}
