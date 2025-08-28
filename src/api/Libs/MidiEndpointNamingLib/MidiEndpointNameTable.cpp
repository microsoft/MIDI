// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>

#include <string>
#include <vector>
#include <algorithm>

#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include <winrt/windows.devices.enumeration.h>
#include "wil/result_macros.h"
#include "wil/registry.h"

#include <mmsystem.h>
#include <wtypes.h>
#include <combaseapi.h>
#include <initguid.h>
#include <Devpkey.h>

#include "swd_helpers.h"
#include "MidiDefs.h"
#include "wstring_util.h"

#include "MidiEndpointNameTable.h"

namespace WindowsMidiServicesNamingLib
{
    namespace internal = WindowsMidiServicesInternal;
    
// we always write the total size in bytes (size_t), and then a number of these entries
#define MIDI1_PORT_NAME_ENTRY_HEADER_SIZE (sizeof(size_t))

// max of 32 total inputs/outputs
#define MAX_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) * 32 + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)
#define MIN_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)

// Default WinMM naming for MIDI 1 device using a MIDI 1 driver
#define MIDI_MIDI1_PORT_NAMING_DEFAULT_REG_VALUE_NAME    L"DefaultMidi1PortNaming"
#define MIDI_MIDI1_PORT_NAMING_DEFAULT_VALUE             ((uint32_t)(Midi1PortNameSelection::UseLegacyWinMM))


std::wstring RemoveJustKSPinGeneratedSuffix(
    _In_ std::wstring const& pinName
)
{
    std::wstring cleanedPinName{ WindowsMidiServicesInternal::TrimmedWStringCopy(pinName) };

    std::wstring suffixesToRemove[] =
    {
        // In most cases I've seen, these are added by our USB and KS stack, not by the device
        // Originally this went only to 16, but there are vendor driver devices with 32 total ports
        // that can be configured to be any combo of inputs and outputs. The [:] is an odd one, produced
        // by our stack when reading the 11th port (0-based number 10) from the ESI M8U eX.
        L"[0]", L"[1]", L"[2]", L"[3]", L"[4]", L"[5]", L"[6]", L"[7]", L"[8]",
        L"[9]", L"[10]", L"[11]", L"[12]", L"[13]", L"[14]", L"[15]", L"[16]",
        L"[17]", L"[18]", L"[19]", L"[20]", L"[21]", L"[22]", L"[23]", L"[24]",
        L"[25]", L"[26]", L"[27]", L"[28]", L"[29]", L"[30]", L"[31]", L"[32]",
        L"[:]"
    };

    for (auto const& word : suffixesToRemove)
    {
        if (cleanedPinName.ends_with(word))
        {
            cleanedPinName = cleanedPinName.substr(0, cleanedPinName.length() - word.length());
        }
    }

    return WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedPinName);
}

std::wstring AddGroupNumberToNameIfNeeded(
    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& generatedName,
    _In_ uint8_t groupIndex

)
{
    std::wstring newName{ generatedName };

    if (generatedName == parentDeviceName || generatedName == filterName)
    {
        // we fell back to the device name, so add a group number when > 0

        if (groupIndex > 0)
        {
            // check to see if we already have a number at the end of this. If so, don't add another one

            if (!WindowsMidiServicesInternal::StringEndsWithSpecifiedNumber(newName, groupIndex + 1))
            {
                auto groupNumber = std::to_wstring(groupIndex + 1);

                auto nameReservedSpaces = groupNumber.length() + 2; // null terminator, space, and then up to two digits for the group number

                newName = generatedName.substr(0, MAXPNAMELEN - nameReservedSpaces) + L" " + groupNumber;
            }
        }
    }

    return WindowsMidiServicesInternal::TrimmedWStringCopy(newName);
}



