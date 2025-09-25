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
    };

    struct Midi1PortNameEntry
    {
        uint8_t GroupIndex{ 0 };
        MidiFlow DataFlowFromUserPerspective{ MidiFlow::MidiFlowIn };   // MidiFlowIn is 0

        wchar_t CustomName[MAXPNAMELEN]{ 0 };           // user-supplied
        wchar_t LegacyWinMMName[MAXPNAMELEN]{ 0 };      // for Midi1 driver: based on registry entry and filter name + index. For Midi2 driver: Block name.
        wchar_t NewStyleName[MAXPNAMELEN]{ 0 };         // For Midi1 driver: Filter + Pin name. For Midi2 driver: Block name.
    };

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


        bool IsEqualTo(MidiEndpointNameTable* nameTable);

        uint8_t SourceEntryCount() { return static_cast<uint8_t>(m_sourceEntries.size()); }
        uint8_t DestinationEntryCount() { return static_cast<uint8_t>(m_destinationEntries.size()); }

        std::vector<Midi1PortNameEntry> GetAllSourceEntries();
        std::vector<Midi1PortNameEntry> GetAllDestinationEntries();

    private:
        std::map<uint8_t, std::shared_ptr<Midi1PortNameEntry>> m_sourceEntries{};
        std::map<uint8_t, std::shared_ptr<Midi1PortNameEntry>> m_destinationEntries{};


        std::vector<std::byte> m_nameTablePropertyData{}; // need to retain this here for property writing



    };


}

#endif