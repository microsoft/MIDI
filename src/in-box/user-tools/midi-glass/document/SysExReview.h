// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.
//
// System exclusive from a layout someone else made. A sequence step or a control can hold any
// SysEx at all, and the wrong SysEx can change or damage a device. So a layout that came from
// outside this PC (a pack, an old .zip package, a sync from another PC, or a file opened from
// anywhere but the layouts folder) is held for review. The first time it runs, the app names the
// devices it would send SysEx to and asks. Once the person says yes, it doesn't ask again for
// that layout until its SysEx changes. Saying no runs the layout with SysEx held back.
//
// The list is kept in %LOCALAPPDATA%\Microsoft\MIDI Glass\SysExReview.json, never in the layout
// file, because a layout is what can't be trusted to say it's safe, and because changing it would
// break the signature on a signed layout.

#include <sal.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // UMP message types 3 (SysEx7) and 5 (SysEx8 and Mixed Data Set).
    bool IsSystemExclusivePacket(_In_ uint32_t firstWord) noexcept;

    // How many words a UMP is, from its first word's message type.
    uint32_t UmpWordCount(_In_ uint32_t firstWord) noexcept;

    // The words with every SysEx packet taken out, the rest in order. A partial packet at the end
    // is dropped too.
    std::vector<uint32_t> WithoutSystemExclusive(_In_reads_(wordCount) uint32_t const* words, _In_ uint32_t wordCount);

    bool ContainsSystemExclusive(_In_reads_(wordCount) uint32_t const* words, _In_ uint32_t wordCount) noexcept;

    // The layout's own device names that a control or a sequence step sends SysEx to, each
    // once, in the order they first appear. A row with no device is listed as an empty name.
    std::vector<std::wstring> SystemExclusiveDevices(_In_ LayoutDocument const& document);

    // SHA-256 over every SysEx the layout can send and where it goes, in file order. Empty when
    // it sends none. Approving one fingerprint says nothing about a different one.
    std::wstring SystemExclusiveFingerprint(_In_ LayoutDocument const& document);

    // Marks a layout file as having come from somewhere else. Keeps any earlier approval: if the
    // new copy's SysEx is the same, it won't ask again.
    void MarkForSysExReview(_In_ std::wstring const& filePath) noexcept;

    void ForgetSysExReview(_In_ std::wstring const& filePath) noexcept;

    // A layout renamed or copied keeps what's known about it.
    void MoveSysExReview(_In_ std::wstring const& fromPath, _In_ std::wstring const& toPath) noexcept;
    void CopySysExReview(_In_ std::wstring const& fromPath, _In_ std::wstring const& toPath) noexcept;

    enum class SysExReview
    {
        // Made here, or it sends no SysEx, or the person already said yes to exactly this SysEx.
        Allowed,

        // Ask first, and hold SysEx back until the answer is yes.
        Ask,
    };

    // `document.IsImported` (a file outside the layouts folder) counts as marked. A marked
    // layout with no SysEx, or whose SysEx was already approved, has its mark cleared here.
    SysExReview CheckSysExReview(_In_ LayoutDocument const& document, _In_ std::wstring const& filePath) noexcept;

    // The person said yes to this layout's SysEx as it is now.
    void ApproveSysEx(_In_ LayoutDocument const& document, _In_ std::wstring const& filePath) noexcept;

    // Where the list is kept. Tests point it at a folder of their own.
    void UseSysExReviewFile(_In_ std::wstring const& path) noexcept;
}