std::wstring FullyCleanupKSPinName(
    _In_ std::wstring const& pinName,
    _In_ std::wstring parentDeviceName,
    _In_ std::wstring filterName
)
{
    std::wstring cleanedPinName{ ::WindowsMidiServicesInternal::TrimmedWStringCopy(RemoveJustKSPinGeneratedSuffix(pinName)) };

    // Used by ESI, MOTU, and others. We don't want to mess up other names, so check only
    // for whole word, not substring. We do other removal in the next step

    auto checkPinName = ::WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(cleanedPinName);

    if (checkPinName == L"midi" ||
        checkPinName == L"out" ||
        checkPinName == L"in" ||
        checkPinName == L"io"
        )
    {
        cleanedPinName = L"";
    }

    auto comparePinName = ::WindowsMidiServicesInternal::ToUpperWStringCopy(cleanedPinName);         // this needs to be just the uppercase of cleanedPinName for the replace to work

    // some pins include the filter or parent device name. We don't want that here because some options re-add it.
    auto compareParentName = ::WindowsMidiServicesInternal::ToUpperWStringCopy(parentDeviceName);
    auto compareFilterName = ::WindowsMidiServicesInternal::ToUpperWStringCopy(filterName);

    // the double and triple space entries need to be last
    // there are other ways to do this with pattern matching, 
    // but just banging this through for this version
    // these must all be uppercase when alpha characters are included
    std::wstring wordsToRemove[] =
    {
        compareParentName, compareFilterName,
        /* L"  ", L"   ", L"    " */
    };

    for (auto const& word : wordsToRemove)
    {
        if (cleanedPinName.length() >= word.length())
        {
            auto idx = comparePinName.find(word);

            if (idx != std::wstring::npos)
            {
                cleanedPinName = cleanedPinName.erase(idx, word.length());
                comparePinName = comparePinName.erase(idx, word.length());
            }
        }
    }

    return WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedPinName);
}


std::wstring FullyCleanupBlockName(
    _In_ std::wstring const& blockName,
    _In_ std::wstring parentDeviceName,
    _In_ std::wstring filterName
)
{
    std::wstring cleanedBlockName{ ::WindowsMidiServicesInternal::TrimmedWStringCopy(RemoveJustKSPinGeneratedSuffix(blockName)) };

    // Used by ESI, MOTU, and others. We don't want to mess up other names, so check only
    // for whole word, not substring. We do other removal in the next step

    auto compareBlockName = ::WindowsMidiServicesInternal::ToUpperTrimmedWStringCopy(cleanedBlockName);

    // some pins include the filter or parent device name. We don't want that here because some options re-add it.
    auto compareParentName = ::WindowsMidiServicesInternal::ToUpperTrimmedWStringCopy(parentDeviceName);
    auto compareFilterName = ::WindowsMidiServicesInternal::ToUpperTrimmedWStringCopy(filterName);

    if (compareBlockName == L"MIDI")
    {
        cleanedBlockName = L"";
        compareBlockName = L"";
    }

    // shortcut additional checks
    if (cleanedBlockName.empty())
    {
        return cleanedBlockName;
    }

    // there are other ways to do this with pattern matching, 
    // but just banging this through for this version
    // these must all be uppercase when alpha characters are included
    std::wstring wordsToRemove[] =
    {
        compareParentName, compareFilterName,
    };

    for (auto const& word : wordsToRemove)
    {
        if (cleanedBlockName.length() >= word.length())
        {
            auto idx = compareBlockName.find(word);

            if (idx != std::wstring::npos)
            {
                cleanedBlockName = cleanedBlockName.erase(idx, word.length());
                compareBlockName = compareBlockName.erase(idx, word.length());
            }
        }
    }

    return WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedBlockName);
}



