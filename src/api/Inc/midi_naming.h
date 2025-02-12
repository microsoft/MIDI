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
            pinName == L"In" || pinName == L"IN" || pinName == L"in"
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


    // This is used for generating the GTB names on KSA endpoints, for MIDI 1.0 devices. Those
    // GTB names are used directly when creating WinMM endpoints
    inline std::wstring GenerateMidi1PortName(
        _In_ bool const useOldStyleNaming,                      // this comes from the property on the device, and if not specified, from the registry. Controls using old WinMM-style naming
        _In_ std::wstring const& userSuppliedPortName,          // if the user has supplied a name for the generated port, and we're not using old-style naming, this wins
        _In_ std::wstring const& deviceContainerName,           // oddly some old WinMM code picks up the deviceContainerName somehow
        _In_ std::wstring const& parentDeviceName,              // the name of the actual connected device from which the UMP interface is generated
        _In_ std::wstring const& parentDeviceManufacturerName,  // the name of the parent device
        _In_ std::wstring const& filterName,                    // the name of the filter. This is sometimes the same as the parent device
        _In_ std::wstring const& pinName,                       // the name of the KS Filter pin. This can be the same as the USB iJack
        _In_ uint8_t const groupIndex,                          
        _In_ MidiFlow const flowFromUserPerspective,
        _In_ bool const isUmpDevice,
        _In_ bool const isUsingVendorDriver,
        _In_ bool const truncateToWinMMLimit,
        _In_ std::vector<std::wstring> const& otherExistingMidi1PortNamesForThisDeviceAndFlow
    )
    {
        UNREFERENCED_PARAMETER(deviceContainerName);
        UNREFERENCED_PARAMETER(parentDeviceName);
        UNREFERENCED_PARAMETER(parentDeviceManufacturerName);
        UNREFERENCED_PARAMETER(groupIndex);
        UNREFERENCED_PARAMETER(flowFromUserPerspective);
        UNREFERENCED_PARAMETER(isUmpDevice);
        UNREFERENCED_PARAMETER(isUsingVendorDriver);
        UNREFERENCED_PARAMETER(otherExistingMidi1PortNamesForThisDeviceAndFlow);

        // user supplied a port name, so it is what we use
        // if we're using old-style naming, we do not use
        // any user-supplied information for the name
        if (!userSuppliedPortName.empty() && !useOldStyleNaming)
        {
            return userSuppliedPortName;
        }


        if (useOldStyleNaming)
        {
            std::wstring name{};

            // TODO: Find the old naming code in the source tree, and reimplement



            return name;
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

    }

}

#endif