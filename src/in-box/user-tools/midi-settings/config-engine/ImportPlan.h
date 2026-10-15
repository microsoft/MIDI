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

// Compares a package with this PC and works out, for every item in it, what importing it would
// do. Nothing here changes anything. When a package disagrees with this PC, the customer's own
// version wins by default.

#include "ConfigEngine.h"
#include "ConfigInventory.h"
#include "ConfigPackage.h"

namespace midisettings::configengine
{
    enum class ImportTarget
    {
        // merged into the configuration in use
        InUse,

        // kept apart as a configuration of its own
        NewConfiguration,
    };

    enum class DeviceNamesChoice
    {
        AddToMine,
        ReplaceMine,
        DontImport,
    };

    // for devices that both this PC and the package have names for
    enum class SharedDevicesChoice
    {
        KeepMine,
        UseThePackages,
    };

    enum class PartChoice
    {
        Add,
        DontImport,
    };

    enum class OtherSettingsChoice
    {
        KeepMine,
        UseThePackages,
    };

    struct ImportChoices
    {
        ImportTarget Target{ ImportTarget::InUse };
        DeviceNamesChoice DeviceNames{ DeviceNamesChoice::AddToMine };
        SharedDevicesChoice SharedDevices{ SharedDevicesChoice::KeepMine };
        PartChoice Loopbacks{ PartChoice::Add };
        PartChoice NetworkMidi2{ PartChoice::Add };
        PartChoice RtpMidi{ PartChoice::Add };
        PartChoice Bluetooth{ PartChoice::Add };
        OtherSettingsChoice OtherSettings{ OtherSettingsChoice::KeepMine };

        // the "Add as Max 2" boxes that are ticked, by item ID
        std::vector<std::wstring> AddUnderFreeName{};

        // the "Use this PC's name" boxes that are NOT ticked, by item ID. They start ticked.
        std::vector<std::wstring> KeepOtherPcName{};

        bool IsAddedUnderFreeName(_In_ std::wstring const& itemId) const noexcept;
        bool KeepsOtherPcName(_In_ std::wstring const& itemId) const noexcept;
        void SetAddedUnderFreeName(_In_ std::wstring const& itemId, _In_ bool ticked);
        void SetUseThisPcName(_In_ std::wstring const& itemId, _In_ bool ticked);
    };

    enum class ItemKind
    {
        DeviceName,
        Loopback,
        Host,
        Client,
        BluetoothDevice,
        OtherSection,
    };

    // What an item is, compared with this PC.
    enum class ItemStatus
    {
        New,

        // here already, the same
        AlreadyHere,

        // a device that both this PC and the package have names for
        InBoth,

        // a device this PC has a name for and the package doesn't
        OnlyHere,

        // a loopback or host whose name something else here already uses
        NameTaken,

        // a network host named after the PC the package came from
        NamedAfterOtherPc,
    };

    // What will happen to it, given the choices.
    enum class ItemOutcome
    {
        WillBeAdded,
        StaysAsItIs,
        KeepsYourName,
        GetsThePackagesName,
        LosesYourName,
        LeftOutNameTaken,
        AddedUnderNewName,
        TakesThisPcName,
        KeepsOtherPcName,
        NotImported,
    };

    struct PlanItem
    {
        // stable within the plan, for the choices
        std::wstring Id{};

        ConfigPart Part{ ConfigPart::DeviceNames };
        ItemKind Kind{ ItemKind::DeviceName };
        ItemStatus Status{ ItemStatus::New };
        NetworkKind Network{ NetworkKind::NetworkMidi2 };

        // the package's name for it
        std::wstring Name{};

        // a loopback's ends, and the name they share when they're "Max (A)" and "Max (B)"
        std::vector<std::wstring> EndpointNames{};
        std::wstring BaseName{};

        // this PC's name for the same device
        std::wstring YourName{};
        std::wstring YourImage{};

        // the package's picture for a device
        std::wstring Image{};

        // what it would be added as: a free name when its own is taken, or this PC's name for a
        // host named after the other PC
        std::wstring FreeName{};
        std::vector<std::wstring> FreeEndpointNames{};
        std::wstring FreeServiceInstanceName{};

        // set to a network adapter on the other PC
        bool UsesOtherAdapter{ false };

        // for saying whether a device is plugged in here
        std::wstring EndpointDeviceId{};
        std::wstring DeviceInstanceId{};
        std::wstring TransportSuppliedName{};

        // where it is, for applying the plan
        std::wstring SectionKey{};
        std::wstring MatchIdentity{};
        std::wstring EntryKey{};
    };

    struct ImportPlan
    {
        // where it came from: name, PC, date and note
        PackageInfo Source{};

        // a configuration file saved before packages existed
        bool IsPlainConfiguration{ false };

        // the configuration in use, as it was when this plan was made
        bool HasTarget{ false };
        std::wstring TargetFileName{};
        std::wstring TargetConfigName{};
        std::string PlannedBytes{};

        // the name it gets when it's kept apart, "Studio (STUDIO-PC)"
        std::wstring NewConfigurationName{};

        std::wstring ThisComputerName{};

        ConfigInventory Package{};
        ConfigInventory Here{};

        std::vector<PlanItem> Items{};

        // RTP-MIDI isn't installed here, so what's in that part is saved for when it is
        bool RtpInstalled{ true };

        bool PackageHasSynthMode{ false };
        std::wstring PackageSynthMode{};
        std::wstring HereSynthMode{};

        PortNaming PackagePortNaming{ PortNaming::None };
        PortNaming HerePortNaming{ PortNaming::None };

        // automatic port names, when the package has them, work on this PC
        bool PackagePortNamingUsable{ true };

        // pictures in the package that its device names use
        uint32_t PackageDevicePictureCount{ 0 };
    };

    // Reads the configuration in use and works out what each part of the package would do to it.
    ConfigResult MakeImportPlan(
        _In_ ConfigEnvironment const& environment,
        _In_ PackageContents const& contents,
        _In_ bool rtpInstalled,
        _Out_ ImportPlan& plan) noexcept;

    ItemOutcome OutcomeFor(_In_ PlanItem const& item, _In_ ImportChoices const& choices) noexcept;

    // Each outcome that adds the item, or replaces this PC's version with it.
    bool IsImported(_In_ ItemOutcome outcome) noexcept;

    // The package's synthesizer mode or port names will replace this PC's.
    bool SynthModeWillChange(_In_ ImportPlan const& plan, _In_ ImportChoices const& choices) noexcept;
    bool PortNamingWillChange(_In_ ImportPlan const& plan, _In_ ImportChoices const& choices) noexcept;
}