std::wstring GenerateLegacyMidi1PortName(
    _In_ std::wstring const& nameFromRegistry,
    _In_ std::wstring const& filterName,
    _In_ MidiFlow const flowFromUserPerspective,
    _In_ uint8_t const portIndexWithinThisFilterAndDirection
) noexcept
{
    std::wstring generatedName{};

    if (!WindowsMidiServicesInternal::TrimmedWStringCopy(nameFromRegistry).empty())
    {
        // If name from registry is not blank, use that first
        // NOTE: There's an existing issue in WinMM that causes two of the same make/model of
        // device to have the same name, even if they report different names, because they 
        // share the same registry entry. To maintain compatibility, we cannot fix that here
        // Instead, the custom will need to use one of the other provided naming options.

        generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(nameFromRegistry).substr(0, MAXPNAMELEN - 1);
    }
    else
    {
        // If registry name is empty, use the device friendly name (filter name in this case)
        generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(filterName).substr(0, MAXPNAMELEN - 1);
    }

    generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(generatedName);

    // if this is not the first port for this filter, instance prefix with MIDIIN/OUT #

    if (portIndexWithinThisFilterAndDirection > 0)
    {
        // switching back and forth between wstring and string here is probably not a great idea, but the original
        // values from USB should all be narrow standard strings anyway.
        if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
        {
            auto formatted = std::format("MIDIIN{} ({})", portIndexWithinThisFilterAndDirection + 1, winrt::to_string(generatedName));
            return WindowsMidiServicesInternal::TrimmedWStringCopy(std::wstring(formatted.begin(), formatted.end()).substr(0, MAXPNAMELEN - 1));
        }
        else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
        {
            auto formatted = std::format("MIDIOUT{} ({})", portIndexWithinThisFilterAndDirection + 1, winrt::to_string(generatedName));
            return WindowsMidiServicesInternal::TrimmedWStringCopy(std::wstring(formatted.begin(), formatted.end()).substr(0, MAXPNAMELEN - 1));
        }
        else
        {
            // unexpected
            return generatedName;
        }

    }
    else
    {
        return generatedName;
    }
}

std::wstring GeneratePinNameBasedMidi1PortName(
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& pinName,
    _In_ MidiFlow const flowFromUserPerspective,
    _In_ uint8_t const portIndexWithinThisFilterAndDirection
) noexcept
{
    UNREFERENCED_PARAMETER(filterName);
    UNREFERENCED_PARAMETER(flowFromUserPerspective);
    UNREFERENCED_PARAMETER(portIndexWithinThisFilterAndDirection);

    std::wstring generatedName{ RemoveJustKSPinGeneratedSuffix(pinName) };

    // we use the pin name exactly as it is in the device
    generatedName = generatedName.substr(0, MAXPNAMELEN - 1);

    return WindowsMidiServicesInternal::TrimmedWStringCopy(generatedName);
}


std::wstring GenerateFilterPlusPinNameBasedMidi1PortName(
    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& pinName,
    _In_ uint8_t groupIndex
) noexcept
{
    std::wstring generatedName{};

    auto cleanedPinName = FullyCleanupKSPinName(pinName, parentDeviceName, filterName);

    generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(filterName + L" " + WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedPinName));

    // if the name is too long, try using just the pin name or just the filter name

    if (generatedName.length() + 1 > MAXPNAMELEN)
    {
        if (!cleanedPinName.empty())
        {
            // we're over length, so just use the pin name
            generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedPinName.substr(0, MAXPNAMELEN - 1));
        }
        else
        {
            // we're over length, and there's no pin name
            // so we use the filter name
            generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(filterName.substr(0, MAXPNAMELEN - 1));
        }
    }

    generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);

    return generatedName;
}

std::wstring GenerateFilterPlusBlockMidi1PortName(
    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& blockName,
    _In_ uint8_t groupIndex
) noexcept
{
    std::wstring generatedName{};

    auto cleanedBlockName = FullyCleanupBlockName(blockName, parentDeviceName, filterName);

    generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(filterName + L" " + WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedBlockName));

    if (generatedName.empty())
    {
        generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(parentDeviceName.substr(0, MAXPNAMELEN - 1));
    }


    // if the name is too long, try using just the pin name or just the filter name

    if (generatedName.length() + 1 > MAXPNAMELEN)
    {
        if (!cleanedBlockName.empty())
        {
            // we're over length, so just use the gtb name
            generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedBlockName.substr(0, MAXPNAMELEN - 1));
        }
        else
        {
            // we're over length, and there's no gtb name
            // so we use the filter name
            generatedName = WindowsMidiServicesInternal::TrimmedWStringCopy(filterName.substr(0, MAXPNAMELEN - 1));
        }
    }

    generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);

    return generatedName;
}


