// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // Values of the UseLegacyMidi entry in Drivers32. See
    // https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/
    enum class ApiMode : uint32_t
    {
        WindowsMidiServices = 0,
        Legacy = 1,
        Hybrid = 2
    };

    enum class EntrySeverity
    {
        Ok = 0,
        Warning,
        Error
    };

    struct RegistryEntryInfo
    {
        std::wstring Name{};
        std::wstring Value{};
        std::wstring Comment{};
        EntrySeverity Severity{ EntrySeverity::Ok };
    };

    struct TransportRegistrationInfo
    {
        std::wstring Name{};
        std::wstring ClassId{};
        std::wstring ModulePath{};
        std::wstring ModuleVersion{};

        bool Enabled{ true };
        bool EnabledValuePresent{ false };
        bool ModuleRegistered{ false };
        bool ModuleFileFound{ false };

        // set for the two transports the service hosts itself, which have no registry entry
        // under the MIDI root
        bool BuiltIn{ false };
    };

    // What a repair would change. Built by the scan so the customer sees the list before
    // anything is written, exactly like the console tool does.
    struct RegistryRepairPlan
    {
        bool SetMidiValue64{ false };
        bool SetMidi1Value64{ false };
        bool SetMidiValueWow{ false };
        bool SetMidi1ValueWow{ false };
        bool SetTransferComplete{ false };

        std::vector<std::wstring> DeleteValues64{};
        std::vector<std::wstring> DeleteValuesWow{};

        // one localized line per proposed change, for the confirmation dialog
        std::vector<std::wstring> Descriptions{};

        bool AnyChanges() const noexcept
        {
            return SetMidiValue64 || SetMidi1Value64 || SetMidiValueWow || SetMidi1ValueWow ||
                SetTransferComplete || !DeleteValues64.empty() || !DeleteValuesWow.empty();
        }
    };

    struct RegistryScan
    {
        ApiMode Mode{ ApiMode::WindowsMidiServices };
        bool ApiModeValuePresent{ false };

        // Legacy mode does not use midisrv or wdmaud2.drv at all, so "repairing" the entries
        // to the Windows MIDI Services layout would be wrong rather than helpful.
        bool LegacyMode{ false };

        bool Drivers32KeyOpened{ false };
        bool Drivers32WowKeyOpened{ false };

        std::vector<RegistryEntryInfo> Drivers32Entries{};
        std::vector<RegistryEntryInfo> Drivers32WowEntries{};
        std::vector<RegistryEntryInfo> ServiceRootEntries{};
        std::vector<TransportRegistrationInfo> Transports{};

        RegistryRepairPlan Plan{};
    };

    struct RepairResult
    {
        bool Succeeded{ false };
        std::vector<std::wstring> Messages{};
    };

    ApiMode GetCurrentApiMode() noexcept;
    bool TrySetApiMode(ApiMode mode) noexcept;

    // Blocking registry work; callers run it from a background thread.
    RegistryScan ScanRegistry() noexcept;

    // Just the transports, for the service health page.
    std::vector<TransportRegistrationInfo> ScanRegisteredTransports() noexcept;

    RepairResult ApplyRegistryRepair(RegistryRepairPlan const& plan) noexcept;

    // Requires the shutdown privilege, which an elevated process has.
    bool TryRestartComputer() noexcept;
}
