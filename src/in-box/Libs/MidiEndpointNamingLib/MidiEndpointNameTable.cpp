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

#include "Feature_Servicing_MIDIPortDisambiguators.h"
#include "Feature_Servicing_MIDI2UnicodeConversion.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"

namespace WindowsMidiServicesNamingLib
{
    namespace internal = WindowsMidiServicesInternal;
    
// we always write the total size in bytes (size_t), and then a number of these entries
#define MIDI1_PORT_NAME_ENTRY_HEADER_SIZE (sizeof(size_t))

// max of 32 total inputs/outputs
#define MAX_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) * 32 + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)
#define MIN_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)

// Entries are memcpy'd straight in and out of the stored property, and a table written by one
// build is read by another, so this layout is a binary contract. Changing it needs a versioned
// format, not an edit.
static_assert(sizeof(Midi1PortNameEntry) == 200, "Midi1PortNameEntry is a persisted binary layout");
static_assert(offsetof(Midi1PortNameEntry, GroupIndex) == 0, "Midi1PortNameEntry is a persisted binary layout");
static_assert(offsetof(Midi1PortNameEntry, DataFlowFromUserPerspective) == 4, "Midi1PortNameEntry is a persisted binary layout");
static_assert(offsetof(Midi1PortNameEntry, CustomName) == 8, "Midi1PortNameEntry is a persisted binary layout");
static_assert(offsetof(Midi1PortNameEntry, LegacyWinMMName) == 72, "Midi1PortNameEntry is a persisted binary layout");
static_assert(offsetof(Midi1PortNameEntry, NewStyleName) == 136, "Midi1PortNameEntry is a persisted binary layout");

// Default WinMM naming for MIDI 1 device using a MIDI 1 driver
#define MIDI_MIDI1_PORT_NAMING_DEFAULT_REG_VALUE_NAME    L"DefaultMidi1PortNaming"
#define MIDI_MIDI1_PORT_NAMING_DEFAULT_VALUE             ((uint32_t)(Midi1PortNameSelection::UseLegacyWinMM))


std::wstring RemoveJustKSPinGeneratedSuffix(
    _In_ std::wstring const& pinName
)
{
    std::wstring cleanedPinName{ WindowsMidiServicesInternal::TrimmedWStringCopy(pinName) };

    if (Feature_Servicing_MIDIPortDisambiguators::IsEnabled())
    {
        std::vector<std::wstring> suffixesToRemove;

        for (int i = 0; i <= 32; i++)
        {
            // In most cases I've seen, these are added by our USB and KS stack, not by the device
            // Originally this went only to 16, but there are vendor driver devices with 32 total ports
            // that can be configured to be any combo of inputs and outputs. The [:] is an odd one, produced
            // by our stack when reading the 11th port (0-based number 10) from the ESI M8U eX.

            // Ideally, we'd just remove this from the KS logic, but that would almost certainly
            // break audio devices, and would also need to have a switch to put them back in when
            // using legacymode or hybrid mode for Windows MIDI Services

            // the space here is important because some devices, like CircuitPython, actually have
            // [0] as part of the pin name and we don't want to remove that
            suffixesToRemove.push_back(std::format(L" [{}]", i));
        }

        // add that weird one from the ESI MIDI interfaces
        suffixesToRemove.push_back(L" [:]");

        for (auto const& word : suffixesToRemove)
        {
            if (cleanedPinName.ends_with(word))
            {
                cleanedPinName = cleanedPinName.substr(0, cleanedPinName.length() - word.length());
            }
        }
    }
    else
    {
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
    }

    return WindowsMidiServicesInternal::TrimmedWStringCopy(cleanedPinName);
}


// Adds the group number differentiator only if the name is the same as the parent device or filter name
std::wstring AddGroupNumberToNameIfNeeded(
    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& generatedName,
    _In_ uint8_t groupIndex

)
{
    // this fails with loopMIDI which creates each port as a new interface with the filter name = to the port name.
    // so we're adding disambiguators for no reason here. 
    //
    // However, this is absolutely needed for some devices, where there's no way to disambiguate the ports without
    // adding another index. This was causing problems before this was added.
    //
    // This whole port/gtb naming process needs a re-think and a comparison to the values generated in other operating 
    // systems for the same devices. Additionally, we need to keep the port names and GTB names in sync when customers
    // rename the MIDI 1 port for a MIDI 1 device. The Settings UI will also need to just treat the GTBs as a background 
    // artifact when it comes to MIDI 1 devices.

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
        // Instead, the customer will need to use one of the other provided naming options.

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
        if (Feature_Servicing_MIDI2UnicodeConversion::IsEnabled())
        {
            // Formatting narrow and then widening a byte at a time turned each non-ASCII
            // character into its separate UTF-8 bytes, so a name like "Pete\u2019s MacBook Pro"
            // came out with three garbage characters. Staying wide has no conversion to get
            // wrong. Truncation is by character for the same reason.
            std::wstring formatted{ };

            if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
            {
                formatted = std::format(L"MIDIIN{} ({})", portIndexWithinThisFilterAndDirection + 1, generatedName);
            }
            else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
            {
                formatted = std::format(L"MIDIOUT{} ({})", portIndexWithinThisFilterAndDirection + 1, generatedName);
            }
            else
            {
                // unexpected
                return generatedName;
            }

            if (formatted.length() > MAXPNAMELEN - 1)
            {
                formatted.resize(MAXPNAMELEN - 1);

                // never leave a lead surrogate without its trail
                if (!formatted.empty() && IS_HIGH_SURROGATE(formatted.back()))
                {
                    formatted.pop_back();
                }
            }

            return WindowsMidiServicesInternal::TrimmedWStringCopy(formatted);
        }

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


std::wstring GenerateFilterPlusPinNameBasedMidi1PortName(
    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& pinName,
    _In_ uint8_t groupIndex,
    _In_ uint8_t portIndexWithinThisFilterAndDirection
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

    if (Feature_Servicing_MIDIPortDisambiguators::IsEnabled())
    {
        if (portIndexWithinThisFilterAndDirection > 0)
        {
            generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);
        }
    }
    else
    {
        generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);
    }