std::vector<Midi1PortNameEntry> ReadMidi1PortNameTableFromPropertyData(
    _In_reads_bytes_(dataSize) uint8_t* tablePointer,
    _In_ uint32_t const dataSize
) noexcept
{
    std::vector<Midi1PortNameEntry> nameTable{};

    if (tablePointer == nullptr)
    {
        return nameTable;   // empty table
    }

    size_t totalSizeBytes{ 0 };

    if (dataSize >= MIN_PORT_NAME_TABLE_SIZE && dataSize <= MAX_PORT_NAME_TABLE_SIZE)
    {
        // the first size_t in the payload is the size of the entire table, including this size_t header
        memcpy(&totalSizeBytes, tablePointer, sizeof(size_t));

        if (totalSizeBytes != dataSize)
        {
            LOG_IF_FAILED(E_INVALIDARG);

            // invalid size, so we return empty table
            return nameTable;
        }
    }
    else
    {
        // invalid table property value, so we return empty table
        LOG_IF_FAILED(E_INVALIDARG);

        return nameTable;
    }

    // we've already read the header
    size_t bytesRead = MIDI1_PORT_NAME_ENTRY_HEADER_SIZE;

    // the rest of the data is just an array of the Midi1PortNameEntry structures

    size_t numStructs = (totalSizeBytes - bytesRead) / sizeof(Midi1PortNameEntry);
    size_t byteCountToCopy = sizeof(Midi1PortNameEntry) * numStructs;

    if (numStructs > 0 && byteCountToCopy == (totalSizeBytes - bytesRead))
    {
        nameTable.resize(numStructs);

        byte* readPosition = (byte*)(tablePointer + bytesRead);

        memcpy(nameTable.data(), readPosition, byteCountToCopy);
    }
    else
    {
        LOG_IF_FAILED(E_FAIL);
    }

    return nameTable;
}


inline bool WriteMidi1PortNameTableToPropertyDataPointer(
    _In_ std::vector<Midi1PortNameEntry> const& entries,
    _Inout_ std::vector<std::byte>& propertyData
)
{
    if (entries.size() == 0) return false;

    // calculate the total size
    size_t entriesSizeBytes = entries.size() * sizeof(Midi1PortNameEntry);
    size_t totalSizeBytes = (size_t)(entriesSizeBytes + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE);

    propertyData.resize(totalSizeBytes, (std::byte)0);

    if (propertyData.size() != totalSizeBytes)
    {
        LOG_IF_FAILED(E_POINTER);
        return false;
    }

    // header value (byte count)
    memcpy((byte*)(propertyData.data()), (byte*)&totalSizeBytes, MIDI1_PORT_NAME_ENTRY_HEADER_SIZE);

    // copy in all the name data. Vectors are guaranteed to be contiguous.
    memcpy((byte*)(propertyData.data() + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE), (byte*)entries.data(), entriesSizeBytes);

    return true;
}

// ========================================================================================================

_Use_decl_annotations_
std::shared_ptr<MidiEndpointNameTable> MidiEndpointNameTable::FromEndpointDeviceId(
    winrt::hstring const& endpointDeviceId)
{
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNameTable);

    winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo { nullptr };

    try
    {
        deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(endpointDeviceId, additionalProperties).get();
    }
    catch (winrt::hresult_error)
    {
    }

    return FromDeviceInfo(deviceInfo);
}

_Use_decl_annotations_
std::shared_ptr<MidiEndpointNameTable> MidiEndpointNameTable::FromDeviceInfo(
    _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInfo)
{
    if (deviceInfo != nullptr)
    {
        auto refArray = WindowsMidiServicesInternal::SafeGetSwdBinaryPropertyFromDeviceInformation(
            STRING_PKEY_MIDI_Midi1PortNameTable,
            deviceInfo
        );

        if (refArray != nullptr)
        {
            auto refData = refArray.Value();

            return MidiEndpointNameTable::FromPropertyData(refData);
        }

        // even if no name table, we should still return the object in case the consumer wants to add new name table entries

    }

    return std::make_shared<MidiEndpointNameTable>();

}



