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
#include "ConfigPackage.h"

#include <utility>

namespace midisettings::configengine
{
    // Old name to the name the picture is stored under here.
    using PictureRenames = std::vector<std::pair<std::wstring, std::wstring>>;

    struct StoredPictures
    {
        PictureRenames Renames{};
        uint32_t Stored{ 0 };
        uint32_t Failed{ 0 };
    };

    // Copies pictures out of a package into the shared pictures folder, through the same code
    // the picture chooser uses: it cleans names, reuses an identical file and gives a different
    // file with the same name a "(1)" suffix. Each picture goes through its own temporary folder,
    // which is removed afterwards, and the package's name is only ever the last part of that path.
    StoredPictures StorePictures(
        _In_ ConfigEnvironment const& environment,
        _In_ PackageContents const& contents,
        _In_ std::vector<std::wstring> const& names) noexcept;

    // Points every picture reference in the configuration at the stored name.
    void RenamePictures(
        _In_ json::JsonObject const& document,
        _In_ PictureRenames const& renames) noexcept;

    // The name and the full path of each picture that's stored here, for a package.
    std::vector<std::pair<std::wstring, std::wstring>> PictureFiles(
        _In_ ConfigEnvironment const& environment,
        _In_ std::vector<std::wstring> const& names) noexcept;
}
