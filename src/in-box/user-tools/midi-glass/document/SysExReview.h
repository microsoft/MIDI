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
// SysEx at all, and the wrong SysEx can change or damage a device. So SysEx is held back unless
// this PC has seen exactly that SysEx before: written in the editor here, or allowed when asked.
// The first time a layout with other SysEx runs, the app names the devices it would go to and
// asks. Saying no runs the layout with SysEx held back, and it asks again next time.
//
// It's an allow list of fingerprints, kept in %LOCALAPPDATA%\Microsoft\MIDI Glass\SysExReview.json.
// Nothing is written into the layout file, so a layout can't vouch for itself and a signed one
// stays signed. A missing or damaged list fails safe: everything with SysEx asks again. The list
// is the signed-in user's own, so it guards against a layout file, not against a program already
// running as that user, which could send SysEx itself.

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

    enum class SysExReview
    {
        // It sends no SysEx, or exactly this SysEx was written on this PC or allowed before.
        Allowed,

        // Ask first, and hold SysEx back until the answer is yes.
        Ask,
    };

    // Whether this layout's SysEx is on the list. Where the file is and how it arrived don't
    // matter: a layout dropped into the layouts folder by Explorer or OneDrive asks like any other.
    // When the list can't be read, everything with SysEx asks.
    SysExReview CheckSysExReview(_In_ LayoutDocument const& document) noexcept;

    // Puts this layout's SysEx on the list: the person allowed it, or wrote it in the editor.
    void TrustSysEx(_In_ LayoutDocument const& document) noexcept;

    // Where the list is kept. Tests point it at a folder of their own.
    void UseSysExReviewFile(_In_ std::wstring const& path) noexcept;
}