_Use_decl_annotations_
std::shared_ptr<MidiEndpointNameTable> MidiEndpointNameTable::FromPropertyData(
    winrt::com_array<uint8_t> const& propertyData)
{
    std::shared_ptr<MidiEndpointNameTable> results = std::make_shared<MidiEndpointNameTable>();

    // ideally, this should get folded into this code and then the header removed. Need to refactor.
    auto nameEntries = ReadMidi1PortNameTableFromPropertyData(propertyData.data(), propertyData.size());

    for (auto const& entry : nameEntries)
    {
        auto ptr = std::make_shared<Midi1PortNameEntry>();

        if (ptr != nullptr)
        {
            memcpy(ptr.get(), &entry, sizeof(Midi1PortNameEntry));

            if (entry.DataFlowFromUserPerspective == MidiFlow::MidiFlowIn)
            {
                results->m_sourceEntries.emplace(ptr->GroupIndex, ptr);
            }
            else
            {
                results->m_destinationEntries.emplace(ptr->GroupIndex, ptr);
            }
        }
    }

    return results;
}



_Use_decl_annotations_
bool MidiEndpointNameTable::UpdateSourceEntryCustomName(
    uint8_t const groupIndex, 
    winrt::hstring const& name) noexcept
{
    auto entry = GetSourceEntry(groupIndex);

    if (entry != nullptr)
    {
        memset(entry->CustomName, 0, MAXPNAMELEN);
        wcsncpy_s(entry->CustomName, MAXPNAMELEN, name.c_str(), name.size());


        return true;
    }

    return false;
}

_Use_decl_annotations_
bool MidiEndpointNameTable::UpdateDestinationEntryCustomName(
    uint8_t const groupIndex,
    winrt::hstring const& name) noexcept
{
    auto entry = GetDestinationEntry(groupIndex);

    if (entry != nullptr)
    {
        memset(entry->CustomName, 0, MAXPNAMELEN);
        wcsncpy_s(entry->CustomName, MAXPNAMELEN, name.c_str(), name.size());

        return true;
    }

    return false;
}




_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::WriteProperties(
    std::vector<DEVPROPERTY>& destination) noexcept
{
    std::vector<Midi1PortNameEntry> entries;

    // copy all the map values over into this temp vector, so we can write to the data pointer
    std::transform(m_sourceEntries.begin(), m_sourceEntries.end(), std::back_inserter(entries), [](auto& p){ return *p.second; });
    std::transform(m_destinationEntries.begin(), m_destinationEntries.end(), std::back_inserter(entries), [](auto& p) { return *p.second; });

    m_nameTablePropertyData.clear();

    if (WriteMidi1PortNameTableToPropertyDataPointer(entries, m_nameTablePropertyData))
    {
        destination.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)m_nameTablePropertyData.size(), (PVOID)m_nameTablePropertyData.data() });

        //destination.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
        //    DEVPROP_TYPE_UINT32, (ULONG)sizeof(Midi1PortNameSelection), (PVOID)&EndpointLocalPortNameSelectionOverride });

        return S_OK;
    }
    else
    {
        destination.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_EMPTY, 0, (PVOID)nullptr });

        return S_OK;
    }


    return E_FAIL;
}


