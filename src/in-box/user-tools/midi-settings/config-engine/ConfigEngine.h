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

// The configuration engine behind the MIDI Settings Configurations window. Plain C++ and
// Windows.Data.Json, with no XAML, no app strings and no precompiled header, so it can be
// compiled into any tool and into the unit tests exactly as it ships. It returns data and reason
// codes, and the window turns those into words.

#include <windows.h>
#include <sal.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace midisettings::configengine
{
    namespace json = ::winrt::Windows::Data::Json;

    inline constexpr std::wstring_view ConfigFileSuffix{ L".midiconfig.json" };
    inline constexpr std::wstring_view PackageFileSuffix{ L".midiconfigpack" };
    inline constexpr std::wstring_view BackupFileSuffix{ L".bak" };

    // Far larger than any real configuration, and still small enough to read in one go.
    inline constexpr uint64_t MaximumConfigBytes{ 16 * 1024 * 1024 };

    // The port names setting for older apps. A package stores it by name, never as a number.
    enum class PortNaming
    {
        None,
        ClassicCompatible,
        NewStyle,
        Automatic,
    };

    // Where the files are, and how this PC is read and changed. The app fills this in from the
    // real system. The tests point it at a temporary folder and fake the rest, so nothing they do
    // reaches the registry or the configurations in use.
    struct ConfigEnvironment
    {
        // each with a trailing separator
        std::wstring ConfigFolder{};
        std::wstring BackupFolder{};
        std::wstring TemporaryFolder{};

        // shown, and used to spot network hosts named after the PC a package came from
        std::wstring ComputerName{};
        std::wstring AppVersion{};

        // the bare file name of the configuration in use, empty when there is none
        std::function<std::wstring()> ReadCurrentFileName{};
        std::function<bool(std::wstring const&)> WriteCurrentFileName{};

        std::function<PortNaming()> ReadPortNaming{};
        std::function<bool(PortNaming)> WritePortNaming{};

        // the service on this PC can use the automatic port names
        bool AutomaticPortNamingAvailable{ true };

        // Full path of a stored picture, or empty when there is no such picture.
        std::function<std::wstring(std::wstring const&)> PicturePath{};

        // Copies a picture into the shared pictures folder and returns the name it's stored under,
        // which may differ from the source file's name. Empty on failure.
        std::function<std::wstring(std::wstring const&)> StorePicture{};
    };

    enum class ConfigError
    {
        None,
        NoFolder,
        NameNotUsable,
        NameTaken,
        NotFound,
        Empty,
        TooLarge,
        NotAConfiguration,
        NotAPackage,
        NewerPackage,
        Damaged,
        InUse,
        ChangedSincePlanned,
        BackupFailed,
        ReadFailed,
        WriteFailed,
        LinkRefused,
        RegistryFailed,
        DeleteFailed,
        Unexpected,
    };

    struct ConfigResult
    {
        ConfigError Error{ ConfigError::None };

        // the Win32 error behind the failure, if there is one
        DWORD SystemError{ ERROR_SUCCESS };

        bool Succeeded() const noexcept { return Error == ConfigError::None; }
    };

    // Ordinal, ignoring case, which is how Windows compares file names and how the service
    // compares the IDs in a configuration.
    bool SameText(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept;

    bool EndsWithText(_In_ std::wstring_view text, _In_ std::wstring_view suffix) noexcept;

    // Found, or replaced everywhere, without regard to case.
    bool ContainsText(_In_ std::wstring_view text, _In_ std::wstring_view part) noexcept;
    std::wstring ReplaceText(_In_ std::wstring const& text, _In_ std::wstring_view part, _In_ std::wstring_view replacement);

    // A bare name, with no folder in it, that the file system accepts.
    bool IsUsableFileName(_In_ std::wstring_view name) noexcept;

    // The names a package stores: classicCompatible, newStyle and automatic.
    std::wstring PortNamingName(_In_ PortNaming naming);
    PortNaming PortNamingFromName(_In_ std::wstring_view name) noexcept;

    // A file time as UTC text, 2026-10-14T19:20:31Z, and back.
    std::wstring FileTimeToUtcText(_In_ FILETIME const& time);
    bool TryParseUtcText(_In_ std::wstring_view text, _Out_ FILETIME& time) noexcept;

    FILETIME CurrentFileTime() noexcept;
    uint64_t FileTimeValue(_In_ FILETIME const& time) noexcept;
}
