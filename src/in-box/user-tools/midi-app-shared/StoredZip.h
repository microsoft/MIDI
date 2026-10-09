// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The small part of zip these tools use: stored entries only, no compression, encryption or
// zip64, so no decompressor ever sees a stranger's file.

#include <sal.h>
#include <cstdint>
#include <string>
#include <vector>

namespace midiapp
{
    struct StoredZipEntry
    {
        std::wstring Name{};
        std::vector<uint8_t> Bytes{};
    };

    // UTF-8 names and no timestamps, so the same files always give the same bytes.
    std::vector<uint8_t> BuildStoredZip(_In_ std::vector<StoredZipEntry> const& entries) noexcept;

    enum class StoredZipStatus
    {
        Read,
        NotAZip,
        Compressed,
        TooManyEntries,
        Damaged,

        // The central directory, which other tools show, doesn't list what the local headers hold.
        DirectoryMismatch,
    };

    StoredZipStatus ReadStoredZip(
        _In_ std::vector<uint8_t> const& zip,
        _In_ uint32_t maximumEntries,
        _Out_ std::vector<StoredZipEntry>& entries) noexcept;

    uint32_t ComputeCrc32(_In_reads_bytes_(size) uint8_t const* data, _In_ size_t size) noexcept;
}