_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::PopulateEntryForNativeUmpDevice(
    uint8_t const groupIndex,
    MidiFlow const flowFromUserPerspective,
    std::wstring const& customName,
    std::wstring const& parentDeviceName,
    std::wstring const& filterName,
    std::wstring const& blockName,
    uint8_t const portIndexWithinThisFilterAndDirection
) noexcept
{
    UNREFERENCED_PARAMETER(portIndexWithinThisFilterAndDirection);

    auto entry = std::make_shared<Midi1PortNameEntry>();
   
    std::wstring newStyleName = GenerateFilterPlusBlockMidi1PortName(
        parentDeviceName,
        filterName,
        blockName,
        groupIndex
    );



    //std::wstring legacyWinMMName = GenerateLegacyMidi1PortName(
    //    parentDeviceName,
    //    filterName,
    //    flowFromUserPerspective,
    //    portIndexWithinThisFilterAndDirection
    //);

    // when generating names for native UMP devices, we don't have old rules to adhere to
    // so we can use new-style names all the time.
    std::wstring legacyWinMMName = newStyleName;

    internal::SafeCopyWStringToFixedArray(entry->CustomName, MAXPNAMELEN, customName);
    internal::SafeCopyWStringToFixedArray(entry->LegacyWinMMName, MAXPNAMELEN, legacyWinMMName);
    internal::SafeCopyWStringToFixedArray(entry->NewStyleName, MAXPNAMELEN, newStyleName);

    entry->GroupIndex = groupIndex;
    entry->DataFlowFromUserPerspective = flowFromUserPerspective;

    if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
    {
        m_sourceEntries[groupIndex] = entry;
    }
    else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
    {
        m_destinationEntries[groupIndex] = entry;
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::PopulateAllEntriesForMidi1DeviceUsingUmpDriver(
    std::wstring const& parentDeviceName,
    std::vector<WindowsMidiServicesInternal::GroupTerminalBlockInternal>& blocks
) noexcept
{
    UNREFERENCED_PARAMETER(parentDeviceName);

    for (auto const& gtb : blocks)
    {
        for (uint8_t groupIndex = gtb.FirstGroupIndex; groupIndex < gtb.FirstGroupIndex + gtb.GroupCount; groupIndex++)
        {
            std::wstring customName = L"";
            std::wstring pinName = gtb.Name;

            if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_OUTPUT)              // gtb output is a midi input (Source)
            {
                LOG_IF_FAILED(PopulateEntryForMidi1DeviceUsingUmpDriver(
                    groupIndex,
                    MidiFlow::MidiFlowIn,
                    customName,
                    gtb.Name));
            }

            if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_INPUT)           // block input is a MIDI output (Destination)
            {
                LOG_IF_FAILED(PopulateEntryForMidi1DeviceUsingUmpDriver(
                    groupIndex,
                    MidiFlow::MidiFlowOut,
                    customName,
                    gtb.Name));
            }
        }
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::PopulateAllEntriesForNativeUmpDevice(
    std::wstring const& parentDeviceName,
    std::vector<WindowsMidiServicesInternal::GroupTerminalBlockInternal>& blocks
) noexcept
{
    uint8_t outIndex{ 0 };
    uint8_t inIndex{ 0 };

    for (auto const& gtb : blocks)
    {
        for (uint8_t groupIndex = gtb.FirstGroupIndex; groupIndex < gtb.FirstGroupIndex + gtb.GroupCount; groupIndex++)
        {
            std::wstring customName = L"";
            std::wstring pinName = gtb.Name;

            if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_OUTPUT)              // gtb output is a midi input (Source)
            {
                LOG_IF_FAILED(PopulateEntryForNativeUmpDevice(
                    groupIndex,
                    MidiFlow::MidiFlowIn,
                    customName,
                    parentDeviceName,
                    parentDeviceName,
                    gtb.Name,
                    outIndex));

                outIndex++;
            }

            if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_INPUT)           // block input is a MIDI output (Destination)
            {
                LOG_IF_FAILED(PopulateEntryForNativeUmpDevice(
                    groupIndex,
                    MidiFlow::MidiFlowOut,
                    customName,
                    parentDeviceName,
                    parentDeviceName,
                    gtb.Name,
                    inIndex));

                inIndex++;
            }
        }
    }

    return S_OK;
}



