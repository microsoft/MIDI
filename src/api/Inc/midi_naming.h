// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_NAMING_H
#define MIDI_NAMING_H

#include <string>
#include <vector>

namespace WindowsMidiServicesInternal::Midi1PortNaming
{

    // we always write the total size in bytes (size_t), and then a number of these entries

#define MIDI1_PORT_NAME_ENTRY_HEADER_SIZE (sizeof(size_t))

    // this needs to be kept in sync with Midi1PortNameTableEntry in SDK

    struct Midi1PortNameEntry
    {
        uint8_t GroupIndex{ 0 };
        MidiFlow DataFlowFromUserPerspective{ MidiFlow::MidiFlowIn };   // MidiFlowIn is 0

        wchar_t CustomName[MAXPNAMELEN]{ 0 };
        wchar_t LegacyWinMMName[MAXPNAMELEN]{ 0 };
        wchar_t PinName[MAXPNAMELEN]{ 0 };
        wchar_t FilterPlusPinName[MAXPNAMELEN]{ 0 };
        wchar_t BlockName[MAXPNAMELEN]{ 0 };                // GTB or FB
        wchar_t FilterPlusBlockName[MAXPNAMELEN]{ 0 };      // Filter + GTB or FB
    };

// max of 32 total inputs/outputs
#define MAX_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) * 32 + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)
#define MIN_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)


    inline std::wstring RemoveJustKSPinGeneratedSuffix(
        _In_ std::wstring const& pinName
    )
    {
        std::wstring cleanedPinName{ internal::TrimmedWStringCopy(pinName) };

        std::wstring suffixesToRemove[] =
        {
            // In every case I've seen, these are added by our USB and KS stack, not by the device
            // Originally this went only to 16, but there are vendor driver devices with 32 total ports
            // that can be configured to be any combo of inputs and outputs, which is weird, and
            // potentially breaks our MIDI 2 implementation
            L"[0]", L"[1]", L"[2]", L"[3]", L"[4]", L"[5]", L"[6]", L"[7]", L"[8]", 
            L"[9]", L"[10]", L"[11]", L"[12]", L"[13]", L"[14]", L"[15]", L"[16]",
            L"[17]", L"[18]", L"[19]", L"[20]", L"[21]", L"[22]", L"[23]", L"[24]",
            L"[25]", L"[26]", L"[27]", L"[28]", L"[29]", L"[30]", L"[31]", L"[32]",
        };

        for (auto const& word : suffixesToRemove)
        {
            if (cleanedPinName.ends_with(word))
            {
                cleanedPinName = cleanedPinName.substr(0, cleanedPinName.length() - word.length());
            }
        }

        return internal::TrimmedWStringCopy(cleanedPinName);
    }

    inline std::wstring FullyCleanupKSPinName(
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

        return internal::TrimmedWStringCopy(cleanedPinName);
    }

    inline std::wstring FullyCleanupBlockName(
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

        return internal::TrimmedWStringCopy(cleanedBlockName);
    }


    inline bool StringEndsWithNumber(_In_ std::wstring s, _In_ uint16_t numberToFind)
    {
        // TODO: Check to see if there's a number at the end of the string (take the last numeric values) and compare *whole* value

        // Need to ensure "YAMAHA DX-MULTI 12" returns true only for 12, not for 2, 
        // for example. That's why wstring.ends_with is insufficient
        // also needs to support padded numbers "Port 02" should match the value "2"

        auto lastIndex = s.find_last_not_of(L"0123456789");

        if (lastIndex != std::wstring::npos)
        {
            if (lastIndex + 1 <= s.length())
            {
                auto trailingNumber = s.substr(lastIndex + 1);

                int i = _wtoi(trailingNumber.c_str());

                // step 3: compare to the provided number
                if (i == numberToFind)
                {
                    return true;
                }
            }
        }

        return false;
    }


