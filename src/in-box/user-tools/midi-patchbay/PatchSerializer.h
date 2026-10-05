// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Reads and writes the patch file's JSON, and converts version 1 files to blocks on the way in.
// Pure: the unit tests read and write exactly what the app does. Files on disk, backups and
// error messages for people are PatchStore's job.

#include "PatchDocument.h"

namespace midipatchbay
{
    // A connection as version 1 files stored it: one hop with a filter, a transform and a send
    // speed limit built in.
    struct LegacyConnection
    {
        std::wstring Id{};
        std::wstring SourceEndpointId{};
        int32_t SourceGroupIndex{ AllGroups };
        std::wstring DestinationEndpointId{};
        int32_t DestinationGroupIndex{ AllGroups };
        bool Muted{ false };

        MessageFilter Filter{};
        MessageTransform Transform{};

        // A multiple of MIDI 1.0 wire speed, 0 for no limit.
        uint32_t SendSpeedLimit{ 0 };
    };

    // Reads a patch file's text. A version 1 file comes back converted, with LoadedFileVersion
    // set to 1 so the caller knows to keep a copy of the original. Nothing in the text is trusted:
    // everything is bounded, and a link to anything the file does not contain is left out.
    std::optional<PatchDocument> ReadPatchJson(
        _In_ std::wstring_view text,
        _In_ std::wstring const& fallbackName) noexcept;

    // Always the current version. Empty when the text could not be built.
    std::wstring WritePatchJson(_In_ PatchDocument const& patch) noexcept;

    // Rebuilds one version 1 connection as a chain of blocks that does exactly what it did: the
    // filter's parts first, then the transform's parts in the order version 1 ran them, then the
    // throttle. A connection with nothing turned on stays a plain connection. Returns false, and
    // adds nothing, when the patch has no room left.
    bool ConvertLegacyConnection(
        _Inout_ PatchDocument& patch,
        _In_ LegacyConnection const& legacy) noexcept;
}
