// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINT_NAME_TABLE_H
#define MIDI_ENDPOINT_NAME_TABLE_H

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <Windows.h>
#include <mmsystem.h>
#include <string.h>
#include <string>
#include <vector>
#include "MidiDefs.h"

#include "midi_group_terminal_blocks.h"

#include "WindowsMidiServices.h"    // for MidiFlow

namespace WindowsMidiServicesNamingLib
{
    // a provided custom name will always override this. If changed, this needs 
    // to be kept in sync with the MIDI settings app registry service
    enum Midi1PortNameSelection : uint32_t
    {
        UseGlobalDefault = 0,                  // global default tells us which to defer to
        UseLegacyWinMM = 1,                    // compatible with pre-Windows MIDI Services WinMM port names
        UseNewStyleName = 2,                   // will differ based on UMP driver vs MIDI 1 driver    
        UseAutomatic = 3,                      // Windows decides per endpoint, from what the device reported
    };

    struct Midi1PortNameEntry
    {
        uint8_t GroupIndex{ 0 };
        MidiFlow DataFlowFromUserPerspective{ MidiFlow::MidiFlowIn };   // MidiFlowIn is 0

        wchar_t CustomName[MAXPNAMELEN]{ 0 };           // user-supplied
        wchar_t LegacyWinMMName[MAXPNAMELEN]{ 0 };      // for Midi1 driver: based on registry entry and filter name + index. For Midi2 driver: Block name.
        wchar_t NewStyleName[MAXPNAMELEN]{ 0 };         // For Midi1 driver: Filter + Pin name. For Midi2 driver: Block name.
    };

    // Where the name that identifies an individual port came from. Also the signal the Automatic
    // naming style uses: anything other than None means the device told us something about its
    // ports, so building a new name from it is worth the compatibility cost.
    enum class Midi1PortNameSource
    {
        None = 0,
        Pin,                    // iJack / KS pin name
        DriverRegistry,         // MediaCategories entry, only when it is a per-filter fact
        Filter,                 // KS filter name, when it differs from the device name
        GroupTerminalBlock,
        FunctionBlock,
    };

    struct Midi1ResolvedPortName
    {
        std::wstring Name{ };
        Midi1PortNameSource Source{ Midi1PortNameSource::None };
    };

    // One port's worth of raw material, as gathered by a transport.
    struct Midi1PortNameInput
    {
        uint8_t GroupIndex{ 0 };
        MidiFlow DataFlowFromUserPerspective{ MidiFlow::MidiFlowIn };

        std::wstring PinName{ };                // iJack / KS pin, or a group terminal block or function block name
        std::wstring DriverRegistryName{ };     // MediaCategories
        std::wstring FilterName{ };
    };

    struct Midi1PortNameResult
    {
        uint8_t GroupIndex{ 0 };
        MidiFlow DataFlowFromUserPerspective{ MidiFlow::MidiFlowIn };

        std::wstring Name{ };
        Midi1ResolvedPortName Resolved{ };
    };

    // Building blocks, exposed so they can be tested directly against real device data.
    std::wstring RemoveGeneratedPinNameSuffix(_In_ std::wstring const& pinName) noexcept;
    bool IsPlaceholderPortName(_In_ std::wstring const& name) noexcept;
    bool PortNameCarriesDeviceName(_In_ std::wstring const& portName, _In_ std::wstring const& deviceName) noexcept;
    std::wstring RemoveDeviceNamePrefixFromPortName(_In_ std::wstring const& portName, _In_ std::wstring const& deviceName) noexcept;
    std::wstring ShortenDeviceNameToFit(_In_ std::wstring const& deviceName, _In_ size_t const maxCharacters) noexcept;
    std::wstring TruncateWithoutSplittingCharacters(_In_ std::wstring const& value, _In_ size_t const maxCharacters) noexcept;

    // driverRegistryNameIsPerFilter: the MediaCategories entry is a per-model location, so its value
    // is only a per-port fact when the device gives each filter its own entry.
    Midi1ResolvedPortName ResolveDeviceSuppliedPortName(
        _In_ std::wstring const& pinName,
        _In_ std::wstring const& driverRegistryName,
        _In_ bool const driverRegistryNameIsPerFilter,
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& deviceName) noexcept;

