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
    inline std::wstring CleanupKSPinName(
        _In_ std::wstring const& pinName, 
        _In_ std::wstring parentDeviceName, 
        _In_ std::wstring filterName
    )
    {
        std::wstring cleanedPinName{};

        // Used by ESI, MOTU, and others. We don't want to mess up other names, so check only
        // for whole word. We do other removal in the next step
        if (pinName == L"MIDI" || 
            pinName == L"Out" || pinName == L"OUT" || pinName == L"out" ||
            pinName == L"In" || pinName == L"IN" || pinName == L"in" ||
            pinName == L"IO"
            )
        {
            cleanedPinName = L"";
        }
        else
        {
            cleanedPinName = pinName;
        }

        // the double and triple space entries need to be last
        // there are other ways to do this with pattern matching, 
        // but just banging this through for this version
        const std::wstring wordsToRemove[] =
        {
            // many of these are added by our USB and KS stack, not by the device, which is why they are here
            parentDeviceName, filterName,
            L"[0]", L"[1]", L"[2]", L"[3]", L"[4]", L"[5]", L"[6]", L"[7]", L"[8]", L"[9]", L"[10]", L"[11]", L"[12]", L"[13]", L"[14]", L"[15]", L"[16]",
            L"  ", L"   ", L"    "
        };

        for (auto const& word : wordsToRemove)
        {
            if (cleanedPinName.length() >= word.length())
            {
                auto idx = cleanedPinName.find(word);

                if (idx != std::wstring::npos)
                {
                    cleanedPinName = cleanedPinName.erase(idx, word.length());
                }
            }
        }

        internal::InPlaceTrim(cleanedPinName);

        return cleanedPinName;
    }


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
        /*_In_ std::vector<std::wstring> const& otherExistingMidi1PortNamesForThisDeviceAndFlow*/
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


        if (useOldStyleNamingForNonUmpDevice)
        {
            // TODO: Find the old naming code in the source tree, and reimplement it here

            std::wstring name;

            // TEMPORARY code
            name = transportSuppliedEndpointName;


            return truncateToWinMMLimit ? name.substr(0, MAXPNAMELEN - 1) : name;
        }
        else
        {

            std::wstring name;

            // TEMPORARY code

            name = parentDeviceName;


            return truncateToWinMMLimit ? name.substr(0, MAXPNAMELEN - 1) : name;
        }


        return L"No name available";
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

}

#endif