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

// Carries out an import plan: backs up the configuration in use, copies the pictures in, merges,
// writes the configuration file in place and sets the port names. The MIDI service reads the file
// when it next starts, so nothing here restarts it.

#include "ConfigBackups.h"
#include "ConfigPictures.h"
#include "ImportPlan.h"

#include <optional>

namespace midisettings::configengine
{
    // One line on the Done page each. The window words them.
    enum class ResultCode
    {
        // Count added, Count2 pictures copied in, Count3 of yours kept, Count4 of yours replaced,
        // Count5 of yours removed
        DeviceNames,
        DeviceNamesNotImported,

        // Name, and NewName when it was added under a free name
        LoopbackAdded,
        LoopbackAddedAs,
        LoopbackLeftOut,
        LoopbacksNotImported,

        // Network, Name, and NewName when it was renamed
        HostAdded,
        HostAddedAsThisPc,
        HostAddedAs,
        HostKeptOtherPcName,
        HostLeftOut,
        ClientAdded,
        NetworkNotImported,

        // Count hosts and connections, kept for when RTP-MIDI is installed
        RtpSavedForLater,

        // Count
        BluetoothAdded,
        BluetoothNotImported,

        // Name is the mode as written: "compatible" or "modern"
        SynthModeSet,
        PortNamingSet,
        PortNamingNotUsable,
        PortNamingFailed,
        OtherSettingsKept,

        // Name is the transport's ID
        OtherSectionAdded,

        // Count
        PicturesFailed,
    };

    enum class ResultTone
    {
        // it's in the configuration
        Done,

        // it was left out, or needs extra care
        Warning,

        // it's waiting for something else
        Waiting,

        // the customer chose to leave it out
        Neutral,
    };

    struct ResultLine
    {
        ResultCode Code{ ResultCode::DeviceNames };
        ResultTone Tone{ ResultTone::Done };
        NetworkKind Network{ NetworkKind::NetworkMidi2 };
        std::wstring Name{};
        std::wstring NewName{};
        uint32_t Count{ 0 };
        uint32_t Count2{ 0 };
        uint32_t Count3{ 0 };
        uint32_t Count4{ 0 };
        uint32_t Count5{ 0 };
    };

    struct ImportResult : ConfigResult
    {
        ImportTarget Target{ ImportTarget::InUse };

        // the configuration it went into
        std::wstring ConfigFileName{};
        std::wstring ConfigName{};

        // the full backup made first, which Undo this import puts back
        std::optional<BackupEntry> Backup{};

        bool PortNamingChanged{ false };

        std::vector<ResultLine> Lines{};
    };

    // The pictures the items being imported use.
    std::vector<std::wstring> PicturesToImport(_In_ ImportPlan const& plan, _In_ ImportChoices const& choices);

    // The merged configuration, built in memory without writing anything. Null when the plan's
    // configuration or the package's can't be read.
    json::JsonObject MergeImport(
        _In_ ImportPlan const& plan,
        _In_ PackageContents const& contents,
        _In_ ImportChoices const& choices,
        _In_ PictureRenames const& renames,
        _Inout_ std::vector<ResultLine>& lines) noexcept;

    // Backs up the configuration in use, copies the pictures in, merges, checks the result and
    // writes it in place. When the configuration changed after the plan was made, nothing is
    // written and ChangedSincePlanned comes back, so the import can be planned again.
    ImportResult ApplyImport(
        _In_ ConfigEnvironment const& environment,
        _In_ PackageContents const& contents,
        _In_ ImportPlan const& plan,
        _In_ ImportChoices const& choices) noexcept;
}
