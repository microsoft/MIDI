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

// Packages that set up another PC the same way.

#include "ConfigBackups.h"
#include "ConfigInventory.h"

namespace midisettings::configengine
{
    struct ShareResult : ConfigResult
    {
        uint32_t Pictures{ 0 };

        // pictures the configuration names that aren't on this PC any more
        uint32_t PicturesMissing{ 0 };
    };

    // The parts that can go into a package of this configuration. Other settings is there whenever
    // this PC has a port names setting, because that lives outside the configuration file.
    uint32_t ShareableParts(_In_ ConfigEnvironment const& environment, _In_ ConfigInventory const& inventory) noexcept;

    // A package of a configuration with the parts chosen, the pictures they use, and the port
    // names setting when Other settings is chosen. The package name and the note show on the
    // other PC when it's imported.
    ShareResult ShareConfiguration(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& configFileName,
        _In_ uint32_t parts,
        _In_ std::wstring const& packageName,
        _In_ std::wstring const& note,
        _In_ std::wstring const& packagePath) noexcept;

    // A package of a backup. A full backup is already a package, so it's copied as it is. A
    // configuration-only backup gets the pictures it names that are still on this PC.
    ShareResult ShareBackup(
        _In_ ConfigEnvironment const& environment,
        _In_ BackupEntry const& backup,
        _In_ std::wstring const& packagePath) noexcept;
}