_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::PopulateEntryForMidi1DeviceUsingUmpDriver(
    uint8_t const groupIndex,
    MidiFlow const flowFromUserPerspective,
    std::wstring const& customName,
    std::wstring const& blockName
) noexcept
{
    auto entry = std::make_shared<Midi1PortNameEntry>();

    // use teh block name directly
    std::wstring newStyleName = blockName;
    std::wstring legacyWinMMName = blockName;

    internal::SafeCopyWStringToFixedArray(entry->CustomName, MAXPNAMELEN, customName);
    internal::SafeCopyWStringToFixedArray(entry->LegacyWinMMName, MAXPNAMELEN, legacyWinMMName);
    internal::SafeCopyWStringToFixedArray(entry->NewStyleName, MAXPNAMELEN, newStyleName);

    entry->GroupIndex = groupIndex;
    entry->DataFlowFromUserPerspective = flowFromUserPerspective;

    if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
    {
        m_sourceEntries[groupIndex] = entry;
    }
    else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
    {
        m_destinationEntries[groupIndex] = entry;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::PopulateEntryForMidi1DeviceUsingMidi1Driver(
    uint8_t const groupIndex,
    MidiFlow const flowFromUserPerspective,
    std::wstring const& customName,
    std::wstring const& nameFromRegistry,
    std::wstring const& filterName,
    std::wstring const& pinName,
    uint8_t const portIndexWithinThisFilterAndDirection
) noexcept
{
    auto entry = std::make_shared<Midi1PortNameEntry>();

    // use teh block name directly
    std::wstring newStyleName = GenerateFilterPlusPinNameBasedMidi1PortName(
        nameFromRegistry,
        filterName,
        pinName,
        groupIndex
    );

    std::wstring legacyWinMMName = GenerateLegacyMidi1PortName(
        nameFromRegistry,
        filterName,
        flowFromUserPerspective,
        portIndexWithinThisFilterAndDirection       
   );


    internal::SafeCopyWStringToFixedArray(entry->CustomName, MAXPNAMELEN, customName);
    internal::SafeCopyWStringToFixedArray(entry->LegacyWinMMName, MAXPNAMELEN, legacyWinMMName);
    internal::SafeCopyWStringToFixedArray(entry->NewStyleName, MAXPNAMELEN, newStyleName);

    entry->GroupIndex = groupIndex;
    entry->DataFlowFromUserPerspective = flowFromUserPerspective;

    if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
    {
        m_sourceEntries[groupIndex] = entry;
    }
    else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
    {
        m_destinationEntries[groupIndex] = entry;
    }

    return S_OK;
}













_Use_decl_annotations_
std::shared_ptr<Midi1PortNameEntry> MidiEndpointNameTable::GetSourceEntry(
    uint8_t const groupIndex) const noexcept
{
    auto entry = m_sourceEntries.find(groupIndex);

    if (entry != m_sourceEntries.end())
    {
        return entry->second;
    }
    else
    {
        return nullptr;
    }
}

_Use_decl_annotations_
std::shared_ptr<Midi1PortNameEntry> MidiEndpointNameTable::GetDestinationEntry(
    uint8_t const groupIndex) const noexcept
{
    auto entry = m_destinationEntries.find(groupIndex);

    if (entry != m_destinationEntries.end())
    {
        return entry->second;
    }
    else
    {
        return nullptr;
    }
}


Midi1PortNameSelection MidiEndpointNameTable::GetSystemDefaultPortNameSelection() noexcept
{
    DWORD defaultPortNamingForMidi1Drivers{ MIDI_MIDI1_PORT_NAMING_DEFAULT_VALUE };

    if (SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, MIDI_MIDI1_PORT_NAMING_DEFAULT_REG_VALUE_NAME, &defaultPortNamingForMidi1Drivers)))
    {
        auto defaultMidi1PortNamingForByteDriverSelection = static_cast<Midi1PortNameSelection>(defaultPortNamingForMidi1Drivers);

        // make sure we don't get all recursive here
        if (defaultMidi1PortNamingForByteDriverSelection != Midi1PortNameSelection::UseGlobalDefault)
        {
            return defaultMidi1PortNamingForByteDriverSelection;
        }
    }

    return static_cast<Midi1PortNameSelection>(MIDI_MIDI1_PORT_NAMING_DEFAULT_VALUE);
}