    // Builds every new-style port name for one endpoint. Numbering and the decision to keep or drop
    // the device name are made across all the ports in a direction, so this cannot be done one port
    // at a time.
    std::vector<Midi1PortNameResult> BuildMidi1PortNamesForEndpoint(
        _In_ std::wstring const& endpointName,
        _In_ bool const driverRegistryNamesArePerFilter,
        _In_ std::vector<Midi1PortNameInput> const& ports) noexcept;

    // Flags for PKEY_MIDI_Midi1PortNameSourceFlags, from what the resolution actually found.
    uint32_t CalculateMidi1PortNameSourceFlags(
        _In_ std::wstring const& endpointName,
        _In_ std::vector<Midi1PortNameResult> const& results) noexcept;

    // Automatic resolves to one of the two concrete styles per endpoint. Never returns
    // UseAutomatic or UseGlobalDefault.
    Midi1PortNameSelection ResolveAutomaticPortNameSelection(
        _In_ bool const hasLegacyEquivalent,
        _In_ uint32_t const nameSourceFlags) noexcept;

    // Drivers which describe every one of their devices identically leave the model name only in
    // the port names. Returns deviceName unchanged when there is nothing to recover.
    std::wstring RecoverModelNameFromPortNames(
        _In_ std::wstring const& deviceName,
        _In_ bool const driverRegistryNamesArePerFilter,
        _In_ std::vector<Midi1PortNameInput> const& ports) noexcept;

    // WinMM marked the Nth unit of a model with a leading "2- ", so a legacy name has to do the
    // same to stay recognizable. New style marks the endpoint name with a trailing " (2)" instead.
    // oneBasedIndex is 1 for the first unit, which is never marked.
    std::wstring ApplyLegacyDuplicateDeviceMarker(
        _In_ std::wstring const& baseDeviceName,
        _In_ uint32_t const oneBasedIndex) noexcept;

    class MidiEndpointNameTable
    {
    public:
        MidiEndpointNameTable() = default;

        static std::shared_ptr<MidiEndpointNameTable> FromEndpointDeviceId(_In_ winrt::hstring const& endpointDeviceId);
        static std::shared_ptr<MidiEndpointNameTable> FromDeviceInfo(_In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInfo);
        static std::shared_ptr<MidiEndpointNameTable> FromPropertyData(_In_ winrt::com_array<uint8_t> const& propertyData);

        static Midi1PortNameSelection GetSystemDefaultPortNameSelection() noexcept;

        std::wstring GetPreferredName(
            _In_ uint8_t const groupIndex, 
            _In_ MidiFlow const flowFromUserPerspective,
            _In_ Midi1PortNameSelection const endpointLocalPortNameSelection);

        std::shared_ptr<Midi1PortNameEntry> GetSourceEntry(
            _In_ uint8_t const groupIndex) const noexcept;

        std::shared_ptr<Midi1PortNameEntry> GetDestinationEntry(
            _In_ uint8_t const groupIndex) const noexcept;



        bool UpdateSourceEntryCustomName(
            _In_ uint8_t const groupIndex,
            _In_ winrt::hstring const& customName
        ) noexcept;

        bool UpdateDestinationEntryCustomName(
            _In_ uint8_t const groupIndex,
            _In_ winrt::hstring const& customName
        ) noexcept;


        std::wstring GetSourceEntryCustomName(
            _In_ uint8_t const groupIndex
        ) noexcept;

        std::wstring GetDestinationEntryCustomName(
            _In_ uint8_t const groupIndex
        ) noexcept;


        HRESULT PopulateAllEntriesForNativeUmpDevice(
            _In_ std::wstring const& parentDeviceName,
            _In_ std::vector<WindowsMidiServicesInternal::GroupTerminalBlockInternal>& blocks
        ) noexcept;

        HRESULT PopulateAllEntriesForMidi1DeviceUsingUmpDriver(
            _In_ std::wstring const& parentDeviceName,
            _In_ std::vector<WindowsMidiServicesInternal::GroupTerminalBlockInternal>& blocks
        ) noexcept;


        HRESULT PopulateEntryForNativeUmpDevice(
            _In_ uint8_t const groupIndex,
            _In_ MidiFlow const flowFromUserPerspective,
            _In_ std::wstring const& customName,
            _In_ std::wstring const& parentDeviceName,
            _In_ std::wstring const& filterName,
            _In_ std::wstring const& blockName,
            _In_ uint8_t const portIndexWithinThisFilterAndDirection
        ) noexcept;

