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
#include "ConfigInventory.h"

namespace midisettings::configengine
{
    struct ConfigFileEntry
    {
        std::wstring FileName{};

        // from the header, or the file name when the header doesn't have one
        std::wstring ConfigName{};

        FILETIME LastWritten{};
        FILETIME Created{};

        bool IsCurrent{ false };

        // it read and parsed as a configuration
        bool Readable{ false };

        ConfigInventory Inventory{};
    };

    // Every configuration in the folder, by name.
    std::vector<ConfigFileEntry> ListConfigurations(_In_ ConfigEnvironment const& environment) noexcept;

    // A file name for a new configuration, from its name, that isn't taken yet.
    std::wstring FileNameForNewConfiguration(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& configName) noexcept;

    // The name with spaces trimmed, or empty when there's nothing usable left.
    std::wstring CleanConfigName(_In_ std::wstring const& configName);

    struct CreateConfigResult : ConfigResult
    {
        std::wstring FileName{};
    };

    // A new configuration: empty, or a copy of another one with the new name. It isn't made the
    // one in use.
    CreateConfigResult CreateConfiguration(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& configName,
        _In_ std::wstring const& copyOfFileName) noexcept;

    // The same, from a configuration's bytes, for an import kept apart as a new configuration.
    CreateConfigResult CreateConfigurationFromBytes(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& configName,
        _In_ std::string const& configurationBytes) noexcept;

    // Changes the name in the configuration. The file keeps its name, so nothing that points at
    // the file has to change with it.
    ConfigResult RenameConfiguration(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& fileName,
        _In_ std::wstring const& configName) noexcept;

    // Makes it the one the MIDI service uses the next time it starts.
    ConfigResult SwitchConfiguration(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& fileName) noexcept;

    // Removes a configuration that isn't in use, after a full backup of it. When the backup
    // can't be made, nothing is deleted.
    ConfigResult DeleteConfiguration(
        _In_ ConfigEnvironment const& environment,
        _In_ std::wstring const& fileName) noexcept;
}