std::wstring MidiEndpointNameTable::GetPreferredName(
    _In_ uint8_t const groupIndex,
    _In_ MidiFlow const flowFromUserPerspective,
    _In_ Midi1PortNameSelection const endpointLocalPortNameSelection)
{
    std::shared_ptr<Midi1PortNameEntry> entry{ nullptr };

    if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
    {
        entry = GetSourceEntry(groupIndex);
    }
    else
    {
        entry = GetDestinationEntry(groupIndex);
    }

    if (entry != nullptr)
    {
        // if a custom name was provided, use it
        if (entry->CustomName[0] != 0)
        {
            return std::wstring{ entry->CustomName };
        }

        Midi1PortNameSelection selection = endpointLocalPortNameSelection;
        if (selection == Midi1PortNameSelection::UseGlobalDefault)
        {
            selection = MidiEndpointNameTable::GetSystemDefaultPortNameSelection();
        }

        switch (endpointLocalPortNameSelection)
        {
        case Midi1PortNameSelection::UseLegacyWinMM:
            return std::wstring{ entry->LegacyWinMMName };

        case Midi1PortNameSelection::UseNewStyleName:
            return std::wstring{ entry->NewStyleName };

        default:
            return std::wstring{ entry->NewStyleName };
        }
    }

    // if this happens, we have a logic issue in creating MIDI 1 ports. 
    return L"";
}

std::wstring MidiEndpointNameTable::GetSourceEntryCustomName(
    _In_ uint8_t const groupIndex
) noexcept
{
    auto entry = GetSourceEntry(groupIndex);

    if (entry)
    {
        return std::wstring{ entry->CustomName } ;
    }

    return L"";
}

std::wstring MidiEndpointNameTable::GetDestinationEntryCustomName(
    _In_ uint8_t const groupIndex
) noexcept
{
    auto entry = GetDestinationEntry(groupIndex);

    if (entry)
    {
        return std::wstring{ entry->CustomName };
    }

    return L"";

}


bool MidiEndpointNameTable::IsEqualTo(MidiEndpointNameTable* nameTable)
{
    if (SourceEntryCount() != nameTable->SourceEntryCount() ||
        DestinationEntryCount() != nameTable->DestinationEntryCount())
    {
        return true;
    }

    for (auto const& it : m_sourceEntries)
    {
        auto index = it.first;
        auto thatEntry = nameTable->GetSourceEntry(index);

        if (wcscmp(it.second->CustomName, thatEntry->CustomName) != 0) return false;
        if (wcscmp(it.second->LegacyWinMMName, thatEntry->LegacyWinMMName) != 0) return false;
        if (wcscmp(it.second->NewStyleName, thatEntry->NewStyleName) != 0) return false;
    }

    for (auto const& it : m_destinationEntries)
    {
        auto index = it.first;
        auto thatEntry = nameTable->GetDestinationEntry(index);

        if (wcscmp(it.second->CustomName, thatEntry->CustomName) != 0) return false;
        if (wcscmp(it.second->LegacyWinMMName, thatEntry->LegacyWinMMName) != 0) return false;
        if (wcscmp(it.second->NewStyleName, thatEntry->NewStyleName) != 0) return false;
    }

    return true;
}


std::vector<Midi1PortNameEntry> MidiEndpointNameTable::GetAllSourceEntries()
{
    std::vector<Midi1PortNameEntry> entries{};

    std::transform(m_sourceEntries.begin(), m_sourceEntries.end(), std::back_inserter(entries), [](auto& p) { return *p.second; });

    return entries;
}

std::vector<Midi1PortNameEntry> MidiEndpointNameTable::GetAllDestinationEntries()
{
    std::vector<Midi1PortNameEntry> entries{};

    std::transform(m_destinationEntries.begin(), m_destinationEntries.end(), std::back_inserter(entries), [](auto& p) { return *p.second; });

    return entries;

}




}