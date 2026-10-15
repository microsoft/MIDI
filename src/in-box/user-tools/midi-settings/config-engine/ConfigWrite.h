// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#pragma once

#include "ConfigEngine.h"

namespace midisettings::configengine
{
    // The whole file, shared the way the API's writer shares it, so a read never sees half of a
    // save. Sharing violations are retried for a short while, the way the API retries them.
    ConfigResult ReadWholeFile(
        _In_ std::wstring const& path,
        _In_ uint64_t maximumBytes,
        _Out_ std::string& bytes) noexcept;

    struct WriteInPlaceResult : ConfigResult
    {
        // the file already held exactly these bytes, so nothing was written
        bool Unchanged{ false };
    };

    // Writes a configuration in place. The file is held open for reading and writing, sharing
    // read only, which is the same write lock the API takes when it saves a transport section,
    // so the two never write over each other. Links and reparse points are refused.
    //
    // When expectedBytes is given, the file has to hold exactly those bytes, or nothing is
    // written and ChangedSincePlanned comes back: someone else saved in between, and the change
    // has to be planned again rather than written over theirs.
    WriteInPlaceResult WriteInPlace(
        _In_ std::wstring const& path,
        _In_ std::string const& bytes,
        _In_opt_ std::string const* expectedBytes) noexcept;

    // A file that must not already exist.
    ConfigResult WriteNewFile(
        _In_ std::wstring const& path,
        _In_ std::string const& bytes) noexcept;

    // It parses and has the transport settings object, so it's a Windows MIDI Services
    // configuration rather than some other JSON. What the service makes of the entries isn't
    // judged here.
    bool LooksLikeConfiguration(_In_ std::string_view bytes) noexcept;

    // Removes a file. The folders grant administrators delete, and nobody else.
    ConfigResult DeleteFileAt(_In_ std::wstring const& path) noexcept;
}