    return generatedName;
}

std::wstring GenerateFilterPlusBlockMidi1PortName(
    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    _In_ std::wstring const& filterName,
    _In_ std::wstring const& blockName,
    _In_ uint8_t groupIndex,
    _In_ uint8_t portIndexWithinThisFilterAndDirection
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


    if (Feature_Servicing_MIDIPortDisambiguators::IsEnabled())
    {
        if (portIndexWithinThisFilterAndDirection > 0)
        {
            generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);
        }
    }
    else
    {
        generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);
    }


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


bool WriteMidi1PortNameTableToPropertyDataPointer(
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
    additionalProperties.Append(STRING_PKEY_MIDI_Midi1PortNameTable);               // this gets used by FromDeviceInfo

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

    // if the property isn't present, we return an empty name table which can be used to create the entries
    return std::make_shared<MidiEndpointNameTable>();

}



_Use_decl_annotations_
std::shared_ptr<MidiEndpointNameTable> MidiEndpointNameTable::FromPropertyData(
    winrt::com_array<uint8_t> const& propertyData)
{
    std::shared_ptr<MidiEndpointNameTable> results = std::make_shared<MidiEndpointNameTable>();

    if (propertyData.data() == nullptr || propertyData.size() == 0)
    {
        // return an empty name table
        return results;
    }

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
        auto charCount = min(MAXPNAMELEN-1, name.size());

        memset(entry->CustomName, 0, MAXPNAMELEN);
        wcsncpy_s(entry->CustomName, MAXPNAMELEN, name.c_str(), charCount);

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
        auto charCount = min(MAXPNAMELEN - 1, name.size());

        memset(entry->CustomName, 0, MAXPNAMELEN);
        wcsncpy_s(entry->CustomName, MAXPNAMELEN, name.c_str(), charCount);

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

    // Only a table that was rebuilt has anything to say here. Everything else leaves the property
    // alone rather than overwriting it with a zero it did not calculate.
    if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
    {
        if (m_nameSourceFlagsValid)
        {
            destination.push_back({ { PKEY_MIDI_Midi1PortNameSourceFlags, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, (ULONG)sizeof(uint32_t), (PVOID)&m_nameSourceFlags });
        }

        if (m_hasLegacyEquivalentSet)
        {
            destination.push_back({ { PKEY_MIDI_Midi1PortNamesHaveLegacyEquivalent, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BOOLEAN, (ULONG)sizeof(DEVPROP_BOOLEAN), (PVOID)&m_hasLegacyEquivalent });
        }
    }

    m_nameTablePropertyData.clear();

    if (WriteMidi1PortNameTableToPropertyDataPointer(entries, m_nameTablePropertyData))
    {
        destination.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)m_nameTablePropertyData.size(), (PVOID)m_nameTablePropertyData.data() });

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
        groupIndex,
        portIndexWithinThisFilterAndDirection
    );

    std::wstring legacyWinMMName = GenerateLegacyMidi1PortName(
        parentDeviceName,
        filterName,
        flowFromUserPerspective,
        portIndexWithinThisFilterAndDirection
    );

    internal::SafeCopyWStringToFixedArray(entry->CustomName, MAXPNAMELEN, customName);
    internal::SafeCopyWStringToFixedArray(entry->LegacyWinMMName, MAXPNAMELEN, legacyWinMMName);
    internal::SafeCopyWStringToFixedArray(entry->NewStyleName, MAXPNAMELEN, newStyleName);

    entry->GroupIndex = groupIndex;
    entry->DataFlowFromUserPerspective = flowFromUserPerspective;

    if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
    {
        RecordPortInput(groupIndex, flowFromUserPerspective, blockName, L"", filterName);
    }

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
    uint8_t outIndex{ 0 };
    uint8_t inIndex{ 0 };

    for (auto const& gtb : blocks)
    {
        for (uint8_t groupIndex = gtb.FirstGroupIndex; groupIndex < gtb.FirstGroupIndex + gtb.GroupCount; groupIndex++)
        {
            std::wstring customName = L"";

            if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_OUTPUT)              // gtb output is a midi input (Source)
            {
                LOG_IF_FAILED(PopulateEntryForMidi1DeviceUsingUmpDriver(
                    groupIndex,
                    MidiFlow::MidiFlowIn,
                    customName,
                    parentDeviceName,
                    gtb.Name,
                    outIndex));

                outIndex++;
            }
            else if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_INPUT)           // block input is a MIDI output (Destination)
            {
                LOG_IF_FAILED(PopulateEntryForMidi1DeviceUsingUmpDriver(
                    groupIndex,
                    MidiFlow::MidiFlowOut,
                    customName,
                    parentDeviceName,
                    gtb.Name,
                    inIndex));

                inIndex++;
            }
            else
            {
                // should be no bidirectional GTBs for a MIDI 1.0 device
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
    std::wstring const& parentDeviceName,
    std::wstring const& blockName,
    uint8_t const portIndexWithinThisFilterAndDirection

) noexcept
{
    auto entry = std::make_shared<Midi1PortNameEntry>();

    // uses the block name directly, but only if not empty
    std::wstring newStyleName = GenerateFilterPlusBlockMidi1PortName(
        parentDeviceName,
        parentDeviceName,
        blockName,
        groupIndex,
        portIndexWithinThisFilterAndDirection
    );

    std::wstring legacyWinMMName = GenerateLegacyMidi1PortName(
        parentDeviceName,
        parentDeviceName,           // no filter here, so use the parent name again
        flowFromUserPerspective,
        portIndexWithinThisFilterAndDirection
    );

    internal::SafeCopyWStringToFixedArray(entry->CustomName, MAXPNAMELEN, customName);
    internal::SafeCopyWStringToFixedArray(entry->LegacyWinMMName, MAXPNAMELEN, legacyWinMMName);
    internal::SafeCopyWStringToFixedArray(entry->NewStyleName, MAXPNAMELEN, newStyleName);

    entry->GroupIndex = groupIndex;
    entry->DataFlowFromUserPerspective = flowFromUserPerspective;

    if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
    {
        RecordPortInput(groupIndex, flowFromUserPerspective, blockName, L"", parentDeviceName);
    }

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


    // TODO: The block name should be synchronized with this.

    // use the block name directly
    std::wstring newStyleName = GenerateFilterPlusPinNameBasedMidi1PortName(
        nameFromRegistry,
        filterName,
        pinName,
        groupIndex,
        portIndexWithinThisFilterAndDirection
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
void
MidiEndpointNameTable::RecordPortInput(
    uint8_t const groupIndex,
    MidiFlow const flowFromUserPerspective,
    std::wstring const& pinName,
    std::wstring const& driverRegistryName,
    std::wstring const& filterName) noexcept
{
    try
    {
        Midi1PortNameInput input{ };
        input.GroupIndex = groupIndex;
        input.DataFlowFromUserPerspective = flowFromUserPerspective;
        input.PinName = pinName;
        input.DriverRegistryName = driverRegistryName;
        input.FilterName = filterName;

        if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
        {
            m_sourcePortInputs[groupIndex] = input;
        }
        else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
        {
            m_destinationPortInputs[groupIndex] = input;
        }
    }
    CATCH_LOG();
}


void MidiEndpointNameTable::ResetPortInputs() noexcept
{
    m_sourcePortInputs.clear();
    m_destinationPortInputs.clear();
}


_Use_decl_annotations_
void MidiEndpointNameTable::SetPortNamesHaveLegacyEquivalent(bool const hasLegacyEquivalent) noexcept
{
    m_hasLegacyEquivalent = hasLegacyEquivalent ? DEVPROP_TRUE : DEVPROP_FALSE;
    m_hasLegacyEquivalentSet = true;
}


_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::RebuildNewStyleNames(
    std::wstring const& endpointName,
    bool const driverRegistryNamesArePerFilter) noexcept
{
    if (!Feature_Servicing_MIDI2PortNamingRework::IsEnabled()) return S_OK;

    try
    {
        std::vector<Midi1PortNameInput> inputs{ };

        for (auto const& input : m_sourcePortInputs) { inputs.push_back(input.second); }
        for (auto const& input : m_destinationPortInputs) { inputs.push_back(input.second); }

        if (inputs.empty()) return S_OK;

        auto results = BuildMidi1PortNamesForEndpoint(endpointName, driverRegistryNamesArePerFilter, inputs);

        for (auto const& result : results)
        {
            auto const& entries = result.DataFlowFromUserPerspective == MidiFlow::MidiFlowIn ?
                m_sourceEntries : m_destinationEntries;

            auto entry = entries.find(result.GroupIndex);
            if (entry == entries.end() || entry->second == nullptr) continue;

            internal::SafeCopyWStringToFixedArray(entry->second->NewStyleName, MAXPNAMELEN, result.Name);
        }

        m_nameSourceFlags = CalculateMidi1PortNameSourceFlags(endpointName, results);
        m_nameSourceFlagsValid = true;

        return S_OK;
    }
    CATCH_LOG();

    return E_FAIL;
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
    // Absence of the registry value is what selects Automatic, so a rollback lands exactly on the
    // legacy default that shipped.
    uint32_t const valueWhenUnset = Feature_Servicing_MIDI2PortNamingRework::IsEnabled() ?
        static_cast<uint32_t>(Midi1PortNameSelection::UseAutomatic) :
        MIDI_MIDI1_PORT_NAMING_DEFAULT_VALUE;

    DWORD defaultPortNamingForMidi1Drivers{ valueWhenUnset };

    if (SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, MIDI_MIDI1_PORT_NAMING_DEFAULT_REG_VALUE_NAME, &defaultPortNamingForMidi1Drivers)))
    {
        auto defaultMidi1PortNamingForByteDriverSelection = static_cast<Midi1PortNameSelection>(defaultPortNamingForMidi1Drivers);

        // make sure we don't get all recursive here
        if (defaultMidi1PortNamingForByteDriverSelection != Midi1PortNameSelection::UseGlobalDefault)
        {
            return defaultMidi1PortNamingForByteDriverSelection;
        }
    }

    return static_cast<Midi1PortNameSelection>(valueWhenUnset);
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

        switch (selection)
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
        return false;
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


// ========================================================================================================
// New-style name construction.
//
// These are net-new alongside the Generate* functions above, which are left exactly as they shipped
// so that turning Feature_Servicing_MIDI2PortNamingRework off restores the original behavior.
// ========================================================================================================

namespace
{
    constexpr size_t MidiMaxPortNameCharacters = MAXPNAMELEN - 1;

    // Words that mean nothing on their own, so a name made only of them is not a port name. Also
    // stripped from the end of a name before comparing two names for sameness.
    bool IsUninformativeWord(_In_ std::wstring const& lowercaseWord) noexcept
    {
        return
            lowercaseWord == L"midi" ||
            lowercaseWord == L"port" ||
            lowercaseWord == L"in" ||
            lowercaseWord == L"out" ||
            lowercaseWord == L"io" ||
            lowercaseWord == L"device";
    }

    std::vector<std::wstring> SplitIntoWords(_In_ std::wstring const& value) noexcept
    {
        std::vector<std::wstring> words{ };

        try
        {
            std::wstring current{ };

            for (auto const& ch : value)
            {
                if (iswspace(ch))
                {
                    if (!current.empty()) { words.push_back(current); current.clear(); }
                }
                else
                {
                    current += ch;
                }
            }

            if (!current.empty()) { words.push_back(current); }
        }
        CATCH_LOG();

        return words;
    }

    // Words for comparison. Punctuation that vendors use as a separator is treated as whitespace,
    // so "Yamaha USB-MIDI-1" compares as three words rather than one.
    std::vector<std::wstring> SplitIntoComparisonWords(_In_ std::wstring const& value) noexcept
    {
        std::vector<std::wstring> words{ };

        try
        {
            std::wstring current{ };

            for (auto const& ch : value)
            {
                if (iswspace(ch) || ch == L':' || ch == L',' || ch == L';' || ch == L'/' || ch == L'_' || ch == L'-')
                {
                    if (!current.empty()) { words.push_back(current); current.clear(); }
                }
                else
                {
                    current += ch;
                }
            }

            if (!current.empty()) { words.push_back(current); }
        }
        CATCH_LOG();

        return words;
    }

    // Lowercase, separator-insensitive, with trailing uninformative words removed. Two names with
    // the same comparison form say the same thing, so one of them is not worth showing.
    std::wstring ComparisonForm(_In_ std::wstring const& value) noexcept
    {
        std::wstring result{ };

        try
        {
            auto words = SplitIntoComparisonWords(WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(value));

            while (words.size() > 1 && IsUninformativeWord(words.back()))
            {
                words.pop_back();
            }

            for (auto const& word : words)
            {
                if (!result.empty()) { result += L" "; }
                result += word;
            }
        }
        CATCH_LOG();

        return result;
    }

    // The substring of value covering its first wordCount whitespace-delimited words, keeping the
    // original spacing rather than rebuilding it.
    std::wstring PrefixCoveringWords(_In_ std::wstring const& value, _In_ size_t const wordCount) noexcept
    {
        if (wordCount == 0) { return L""; }

        size_t words{ 0 };
        size_t position{ 0 };

        while (position < value.length())
        {
            while (position < value.length() && iswspace(value[position])) { position++; }

            size_t const start = position;

            while (position < value.length() && !iswspace(value[position])) { position++; }

            if (position > start)
            {
                words++;
                if (words == wordCount) { return value.substr(0, position); }
            }
        }

        return value;
    }

    // Cutting a name at a word boundary can leave the punctuation a vendor used as a separator
    // dangling on the end, as in "teVirtualMIDI -".
    std::wstring TrimSeparators(_In_ std::wstring const& value) noexcept
    {
        std::wstring result{ };

        try
        {
            result = WindowsMidiServicesInternal::TrimmedWStringCopy(value);

            auto isSeparator = [](wchar_t const ch)
                {
                    return iswspace(ch) || ch == L':' || ch == L',' || ch == L';' || ch == L'/' || ch == L'_' || ch == L'-';
                };

            while (!result.empty() && isSeparator(result.back())) { result.pop_back(); }
            while (!result.empty() && isSeparator(result.front())) { result.erase(result.begin()); }
        }
        CATCH_LOG();

        return result;
    }

    // The " (2)" a second unit of the same model carries. It belongs to the endpoint name, not to
    // anything the device said, so comparisons against what the device reported must ignore it.
    std::wstring RemoveDuplicateDeviceMarker(_In_ std::wstring const& deviceName) noexcept
    {
        std::wstring result{ };

        try
        {
            result = WindowsMidiServicesInternal::TrimmedWStringCopy(deviceName);

            // The legacy form is a leading "2- ", the new style form a trailing " (2)". Either way
            // it is something we added, so it is never information the device supplied.
            size_t hyphen{ 0 };
            while (hyphen < result.length() && iswdigit(result[hyphen])) { hyphen++; }

            if (hyphen > 0 && hyphen < result.length())
            {
                auto rest = result.substr(hyphen);
                auto trimmedRest = WindowsMidiServicesInternal::TrimmedWStringCopy(rest);

                if (trimmedRest.length() > 1 && trimmedRest.front() == L'-')
                {
                    auto remainder = WindowsMidiServicesInternal::TrimmedWStringCopy(trimmedRest.substr(1));
                    if (!remainder.empty()) { return remainder; }
                }
            }

            if (result.length() < 4) { return result; }
            if (result.back() != L')') { return result; }

            auto open = result.find_last_of(L'(');
            if (open == std::wstring::npos || open == 0) { return result; }

            auto digits = result.substr(open + 1, result.length() - open - 2);
            if (digits.empty()) { return result; }

            for (auto const ch : digits)
            {
                if (!iswdigit(ch)) { return result; }
            }

            return WindowsMidiServicesInternal::TrimmedWStringCopy(result.substr(0, open));
        }
        CATCH_LOG();

        return result;
    }
}

_Use_decl_annotations_
std::wstring ApplyLegacyDuplicateDeviceMarker(
    std::wstring const& baseDeviceName,
    uint32_t const oneBasedIndex) noexcept
{
    try
    {
        auto trimmed = WindowsMidiServicesInternal::TrimmedWStringCopy(baseDeviceName);

        if (oneBasedIndex < 2 || trimmed.empty()) { return trimmed; }

        return std::format(L"{0}- {1}", oneBasedIndex, trimmed);
    }
    CATCH_LOG();

    return baseDeviceName;
}

_Use_decl_annotations_
std::wstring RemoveGeneratedPinNameSuffix(std::wstring const& pinName) noexcept
{
    // Our USB and KS stack appends a bracketed index when it has to invent a pin name from the
    // filter name. The leading space matters: some devices really are named "[0]".
    return RemoveJustKSPinGeneratedSuffix(pinName);
}

_Use_decl_annotations_
bool IsPlaceholderPortName(std::wstring const& name) noexcept
{
    auto compare = ComparisonForm(name);

    if (compare.empty()) { return true; }

    auto words = SplitIntoComparisonWords(compare);
    if (words.empty()) { return true; }

    // a name made only of uninformative words, with or without a trailing number, says nothing
    for (auto const& word : words)
    {
        if (IsUninformativeWord(word)) { continue; }

        bool allDigits{ !word.empty() };
        for (auto const& ch : word)
        {
            if (!iswdigit(ch)) { allDigits = false; break; }
        }

        if (!allDigits) { return false; }
    }

    return true;
}

_Use_decl_annotations_
bool PortNameCarriesDeviceName(std::wstring const& portName, std::wstring const& deviceName) noexcept
{
    auto portWords = SplitIntoComparisonWords(ComparisonForm(portName));
    auto deviceWords = SplitIntoComparisonWords(ComparisonForm(deviceName));

    if (deviceWords.empty()) { return false; }

    // the device name at the front, which is what most devices that include it actually do
    if (portWords.size() >= deviceWords.size())
    {
        bool leads{ true };

        for (size_t i = 0; i < deviceWords.size(); i++)
        {
            if (portWords[i] != deviceWords[i]) { leads = false; break; }
        }

        if (leads) { return true; }
    }

    // or most of the device name present in some other arrangement, which is what RME does
    size_t matches{ 0 };

    for (auto const& deviceWord : deviceWords)
    {
        if (std::find(portWords.begin(), portWords.end(), deviceWord) != portWords.end()) { matches++; }
    }

    return (matches * 2 >= deviceWords.size());
}

_Use_decl_annotations_
std::wstring RemoveDeviceNamePrefixFromPortName(std::wstring const& portName, std::wstring const& deviceName) noexcept
{
    std::wstring result{ portName };

    try
    {
        auto portWords = SplitIntoComparisonWords(WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(portName));
        auto deviceWords = SplitIntoComparisonWords(WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(deviceName));

        size_t matched{ 0 };

        while (matched < deviceWords.size() && matched < portWords.size() && portWords[matched] == deviceWords[matched])
        {
            matched++;
        }

        // removing every word would leave nothing to distinguish the port
        if (matched == 0 || matched >= portWords.size()) { return result; }

        // find where the first word we are keeping starts in the original string, so punctuation
        // and spacing inside the kept part survive untouched
        size_t position{ 0 };
        size_t wordIndex{ 0 };
        bool inWord{ false };

        for (size_t i = 0; i < portName.length(); i++)
        {
            auto ch = portName[i];
            bool isSeparator = iswspace(ch) || ch == L':' || ch == L',' || ch == L';' || ch == L'/' || ch == L'_' || ch == L'-';

            if (!isSeparator && !inWord)
            {
                if (wordIndex == matched) { position = i; break; }
                inWord = true;
                wordIndex++;
            }
            else if (isSeparator)
            {
                inWord = false;
            }
        }

        if (position > 0)
        {
            result = WindowsMidiServicesInternal::TrimmedWStringCopy(portName.substr(position));
        }
    }
    CATCH_LOG();

    return result;
}

_Use_decl_annotations_
std::wstring ShortenDeviceNameToFit(std::wstring const& deviceName, size_t const maxCharacters) noexcept
{
    std::wstring result{ };

    try
    {
        if (maxCharacters == 0) { return result; }

        auto trimmed = WindowsMidiServicesInternal::TrimmedWStringCopy(deviceName);
        if (trimmed.length() <= maxCharacters) { return trimmed; }

        auto words = SplitIntoWords(trimmed);
        if (words.empty()) { return result; }

        // as many whole words from the front as fit
        std::wstring candidate{ };

        for (auto const& word : words)
        {
            auto next = candidate.empty() ? word : candidate + L" " + word;
            if (next.length() > maxCharacters) { break; }
            candidate = next;
        }

        candidate = TrimSeparators(candidate);
        if (!candidate.empty()) { return candidate; }

        // nothing from the front fits, so keep the end instead. For "Montage M8x" that is the
        // model, which identifies the device better than the family name would.
        for (size_t start = 1; start < words.size(); start++)
        {
            std::wstring tail{ };

            for (size_t i = start; i < words.size(); i++)
            {
                tail = tail.empty() ? words[i] : tail + L" " + words[i];
            }

            tail = TrimSeparators(tail);

            if (!tail.empty() && tail.length() <= maxCharacters) { return tail; }
        }
    }
    CATCH_LOG();

    return result;
}

_Use_decl_annotations_
std::wstring TruncateWithoutSplittingCharacters(std::wstring const& value, size_t const maxCharacters) noexcept
{
    std::wstring result{ value };

    try
    {
        if (result.length() > maxCharacters)
        {
            result.resize(maxCharacters);

            // never leave a lead surrogate without its trail
            if (!result.empty() && IS_HIGH_SURROGATE(result.back()))
            {
                result.pop_back();
            }
        }

        result = WindowsMidiServicesInternal::TrimmedWStringCopy(result);
    }
    CATCH_LOG();

    return result;
}

_Use_decl_annotations_
Midi1ResolvedPortName ResolveDeviceSuppliedPortName(
    std::wstring const& pinName,
    std::wstring const& driverRegistryName,
    bool const driverRegistryNameIsPerFilter,
    std::wstring const& filterName,
    std::wstring const& deviceName) noexcept
{
    Midi1ResolvedPortName resolved{ };

    try
    {
        // The device name arrives here with any duplicate marker already removed, so the
        // candidates have to have theirs removed too. Otherwise a second unit of the same model
        // sees its own name back as if the device had supplied a port name.
        auto isJustTheDeviceName = [](std::wstring const& candidate, std::wstring const& device) noexcept
            {
                return ComparisonForm(RemoveDuplicateDeviceMarker(candidate)) ==
                    ComparisonForm(RemoveDuplicateDeviceMarker(device));
            };

        // 1. the jack name, unless it is empty, a placeholder, or our own stack's "<filter> [n]"
        auto pin = RemoveGeneratedPinNameSuffix(pinName);

        if (!IsPlaceholderPortName(pin) && !isJustTheDeviceName(pin, filterName))
        {
            resolved.Name = pin;
            resolved.Source = Midi1PortNameSource::Pin;
            return resolved;
        }

        // 2. the driver's MediaCategories name, but only when the driver gives each filter its own
        //    entry. A single device-wide entry is shared by every unit of that model, so its value
        //    is whichever unit wrote it last.
        auto registryName = WindowsMidiServicesInternal::TrimmedWStringCopy(driverRegistryName);

        if (driverRegistryNameIsPerFilter &&
            !IsPlaceholderPortName(registryName) &&
            !isJustTheDeviceName(registryName, deviceName))
        {
            resolved.Name = registryName;
            resolved.Source = Midi1PortNameSource::DriverRegistry;
            return resolved;
        }

        // 3. the filter name, when it is not simply the device name again
        auto filter = WindowsMidiServicesInternal::TrimmedWStringCopy(filterName);

        if (!IsPlaceholderPortName(filter) && !isJustTheDeviceName(filter, deviceName))
        {
            resolved.Name = filter;
            resolved.Source = Midi1PortNameSource::Filter;
            return resolved;
        }
    }
    CATCH_LOG();

    return resolved;
}

namespace
{
    // The device name and port name combined, cut down to fit, with the group suffix already
    // accounted for by the caller's budget.
    std::wstring ComposeAndFit(
        _In_ std::wstring const& deviceName,
        _In_ std::wstring const& portName,
        _In_ bool const dropDeviceName,
        _In_ size_t const budget,
        _In_ std::wstring const& duplicateMarker) noexcept
    {
        std::wstring result{ };

        try
        {
            if (portName.empty())
            {
                return TruncateWithoutSplittingCharacters(deviceName, budget);
            }

            auto carries = PortNameCarriesDeviceName(portName, deviceName);

            if (carries)
            {
                // The device name is about to be dropped, and on a second unit of the same model
                // it is the only thing carrying the marker, so the marker moves onto the port name.
                auto const innerBudget = budget > duplicateMarker.length() ? budget - duplicateMarker.length() : 0;

                if (portName.length() <= innerBudget) { return portName + duplicateMarker; }

                // the device name is in there twice over once we add our own, so take the repeated
                // part out and put back as much of the device name as still fits
                auto remainder = RemoveDeviceNamePrefixFromPortName(portName, deviceName);

                if (remainder.length() < innerBudget)
                {
                    auto shortened = ShortenDeviceNameToFit(deviceName, innerBudget - remainder.length() - 1);
                    if (!shortened.empty()) { return shortened + L" " + remainder + duplicateMarker; }
                }

                return TruncateWithoutSplittingCharacters(remainder, innerBudget) + duplicateMarker;
            }

            if (!dropDeviceName)
            {
                auto composed = WindowsMidiServicesInternal::TrimmedWStringCopy(deviceName + L" " + portName);
                if (composed.length() <= budget) { return composed; }
            }

            // the port name on its own still identifies the port
            if (portName.length() <= budget) { return portName; }

            auto shortened = ShortenDeviceNameToFit(deviceName, budget > portName.length() ? budget - portName.length() - 1 : 0);
            if (!shortened.empty() && portName.length() < budget) { return shortened + L" " + portName; }

            return TruncateWithoutSplittingCharacters(portName, budget);
        }
        CATCH_LOG();

        return result;
    }
}

_Use_decl_annotations_
std::vector<Midi1PortNameResult> BuildMidi1PortNamesForEndpoint(
    std::wstring const& endpointName,
    bool const driverRegistryNamesArePerFilter,
    std::vector<Midi1PortNameInput> const& ports) noexcept
{
    std::vector<Midi1PortNameResult> results{ };

    try
    {
        auto device = WindowsMidiServicesInternal::TrimmedWStringCopy(endpointName);

        // A port name that already carries the model is the one case where the endpoint name, and
        // so the marker a second unit of the same model was given, gets dropped during composition.
        auto deviceForComparison = RemoveDuplicateDeviceMarker(device);

        std::wstring duplicateMarker{ };

        if (deviceForComparison.length() < device.length())
        {
            duplicateMarker = device.substr(deviceForComparison.length());
        }

        for (auto const& flow : { MidiFlow::MidiFlowIn, MidiFlow::MidiFlowOut })
        {
            std::vector<Midi1PortNameInput> inDirection{ };

            for (auto const& port : ports)
            {
                if (port.DataFlowFromUserPerspective == flow) { inDirection.push_back(port); }
            }

            if (inDirection.empty()) { continue; }

            // resolve every port first, because the decisions below need the whole set
            std::vector<Midi1ResolvedPortName> resolved{ };

            for (auto const& port : inDirection)
            {
                resolved.push_back(ResolveDeviceSuppliedPortName(
                    port.PinName,
                    port.DriverRegistryName,
                    driverRegistryNamesArePerFilter,
                    port.FilterName,
                    deviceForComparison));
            }

            // A name that repeats across the whole direction tells the ports apart from other
            // devices but not from each other, so it is treated as if the device said nothing.
            bool namesDiffer{ false };

            for (size_t i = 1; i < resolved.size(); i++)
            {
                if (ComparisonForm(resolved[i].Name) != ComparisonForm(resolved[0].Name)) { namesDiffer = true; break; }
            }

            if (!namesDiffer)
            {
                for (auto& item : resolved)
                {
                    if (ComparisonForm(item.Name) == ComparisonForm(deviceForComparison))
                    {
                        item.Name.clear();
                        item.Source = Midi1PortNameSource::None;
                    }
                }
            }

            // Decide once for the whole direction whether the device name is affordable, so one
            // long port name cannot leave some ports carrying it and others not.
            bool dropDeviceName{ false };

            for (auto const& item : resolved)
            {
                if (item.Name.empty()) { continue; }
                if (PortNameCarriesDeviceName(item.Name, device)) { continue; }

                if ((device.length() + 1 + item.Name.length()) > MidiMaxPortNameCharacters)
                {
                    dropDeviceName = true;
                    break;
                }
            }

            // Numbering is needed when two ports would otherwise end up with the same name.
            std::vector<std::wstring> unnumbered{ };

            for (size_t i = 0; i < resolved.size(); i++)
            {
                unnumbered.push_back(ComposeAndFit(device, resolved[i].Name, dropDeviceName, MidiMaxPortNameCharacters, duplicateMarker));
            }

            bool numberingNeeded{ false };

            for (size_t i = 0; i < unnumbered.size() && !numberingNeeded; i++)
            {
                for (size_t j = i + 1; j < unnumbered.size(); j++)
                {
                    if (WindowsMidiServicesInternal::ToUpperWStringCopy(unnumbered[i]) ==
                        WindowsMidiServicesInternal::ToUpperWStringCopy(unnumbered[j]))
                    {
                        numberingNeeded = true;
                        break;
                    }
                }
            }

            for (size_t i = 0; i < inDirection.size(); i++)
            {
                Midi1PortNameResult result{ };

                result.GroupIndex = inDirection[i].GroupIndex;
                result.DataFlowFromUserPerspective = flow;
                result.Resolved = resolved[i];

                if (numberingNeeded)
                {
                    // "group" is never localized. Applications match on these strings, so a name
                    // that changed with the system language would break them.
                    std::wstring suffix{ L" group " + std::to_wstring(static_cast<int>(inDirection[i].GroupIndex) + 1) };

                    result.Name = ComposeAndFit(
                        device,
                        resolved[i].Name,
                        dropDeviceName,
                        MidiMaxPortNameCharacters - suffix.length(),
                        duplicateMarker) + suffix;
                }
                else
                {
                    result.Name = unnumbered[i];
                }

                if (result.Name.empty())
                {
                    result.Name = TruncateWithoutSplittingCharacters(device, MidiMaxPortNameCharacters);
                }

                results.push_back(result);
            }
        }
    }
    CATCH_LOG();

    return results;
}

_Use_decl_annotations_
uint32_t CalculateMidi1PortNameSourceFlags(
    std::wstring const& endpointName,
    std::vector<Midi1PortNameResult> const& results) noexcept
{
    uint32_t flags{ MIDI_MIDI1_PORT_NAME_SOURCE_NONE };

    try
    {
        if (results.empty()) { return flags; }

        size_t named{ 0 };
        size_t carryingDeviceName{ 0 };
        std::vector<std::wstring> seen{ };
        bool distinct{ true };

        for (auto const& result : results)
        {
            if (result.Resolved.Source == Midi1PortNameSource::None) { continue; }

            named++;

            if (PortNameCarriesDeviceName(result.Resolved.Name, endpointName)) { carryingDeviceName++; }

            auto compare = ComparisonForm(result.Resolved.Name);

            if (std::find(seen.begin(), seen.end(), compare) != seen.end()) { distinct = false; }
            else { seen.push_back(compare); }
        }

        if (named > 0)
        {
            flags |= MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED;

            if (named == results.size()) { flags |= MIDI_MIDI1_PORT_NAME_SOURCE_ALL_PORTS_NAMED; }
            if (distinct) { flags |= MIDI_MIDI1_PORT_NAME_SOURCE_NAMES_ARE_DISTINCT; }
            if (carryingDeviceName == named) { flags |= MIDI_MIDI1_PORT_NAME_SOURCE_NAMES_CONTAIN_DEVICE; }
        }
    }
    CATCH_LOG();

    return flags;
}


_Use_decl_annotations_
std::wstring RecoverModelNameFromPortNames(
    std::wstring const& deviceName,
    bool const driverRegistryNamesArePerFilter,
    std::vector<Midi1PortNameInput> const& ports) noexcept
{
    try
    {
        if (ports.size() < 2) { return deviceName; }

        // A second unit of the same model arrives here already marked. Recovery replaces the part
        // before the marker, so the marker has to be put back or both units get the same name.
        auto const trimmedDeviceName = WindowsMidiServicesInternal::TrimmedWStringCopy(deviceName);
        auto const baseDeviceName = RemoveDuplicateDeviceMarker(trimmedDeviceName);

        std::wstring duplicateMarker{ };

        if (baseDeviceName.length() < trimmedDeviceName.length())
        {
            duplicateMarker = trimmedDeviceName.substr(baseDeviceName.length());
        }

        std::vector<std::wstring> portNames{ };

        for (auto const& port : ports)
        {
            auto resolved = ResolveDeviceSuppliedPortName(
                port.PinName,
                port.DriverRegistryName,
                driverRegistryNamesArePerFilter,
                port.FilterName,
                baseDeviceName);

            // one port with nothing to say means there is no run to find
            if (resolved.Source == Midi1PortNameSource::None) { return deviceName; }

            portNames.push_back(resolved.Name);
        }

        auto runWords = SplitIntoWords(portNames.front());

        for (size_t i = 1; i < portNames.size() && !runWords.empty(); i++)
        {
            auto words = SplitIntoWords(portNames[i]);

            size_t common{ 0 };

            while (common < runWords.size() && common < words.size() &&
                WindowsMidiServicesInternal::ToLowerWStringCopy(runWords[common]) ==
                WindowsMidiServicesInternal::ToLowerWStringCopy(words[common]))
            {
                common++;
            }

            runWords.resize(common);
        }

        // "Express  128: Port 1".."Port 8" share "Express  128: Port"; the part that identifies the
        // model is what is left once the words common to any port name are dropped.
        while (!runWords.empty() &&
            IsUninformativeWord(WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(runWords.back())))
        {
            runWords.pop_back();
        }

        if (runWords.empty()) { return deviceName; }

        // a run that is the whole of a port name leaves nothing to tell the ports apart, so it is
        // the device saying one thing many times rather than a model name
        if (runWords.size() >= SplitIntoWords(portNames.front()).size()) { return deviceName; }

        auto candidate = TrimSeparators(PrefixCoveringWords(portNames.front(), runWords.size()));

        if (candidate.empty()) { return deviceName; }

        // If the run repeats something the device name already says, it is the same model stated
        // twice rather than information the description is missing. RME is the case for this.
        auto deviceWords = SplitIntoComparisonWords(WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(baseDeviceName));
        auto candidateWords = SplitIntoComparisonWords(WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(candidate));

        for (auto const& candidateWord : candidateWords)
        {
            if (IsUninformativeWord(candidateWord)) { continue; }

            for (auto const& deviceWord : deviceWords)
            {
                if (candidateWord == deviceWord) { return deviceName; }
            }
        }

        return candidate + duplicateMarker;
    }
    CATCH_LOG();

    return deviceName;
}


_Use_decl_annotations_
Midi1PortNameSelection ResolveAutomaticPortNameSelection(
    bool const hasLegacyEquivalent,
    uint32_t const nameSourceFlags) noexcept
{
    // Nothing existed under these names before, so there is no compatibility to preserve.
    if (!hasLegacyEquivalent) { return Midi1PortNameSelection::UseNewStyleName; }

    // Only rename a port when the device said something about it the old name did not carry.
    if ((nameSourceFlags & MIDI_MIDI1_PORT_NAME_SOURCE_DEVICE_SUPPLIED) != 0)
    {
        return Midi1PortNameSelection::UseNewStyleName;
    }

    return Midi1PortNameSelection::UseLegacyWinMM;
}


_Use_decl_annotations_
HRESULT
MidiEndpointNameTable::WriteGroupTerminalBlockProperties(
    winrt::hstring const& endpointDeviceId,
    std::vector<DEVPROPERTY>& destination) noexcept
{
    if (!Feature_Servicing_MIDI2PortNamingRework::IsEnabled()) return S_OK;

    try
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_GroupTerminalBlocks);

        winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo{ nullptr };

        try
        {
            deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                endpointDeviceId, additionalProperties).get();
        }
        catch (winrt::hresult_error const&)
        {
            return S_OK;
        }

        if (deviceInfo == nullptr) return S_OK;

        auto refArray = internal::SafeGetSwdBinaryPropertyFromDeviceInformation(
            STRING_PKEY_MIDI_GroupTerminalBlocks, deviceInfo);

        if (refArray == nullptr) return S_OK;

        auto refData = refArray.Value();
        if (refData.data() == nullptr || refData.size() == 0) return S_OK;

        auto blocks = internal::ReadGroupTerminalBlocksFromPropertyData(refData.data(), (uint32_t)refData.size());
        if (blocks.empty()) return S_OK;

        bool changed{ false };

        for (auto& block : blocks)
        {
            // The stored block name is never read back in here. The replacement comes from the name
            // table, which is derived from the device, so repeated edits cannot compound.
            std::shared_ptr<Midi1PortNameEntry> entry{ nullptr };

            if (block.Direction == MIDI_GROUP_TERMINAL_BLOCK_OUTPUT)         // block output is a MIDI source
            {
                entry = GetSourceEntry(block.FirstGroupIndex);
            }
            else if (block.Direction == MIDI_GROUP_TERMINAL_BLOCK_INPUT)     // block input is a MIDI destination
            {
                entry = GetDestinationEntry(block.FirstGroupIndex);
            }
            else
            {
                entry = GetSourceEntry(block.FirstGroupIndex);

                if (entry == nullptr)
                {
                    entry = GetDestinationEntry(block.FirstGroupIndex);
                }
            }

            if (entry == nullptr) continue;

            std::wstring updated{ entry->CustomName[0] != 0 ? entry->CustomName : entry->NewStyleName };

            updated = internal::TrimmedWStringCopy(updated);

            if (updated.empty() || updated == block.Name) continue;

            block.Name = updated;
            changed = true;
        }

        if (!changed) return S_OK;

        m_groupTerminalBlockPropertyData.clear();

        if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, m_groupTerminalBlockPropertyData))
        {
            destination.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BINARY,
                (ULONG)m_groupTerminalBlockPropertyData.size(),
                (PVOID)m_groupTerminalBlockPropertyData.data() });
        }

        return S_OK;
    }
    CATCH_LOG();

    return S_OK;
}


}