    inline std::wstring AddGroupNumberToNameIfNeeded(
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

                if (!StringEndsWithNumber(newName, groupIndex + 1))
                {
                    auto groupNumber = std::to_wstring(groupIndex + 1);

                    const int nameReservedSpaces = groupNumber.length() + 2; // null terminator, space, and then up to two digits for the group number

                    newName = generatedName.substr(0, MAXPNAMELEN - nameReservedSpaces) + L" " + groupNumber;
                }
            }
        }

        return internal::TrimmedWStringCopy(newName);
    }


    inline std::wstring GenerateLegacyMidi1PortName(
        _In_ std::wstring const& nameFromRegistry,              
        _In_ std::wstring const& filterName,
        _In_ MidiFlow const flowFromUserPerspective,
        _In_ uint8_t const portIndexWithinThisFilterAndDirection
    )
    {
        std::wstring generatedName{};

        if (!internal::TrimmedWStringCopy(nameFromRegistry).empty())
        {
            // If name from registry is not blank, use that first
            // NOTE: There's an existing issue in WinMM that causes two of the same make/model of
            // device to have the same name, even if they report different names, because they 
            // share the same registry entry. To maintain compatibility, we cannot fix that here
            // Instead, the custom will need to use one of the other provided naming options.

            generatedName = internal::TrimmedWStringCopy(nameFromRegistry).substr(0, MAXPNAMELEN - 1);
        }
        else
        {
            // If registry name is empty, use the device friendly name (filter name in this case)
            generatedName = internal::TrimmedWStringCopy(filterName).substr(0, MAXPNAMELEN - 1);
        }

        generatedName = internal::TrimmedWStringCopy(generatedName);

        // if this is not the first port for this filter, instance prefix with MIDIIN/OUT #

        if (portIndexWithinThisFilterAndDirection > 0)
        {
            // switching back and forth between wstring and string here is probably not a great idea, but the original
            // values from USB should all be narrow standard strings anyway.
            if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
            {
                auto formatted = std::format("MIDIIN{} ({})", portIndexWithinThisFilterAndDirection + 1, winrt::to_string(generatedName));
                return internal::TrimmedWStringCopy(std::wstring(formatted.begin(), formatted.end()).substr(0, MAXPNAMELEN-1));
            }
            else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
            {
                auto formatted = std::format("MIDIOUT{} ({})", portIndexWithinThisFilterAndDirection + 1, winrt::to_string(generatedName));
                return internal::TrimmedWStringCopy(std::wstring(formatted.begin(), formatted.end()).substr(0, MAXPNAMELEN - 1));
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

    inline std::wstring GeneratePinNameBasedMidi1PortName(
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& pinName,
        _In_ MidiFlow const flowFromUserPerspective,
        _In_ uint8_t const portIndexWithinThisFilterAndDirection
    )
    {
        UNREFERENCED_PARAMETER(filterName);
        UNREFERENCED_PARAMETER(flowFromUserPerspective);
        UNREFERENCED_PARAMETER(portIndexWithinThisFilterAndDirection);

        std::wstring generatedName{ RemoveJustKSPinGeneratedSuffix(pinName) };

        // we use the pin name exactly as it is in the device
        generatedName = generatedName.substr(0, MAXPNAMELEN - 1);

        return internal::TrimmedWStringCopy(generatedName);
    }

    inline std::wstring GenerateFilterPlusPinNameBasedMidi1PortName(
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& pinName,
        _In_ uint8_t groupIndex
    )
    {
        std::wstring generatedName{};

        auto cleanedPinName = FullyCleanupKSPinName(pinName, parentDeviceName, filterName);

        generatedName = internal::TrimmedWStringCopy(filterName + L" " + internal::TrimmedWStringCopy(cleanedPinName));

        // if the name is too long, try using just the pin name or just the filter name

        if (generatedName.length() + 1 > MAXPNAMELEN)
        {
            if (!cleanedPinName.empty())
            {
                // we're over length, so just use the pin name
                generatedName = internal::TrimmedWStringCopy(cleanedPinName.substr(0, MAXPNAMELEN - 1));
            }
            else
            {
                // we're over length, and there's no pin name
                // so we use the filter name
                generatedName = internal::TrimmedWStringCopy(filterName.substr(0, MAXPNAMELEN - 1));
            }
        }

        generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);

        return generatedName;
    }


    inline std::wstring GenerateFilterPlusBlockMidi1PortName(
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& blockName,
        _In_ uint8_t groupIndex
    )
    {
        std::wstring generatedName{};

        auto cleanedBlockName = FullyCleanupBlockName(blockName, parentDeviceName, filterName);

        generatedName = internal::TrimmedWStringCopy(filterName + L" " + internal::TrimmedWStringCopy(cleanedBlockName));

        // if the name is too long, try using just the pin name or just the filter name

        if (generatedName.length() + 1 > MAXPNAMELEN)
        {
            if (!cleanedBlockName.empty())
            {
                // we're over length, so just use the gtb name
                generatedName = internal::TrimmedWStringCopy(cleanedBlockName.substr(0, MAXPNAMELEN - 1));
            }
            else
            {
                // we're over length, and there's no gtb name
                // so we use the filter name
                generatedName = internal::TrimmedWStringCopy(filterName.substr(0, MAXPNAMELEN - 1));
            }
        }

        generatedName = AddGroupNumberToNameIfNeeded(parentDeviceName, filterName, generatedName, groupIndex);

        return generatedName;
    }


    inline void PopulateMidi1PortNameEntryNames(
        _In_ Midi1PortNameEntry& entry,
        _In_ std::wstring const& nameFromRegistry,
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& pinName,
        _In_ std::wstring const& customPortName,
        _In_ MidiFlow const flowFromUserPerspective,
        _In_ uint8_t const portIndexWithinThisFilterAndDirection,
        _In_ uint8_t const groupIndex
    )
    {
        // classic WinMM name. Not great, and sometimes buggy
        auto legacyWinMMName = GenerateLegacyMidi1PortName(
            nameFromRegistry,
            filterName,
            flowFromUserPerspective,
            portIndexWithinThisFilterAndDirection
        );
        legacyWinMMName.copy(entry.LegacyWinMMName, MAXPNAMELEN - 1);

        // Uses device and iJack info to create the name
        auto interfacePlusPinWinMMName = GenerateFilterPlusPinNameBasedMidi1PortName(
            parentDeviceName,
            filterName,
            pinName,
            groupIndex
        );
        interfacePlusPinWinMMName.copy(entry.FilterPlusPinName, MAXPNAMELEN - 1);

        // Uses only the pin/iJack info to name the pin
        auto pinWinMMName = GeneratePinNameBasedMidi1PortName(
            filterName,
            pinName,
            flowFromUserPerspective,
            portIndexWithinThisFilterAndDirection
        );
        pinWinMMName.copy(entry.PinName, MAXPNAMELEN - 1);

        // User-supplied name
        if (!customPortName.empty())
        {
            customPortName.copy(entry.CustomName, MAXPNAMELEN - 1);
        }

        // GTB Name. We should set this later based on the user preference
        pinWinMMName.copy(entry.BlockName, MAXPNAMELEN - 1);

        auto filterPlusBlockName = GenerateFilterPlusBlockMidi1PortName(
            parentDeviceName, 
            filterName, 
            pinName,
            groupIndex);
        filterPlusBlockName.copy(entry.FilterPlusBlockName, MAXPNAMELEN - 1);
    }

    inline std::vector<Midi1PortNameEntry> ReadMidi1PortNameTableFromPropertyData(
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


}

#endif