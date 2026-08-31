// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisettings
{
    // Values match Midi1PortNameSelection in MidiEndpointNameTable.h and the SDK's
    // Midi1PortNamingApproach, which is what the service reads out of the registry.
    enum class Midi1PortNaming : uint32_t
    {
        UseGlobalDefault = 0,
        ClassicCompatible = 1,
        NewStyle = 2
    };

    struct ConfigFileInfo
    {
        std::wstring FileName{};        // bare name, which is all the registry holds
        std::wstring FullPath{};
        std::wstring ConfigName{};      // "configName" from the file header
        bool IsCurrent{ false };
    };

    // Everything the customer can change here is machine wide, so a write needs administrator
    // rights. Each call reports failure through errorMessage rather than throwing, and the
    // caller turns that into a banner offering to relaunch elevated.
    namespace config
    {
        // %ALLUSERSPROFILE%\Microsoft\MIDI\, expanded, with a trailing separator.
        std::wstring FolderPath() noexcept;

        // Bare file name from HKLM, empty when the machine has never been set up.
        std::wstring CurrentFileName() noexcept;

        // Full path of the current file, empty when there is no current file or it is missing.
        // This is the "has the customer completed setup" test the invitation banner uses.
        std::wstring CurrentFullPath() noexcept;

        std::vector<ConfigFileInfo> EnumerateFiles() noexcept;

        // The display name inside the file, falling back to the file name.
        std::wstring ReadConfigName(std::wstring const& fullPath) noexcept;

        bool SetCurrentFileName(std::wstring const& fileName, std::wstring& errorMessage) noexcept;

        // Writes a new, empty configuration file. Fails rather than overwriting an existing one.
        bool CreateFile(
            std::wstring const& configName,
            std::wstring const& fileName,
            std::wstring& errorMessage) noexcept;

        // Strips the characters a file name cannot carry and appends the required suffix.
        std::wstring FileNameFromConfigName(std::wstring const& configName) noexcept;

        bool CopyCurrentFileTo(std::wstring const& destinationPath, std::wstring& errorMessage) noexcept;

        Midi1PortNaming DefaultMidi1PortNaming() noexcept;

        bool SetDefaultMidi1PortNaming(Midi1PortNaming const value, std::wstring& errorMessage) noexcept;

        // Stop and start midisrv. Blocking, so callers run it through BackgroundWork.
        bool RestartService(std::wstring& errorMessage) noexcept;
    }
}
