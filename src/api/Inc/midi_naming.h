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

    struct Midi1PortNameEntry
    {
        uint8_t GroupIndex;
        MidiFlow DataFlowFromUserPerspective;           // a message destination is a MIDI Out

        wchar_t CustomName[MAXPNAMELEN]{ };
        wchar_t LegacyWinMMName[MAXPNAMELEN]{ };
        wchar_t PinName[MAXPNAMELEN]{ };
        wchar_t FilterPlusPinName[MAXPNAMELEN]{ };
        wchar_t GroupTerminalBlockName[MAXPNAMELEN]{ };
        wchar_t FilterPlusGroupTerminalBlockName[MAXPNAMELEN]{ };
    };

// max of 32 total inputs/outputs
#define MAX_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) * 32 + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)
#define MIN_PORT_NAME_TABLE_SIZE    (sizeof(Midi1PortNameEntry) + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE)



    inline std::wstring CleanupKSPinName(
        _In_ std::wstring const& pinName,
        _In_ std::wstring parentDeviceName,
        _In_ std::wstring filterName
    )
    {
        std::wstring cleanedPinName{};

        // Used by ESI, MOTU, and others. We don't want to mess up other names, so check only
        // for whole word. We do other removal in the next step

        auto checkPinName = internal::ToLowerTrimmedWStringCopy(pinName);

        if (checkPinName == L"midi" ||
            checkPinName == L"out" ||
            checkPinName == L"in" ||
            checkPinName == L"io"
            )
        {
            cleanedPinName = L"";
        }
        else
        {
            cleanedPinName = pinName;
        }

        cleanedPinName = internal::TrimmedWStringCopy(cleanedPinName);
        auto comparePinName = internal::ToUpperWStringCopy(cleanedPinName);         // this needs to be just the uppercase of cleanedPinName for the replace to work

        auto compareParentName = internal::ToUpperWStringCopy(parentDeviceName);
        auto compareFilterName = internal::ToUpperWStringCopy(filterName);

        // the double and triple space entries need to be last
        // there are other ways to do this with pattern matching, 
        // but just banging this through for this version
        // these must all be uppercase when alpha characters are included
        std::wstring wordsToRemove[] =
        {
            // many of these are added by our USB and KS stack, not by the device, which is why they are here
            compareParentName, compareFilterName,
            L"[0]", L"[1]", L"[2]", L"[3]", L"[4]", L"[5]", L"[6]", L"[7]", L"[8]", L"[9]", L"[10]", L"[11]", L"[12]", L"[13]", L"[14]", L"[15]", L"[16]",
            L"  ", L"   ", L"    "
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

        internal::InPlaceTrim(cleanedPinName);

        return cleanedPinName;
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


        // if this is not the first port for this filter, instance prefix with MIDIIN/OUT #

        if (portIndexWithinThisFilterAndDirection > 0)
        {
            // switching back and forth between wstring and string here is probably not a great idea, but the original
            // values from USB should all be narrow standard strings anyway.
            if (flowFromUserPerspective == MidiFlow::MidiFlowIn)
            {
                auto formatted = std::format("MIDIIN{} ({})", portIndexWithinThisFilterAndDirection + 1, winrt::to_string(generatedName));
                return std::wstring(formatted.begin(), formatted.end()).substr(0, MAXPNAMELEN-1);
            }
            else if (flowFromUserPerspective == MidiFlow::MidiFlowOut)
            {
                auto formatted = std::format("MIDIOUT{} ({})", portIndexWithinThisFilterAndDirection + 1, winrt::to_string(generatedName));
                return std::wstring(formatted.begin(), formatted.end()).substr(0, MAXPNAMELEN - 1);
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

        std::wstring generatedName{};

        // we use the pin name exactly as it is in the device
        generatedName = pinName.substr(0, MAXPNAMELEN - 1);

        return generatedName;
    }

    inline std::wstring GenerateDevicePlusPinNameBasedMidi1PortName(
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& pinName
    )
    {
        std::wstring generatedName{};

        auto cleanedPinName = CleanupKSPinName(pinName, parentDeviceName, filterName);

        generatedName = internal::TrimmedWStringCopy(filterName + L" " + internal::TrimmedWStringCopy(cleanedPinName));

        // if the name is too long, try using just the pin name or just the filter name

        if (generatedName.length() + 1 > MAXPNAMELEN)
        {
            if (!cleanedPinName.empty())
            {
                // we're over length, so just use the pin name
                generatedName = cleanedPinName.substr(0, MAXPNAMELEN - 1);
            }
            else
            {
                // we're over length, and there's no pin name
                // so we use the filter name
                generatedName = filterName.substr(0, MAXPNAMELEN - 1);
            }
        }

        // TODO: do we need to do any port differentiators here? Look at the collection and see
        // if there are already ports starting with the same name, and if so, increment a counter and append

        return generatedName;
    }


    inline std::wstring GenerateFilterPlusGroupTerminalBlockMidi1PortName(
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& groupTerminalBlockName
    )
    {
        std::wstring generatedName{};

        auto cleanedGtbName = CleanupKSPinName(groupTerminalBlockName, parentDeviceName, filterName);

        generatedName = internal::TrimmedWStringCopy(filterName + L" " + internal::TrimmedWStringCopy(cleanedGtbName));

        // if the name is too long, try using just the pin name or just the filter name

        if (generatedName.length() + 1 > MAXPNAMELEN)
        {
            if (!cleanedGtbName.empty())
            {
                // we're over length, so just use the gtb name
                generatedName = cleanedGtbName.substr(0, MAXPNAMELEN - 1);
            }
            else
            {
                // we're over length, and there's no gtb name
                // so we use the filter name
                generatedName = filterName.substr(0, MAXPNAMELEN - 1);
            }
        }

        // TODO: do we need to do any port differentiators here? Look at the collection and see
        // if there are already ports starting with the same name, and if so, increment a counter and append

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
        _In_ uint8_t const portIndexWithinThisFilterAndDirection
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
        auto interfacePlusPinWinMMName = GenerateDevicePlusPinNameBasedMidi1PortName(
            parentDeviceName,
            filterName,
            pinName
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
        pinWinMMName.copy(entry.GroupTerminalBlockName, MAXPNAMELEN - 1);

        auto filterPlusGroupTerminalBlockName = GenerateFilterPlusGroupTerminalBlockMidi1PortName(
            parentDeviceName, 
            filterName, 
            pinName);
        filterPlusGroupTerminalBlockName.copy(entry.FilterPlusGroupTerminalBlockName, MAXPNAMELEN - 1);

    }

    //inline std::wstring GenerateGroupTerminalBlockNameFromDeviceInformation(
    //    _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
    //    _In_ std::wstring const& filterName,
    //    _In_ std::wstring const& existingBlockName,             // devices using the new driver come with a GTB name already, but we may need to clean it
    //    _In_ uint8_t const& groupIndex
    //)
    //{
    //    UNREFERENCED_PARAMETER(parentDeviceName);
    //    UNREFERENCED_PARAMETER(existingBlockName);
    //    UNREFERENCED_PARAMETER(filterName);
    //    UNREFERENCED_PARAMETER(groupIndex);

    //    std::wstring generatedName{};



    //    return generatedName;
    //}


    inline std::wstring GenerateMidi1PortNameFromCreatedUmpEndpoint(
        _In_ bool const useOldStyleNamingForNonUmpDevice,       // this comes from the property on the device, and if not specified, from the registry. Controls using old WinMM-style naming
        _In_ std::wstring const& customPortName,                // if the user has supplied a name for the generated port, and we're not using old-style naming, this wins
        _In_ std::wstring const& blockName,                     // group terminal block or function block
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& deviceManufacturerName,        // manufacturer name, if we have it
        _In_ std::wstring const& transportSuppliedEndpointName, // the name of the parent endpoint as provided by the transport
        _In_ std::wstring const& customParentEndpointName,      // if the user has supplied a name for the generated port, and we're not using old-style naming, this wins
        _In_ bool const forceUseExactBlockName,    // true for transports that already put the appropriate name in the GTB, like the KSA transport
        _In_ uint8_t const groupIndex,
        _In_ MidiFlow const flowFromUserPerspective,
        _In_ bool const isNativeUmpDevice,
        _In_ bool const truncateToWinMMLimit
    )
    {
        UNREFERENCED_PARAMETER(flowFromUserPerspective);
        //UNREFERENCED_PARAMETER(otherExistingMidi1PortNamesForThisDeviceAndFlow);

        // KSA transport pre-calculates the MIDI 1 port names based upon 
        // information from the USB device, which is not necessarily available
        // at the time this function is called. So when using that transport's
        // devices, we flag to just use the gtb name as it is.
        if (forceUseExactBlockName && !blockName.empty())
        {
            return truncateToWinMMLimit ? blockName.substr(0, MAXPNAMELEN - 1) : blockName;
        }

        // user supplied a port name, so it is what we prefer
        // if we're using old-style naming, we do not use
        // any user-supplied information for the name
        if (!customPortName.empty() && !useOldStyleNamingForNonUmpDevice)
        {
            return truncateToWinMMLimit ? customPortName.substr(0, MAXPNAMELEN - 1) : customPortName;
        }

        if (useOldStyleNamingForNonUmpDevice && !isNativeUmpDevice)
        {
            // TODO: Find the old naming code in the source tree, and reimplement it here

            std::wstring name;

            // TEMPORARY code
            name = transportSuppliedEndpointName;

            if (name.empty())
            {
                name = blockName;
            }


            return truncateToWinMMLimit ? name.substr(0, MAXPNAMELEN - 1) : name;
        }

        if (isNativeUmpDevice)
        {
            std::wstring name{};

            std::wstring suffix{};

            if (!blockName.empty())
            {
                suffix = blockName;
            }
            else
            {
                // a winmm port can only represent a single group
                suffix = L":" + std::to_wstring(groupIndex + 1);
            }

            
            if (!customParentEndpointName.empty())
            {
                name = customParentEndpointName;
            }
            else if (!transportSuppliedEndpointName.empty())
            {
                name = transportSuppliedEndpointName;
            }
            else if (!parentDeviceName.empty())
            {
                name = parentDeviceName;
            }
            else if (!deviceManufacturerName.empty())
            {
                name = deviceManufacturerName;
            }

            if (truncateToWinMMLimit)
            {
                if (name.length() + suffix.length() >= MAXPNAMELEN)
                {
                    // -1 for null, -1 again for the space between words
                    name = name.substr(0, MAXPNAMELEN - 1 - suffix.length() - 1);
                }
            }

            return name + L" " + suffix;
        }


        // this is the fallback. This needs better calculation to better support block name

        std::wstring name;

        if (!transportSuppliedEndpointName.empty())
        {
            if (auto pos = blockName.find(transportSuppliedEndpointName); pos != std::wstring::npos)
            {
                name = internal::TrimmedWStringCopy(transportSuppliedEndpointName + L" " + internal::TrimmedWStringCopy(blockName.substr(pos + transportSuppliedEndpointName.length())));
            }
            else
            {
                name = internal::TrimmedWStringCopy(transportSuppliedEndpointName + L" " + blockName);
            }
        }
        else if (!blockName.empty())
        {
            name = blockName;
        }
        else if (!parentDeviceName.empty())
        {
            name = parentDeviceName;
        }

        return truncateToWinMMLimit ? name.substr(0, MAXPNAMELEN - 1) : name;
    }


    // This is used for generating the GTB names on KSA endpoints, for MIDI 1.0 devices. Those
    // GTB names are used directly when creating WinMM endpoints
    inline std::wstring GenerateGtbNameFromMidi1Device(
        _In_ bool const useOldStyleNaming,                      // this comes from the property on the device, and if not specified, from the registry. Controls using old WinMM-style naming
        _In_ std::wstring const& customPortName,                // if the user has supplied a name for the generated port, and we're not using old-style naming, this wins
        _In_ std::wstring const& deviceContainerName,           // oddly some old WinMM code picks up the deviceContainerName somehow
        _In_ std::wstring const& ksDriverSuppliedDeviceName,    // the name the driver stored in the registry for the device
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& deviceManufacturerName,        // manufacturer name, if we have it
        _In_ std::wstring const& filterName,                    // the name of the filter. This is sometimes the same as the parent device
        _In_ std::wstring const& pinName,                       // the name of the KS Filter pin. This can be the same as the USB iJack
        _In_ uint8_t const groupIndex,                          
        _In_ MidiFlow const flowFromUserPerspective,
        _In_ bool const isUsingVendorDriver,
        _In_ bool const truncateToWinMMLimit,
        _In_ std::vector<std::wstring> const& otherExistingMidi1PortNamesForThisDeviceAndFlow
    )
    {
        UNREFERENCED_PARAMETER(deviceContainerName);
        UNREFERENCED_PARAMETER(deviceManufacturerName);
        UNREFERENCED_PARAMETER(groupIndex);
        UNREFERENCED_PARAMETER(flowFromUserPerspective);
        UNREFERENCED_PARAMETER(isUsingVendorDriver);
        UNREFERENCED_PARAMETER(otherExistingMidi1PortNamesForThisDeviceAndFlow);

        // user supplied a port name, so it is what we prefer
        // if we're using old-style naming, we do not use
        // any user-supplied information for the name
        if (!customPortName.empty() && !useOldStyleNaming)
        {
            return truncateToWinMMLimit ? customPortName.substr(0, MAXPNAMELEN - 1) : customPortName;
        }


        if (useOldStyleNaming)
        {
            std::wstring name{};

            if (!ksDriverSuppliedDeviceName.empty())
            {
                name = truncateToWinMMLimit ? ksDriverSuppliedDeviceName.substr(0, MAXPNAMELEN - 1) : ksDriverSuppliedDeviceName;
            }
            else if (!filterName.empty())
            {
                name = truncateToWinMMLimit ? filterName.substr(0, MAXPNAMELEN - 1) : filterName;
            }

            //auto cleanPinName = CleanupKSPinName(pinName, parentDeviceName, filterName);

            return truncateToWinMMLimit ? name.substr(0, MAXPNAMELEN - 1) : name;
        }
        else
        {
            std::wstring name{};

            auto cleanedPinName = CleanupKSPinName(pinName, parentDeviceName, filterName);

            name = internal::TrimmedWStringCopy(filterName + L" " + internal::TrimmedWStringCopy(cleanedPinName));

            if (truncateToWinMMLimit)
            {
                // if the name is too long, try using just the pin name or just the filter name

                if (name.length() + 1 > MAXPNAMELEN)
                {
                    if (!cleanedPinName.empty())
                    {
                        // we're over length, so just use the pin name
                        name = cleanedPinName.substr(0, MAXPNAMELEN - 1);
                    }
                    else
                    {
                        // we're over length, and there's no pin name
                        // so we use the filter name
                        name = filterName.substr(0, MAXPNAMELEN - 1);
                    }
                }
            }

            // TODO: do we need to do any port differentiators here? Look at the collection and see
            // if there are already ports starting with the same name, and if so, increment a counter and append

            return name;
        }


        return L"No name available";
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
        _In_ std::vector<Midi1PortNameEntry>& entries,
        _Inout_ std::vector<std::byte>& propertyData
    )
    {
        if (entries.size() == 0) return false;

        // calculate the total size
        size_t entriesSize = entries.size() * sizeof(Midi1PortNameEntry);
        size_t totalSize = static_cast<size_t>(entriesSize + MIDI1_PORT_NAME_ENTRY_HEADER_SIZE);
        size_t offset{ 0 };

        propertyData.resize(totalSize, (std::byte)0);

        // header value (byte count)
        memcpy((propertyData.data()), &totalSize, MIDI1_PORT_NAME_ENTRY_HEADER_SIZE);
        offset += MIDI1_PORT_NAME_ENTRY_HEADER_SIZE;

        // copy in all the name data. Vectors are guaranteed to be contiguous.
        memcpy((propertyData.data() + offset), entries.data(), entriesSize);

        return true;
    }


}

#endif