        HRESULT PopulateEntryForMidi1DeviceUsingUmpDriver(
            _In_ uint8_t const groupIndex,
            _In_ MidiFlow const flowFromUserPerspective,
            _In_ std::wstring const& customName,
            _In_ std::wstring const& parentDeviceName,
            _In_ std::wstring const& blockName,
            _In_ uint8_t const portIndexWithinThisFilterAndDirection) noexcept;

        HRESULT PopulateEntryForMidi1DeviceUsingMidi1Driver(
            _In_ uint8_t const groupIndex,
            _In_ MidiFlow const flowFromUserPerspective,
            _In_ std::wstring const& customName,
            _In_ std::wstring const& nameFromRegistry,
            _In_ std::wstring const& filterName,
            _In_ std::wstring const& pinName,
            _In_ uint8_t const portIndexWithinThisFilterAndDirection
        ) noexcept;

        HRESULT WriteProperties(_In_ std::vector<DEVPROPERTY>& destination) noexcept;

        // Rebuilds the group terminal block names from this table, so a port the customer renamed
        // and the block it belongs to do not disagree. Appends nothing when no block changes.
        HRESULT WriteGroupTerminalBlockProperties(
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ std::vector<DEVPROPERTY>& destination) noexcept;

        // Recomputes every new-style name for the endpoint at once. Numbering, and the choice to
        // keep or drop the device name, cannot be decided one port at a time. Only call this after
        // every port has been populated.
        HRESULT RebuildNewStyleNames(
            _In_ std::wstring const& endpointName,
            _In_ bool const driverRegistryNamesArePerFilter) noexcept;

        // Call before re-populating an endpoint, so ports belonging to a filter that has gone away
        // cannot affect the numbering of the ports that remain.
        void ResetPortInputs() noexcept;

        // Raw material for the new-style names. Pass the names as the device reported them, with no
        // duplicate-device marker: that marker belongs on the endpoint name.
        void RecordPortInput(
            _In_ uint8_t const groupIndex,
            _In_ MidiFlow const flowFromUserPerspective,
            _In_ std::wstring const& pinName,
            _In_ std::wstring const& driverRegistryName,
            _In_ std::wstring const& filterName) noexcept;

        // Set false by transports whose ports never existed under WinMM names. Left unset, the
        // endpoint is assumed to have a legacy equivalent, which is what every existing device has.
        void SetPortNamesHaveLegacyEquivalent(_In_ bool const hasLegacyEquivalent) noexcept;

        uint32_t NameSourceFlags() const noexcept { return m_nameSourceFlags; }
        bool NameSourceFlagsValid() const noexcept { return m_nameSourceFlagsValid; }


        bool IsEqualTo(MidiEndpointNameTable* nameTable);

        uint8_t SourceEntryCount() { return static_cast<uint8_t>(m_sourceEntries.size()); }
        uint8_t DestinationEntryCount() { return static_cast<uint8_t>(m_destinationEntries.size()); }

        std::vector<Midi1PortNameEntry> GetAllSourceEntries();
        std::vector<Midi1PortNameEntry> GetAllDestinationEntries();

    private:
        std::map<uint8_t, std::shared_ptr<Midi1PortNameEntry>> m_sourceEntries{};
        std::map<uint8_t, std::shared_ptr<Midi1PortNameEntry>> m_destinationEntries{};

        // Raw material kept beside the entries so the whole endpoint can be rebuilt at once. Keyed
        // the same way the entries are, because a transport re-populates ports as filters arrive.
        std::map<uint8_t, Midi1PortNameInput> m_sourcePortInputs{};
        std::map<uint8_t, Midi1PortNameInput> m_destinationPortInputs{};

        uint32_t m_nameSourceFlags{ 0 };
        bool m_nameSourceFlagsValid{ false };    // only a rebuilt table has anything to report

        DEVPROP_BOOLEAN m_hasLegacyEquivalent{ DEVPROP_TRUE };
        bool m_hasLegacyEquivalentSet{ false };

        std::vector<std::byte> m_nameTablePropertyData{}; // need to retain this here for property writing
        std::vector<std::byte> m_groupTerminalBlockPropertyData{};



    };


}

#endif