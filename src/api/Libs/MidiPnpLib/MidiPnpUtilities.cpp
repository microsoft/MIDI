// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "MidiPnpUtilities.h"
#include "MidiPnpDeviceInfo.h"
#include <iostream>
#include <sstream>
#include "wstring_util.h"


#undef max

namespace internal = WindowsMidiServicesInternal;

namespace WindowsMidiServicesInternal
{
    _Use_decl_annotations_
    bool IsMidiInterfacePseudoParent(std::wstring const& deviceInstanceId) noexcept
    {
        return internal::ToUpperTrimmedWStringCopy(deviceInstanceId).find(L"&MI_") == std::wstring::npos;
    }

    _Use_decl_annotations_
    bool IsPnpRootNode(std::wstring const& deviceInstanceId) noexcept
    {
        return 
            internal::ToUpperTrimmedWStringCopy(deviceInstanceId).starts_with(L"HTREE\\ROOT\\") ||
       /*   internal::ToUpperTrimmedWStringCopy(deviceInstanceId).starts_with(L"ROOT\\") || */
            internal::ToUpperTrimmedWStringCopy(deviceInstanceId).starts_with(L"ACPI_HAL\\") || 
            internal::ToUpperTrimmedWStringCopy(deviceInstanceId).starts_with(L"ACPI\\");
    }


    _Use_decl_annotations_
    bool IsStandardUsbDeviceInstanceId(std::wstring const& deviceInstanceId) noexcept
    {
        auto id = internal::ToUpperTrimmedWStringCopy(deviceInstanceId);
        return id.starts_with(L"USB\\") && !id.starts_with(L"USB\\ROOT_HUB");
    }



    // Examples
    // ---------------------------------------------------------------------------
    // Parent values with serial:    USB\VID_16C0&PID_05E4\ContinuuMini_SN024066
    //                               USB\VID_2573&PID_008A\no_serial_number (yes, this is the iSerialNumber in USB :/
    //                                        0x03	iSerialNumber   "no serial number"
    //                               USB\VID_12E6&PID_002C\251959d4f21fc283
    //                               USB\VID_0499&PID_1064\YMHFF354C6439384B3243256846
    // 
    // Parent values without serial: USB\VID_F055&PID_0069\8&2858bbac&0&4
    //                               USB\VID_2321&PID_000A&MI_00\9&a9274be&0&0000
    //                               USB\VID_1F38&PID_000D&MI_00\9&7c2e940&0&0000
    //                               USB\VID_2662&PID_000D\8&24eb0394&0&4
    //                               USB\VID_05E3&PID_0610\7&2f028fc9&0&4
    //                               ROOT\MOTUBUS\0000
    //                               MotuUSB64\MotuMidi64\0300
    //                               USB\VID_7104&PID_1600&MI_00\8&20e633b&0&0000    
    _Use_decl_annotations_
    HRESULT ParseDeviceInstanceIntoVidPidSerial(
        _In_ std::wstring const& deviceInstanceId,
        _Inout_ uint16_t& vendorId,
        _Inout_ uint16_t& productId,
        _Inout_ std::wstring& serialNumber) noexcept
    {
        auto id = internal::ToUpperTrimmedWStringCopy(deviceInstanceId);

        if (id.empty())
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }

        if (!IsStandardUsbDeviceInstanceId(id))
        {
            // not USB or it's a vendor driver with its own non-USB bus driver/enumerator

            RETURN_IF_FAILED(E_INVALIDARG);
        }

        std::wstringstream ss(id);
        std::wstring usbSection{};

        // we've already validated that this starts with the USB string
        std::getline(ss, usbSection, static_cast<wchar_t>('\\'));


        // get the VID/PID section

        std::wstring vidPidSection{};

        std::getline(ss, vidPidSection, static_cast<wchar_t>('\\'));

        if (vidPidSection.empty())
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }

        std::wstring serialSection{};       
        std::getline(ss, serialSection, static_cast<wchar_t>('\\'));    // returns rest of line if no more \

        // vid/pid are separated by &
        std::wstring vidPidString1{};
        std::wstring vidPidString2{};

        std::wstringstream ssVidPid(vidPidSection);
        std::getline(ssVidPid, vidPidString1, static_cast<wchar_t>('&'));
        std::getline(ssVidPid, vidPidString2, static_cast<wchar_t>('&'));

        wchar_t* end{ nullptr };

        std::wstring vidString;
        std::wstring pidString;

        // find the VID and PID strings.
        if (vidPidString1.starts_with(L"VID_"))
        {
            vidString = vidPidString1.substr(4);
            pidString = vidPidString2.substr(4);
        }
        else if (vidPidString2.starts_with(L"VID_"))
        {
            vidString = vidPidString2.substr(4);
            pidString = vidPidString1.substr(4);
        }

        auto vid = std::wcstoul(vidString.c_str(), &end, 16);

        if (vid != ULONG_MAX && vid <= std::numeric_limits<uint16_t>::max())
        {
            vendorId = static_cast<uint16_t>(vid);
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }

        auto pid = std::wcstoul(pidString.c_str(), &end, 16);

        if (pid != ULONG_MAX && pid <= std::numeric_limits<uint16_t>::max())
        {
            productId = static_cast<uint16_t>(pid);
        }
        else
        {
            RETURN_IF_FAILED(E_INVALIDARG);
        }

        // serial numbers with a & in them are generated by our system
        // it's possible a vendor may have a serial number with this in it,
        // but in that case, we just ditch it.
        if (serialSection.find_first_of('&') == serialSection.npos)
        {
            // Windows replaces spaces in the serial number with the underscore.
            // yes, this will end up catching the few (if any) serials that do 
            // actually include an underscore. However, there are a bunch with spaces.
            std::replace(serialSection.begin(), serialSection.end(), '_', ' ');
            serialNumber = serialSection;
        }
        else
        {
            // no valid serial found
            serialNumber = L"";
        }

        return S_OK;

    }



    _Use_decl_annotations_
    bool FindActualParentDeviceInstanceId(
        std::wstring const& startingParentInstanceId, 
        std::wstring& actualParentInstanceId,
        std::wstring& relatedMidiPseudoParentInstanceId
        ) noexcept
    {
        std::wstring currentParentCandidate;
        std::wstring previousCandidate = startingParentInstanceId.c_str();

        bool searchComplete { false };


        // we need to get the container first
        auto pnpInfo = internal::MidiPnpDeviceInfo::CreateFromInstanceId(startingParentInstanceId);

        winrt::guid containerId{};

        if (pnpInfo)
        {
            containerId = pnpInfo->ContainerId().value();
        }

        currentParentCandidate = startingParentInstanceId;

        std::wstring enumerator = currentParentCandidate.substr(0, 3);


        // This fails for PCIe devices, returning the PCI to PCI bridge as the parent instead
        // of the actual device. In those cases, we short circuit a bunch of the logic
        // and just return the first parent we find
        if (enumerator == L"PCI")
        {
            actualParentInstanceId = currentParentCandidate;
            relatedMidiPseudoParentInstanceId = L"";

            return true;
        }


        while (!searchComplete && !internal::IsPnpRootNode(currentParentCandidate))
        {
            pnpInfo = internal::MidiPnpDeviceInfo::CreateFromInstanceId(currentParentCandidate);

            if (pnpInfo)
            {               
                // check to see if we've found the interface with all the driver info
                // we'll find this before the hardware parent is found
                if (internal::IsMidiInterfacePseudoParent(currentParentCandidate))
                {
                    relatedMidiPseudoParentInstanceId = currentParentCandidate;
                }


                if (internal::IsPnpRootNode(pnpInfo->ParentInstanceId().c_str()))
                {
                    // parent is root, so we're done. Current candidate is as high up as we go.
                    actualParentInstanceId = currentParentCandidate;
                    searchComplete = true;
                }

                // check to see if the device is in the same container or not. If same container
                // than it's still part of the same logical device. If not, then we've gone too far
                else if (pnpInfo->ContainerId() != containerId)
                {
                    actualParentInstanceId = previousCandidate;

                    searchComplete = true;
                }
                else
                {
                    // move up to the next parent
                    previousCandidate = currentParentCandidate;
                    currentParentCandidate = pnpInfo->ParentInstanceId().c_str();
                }
            }
            else
            {
                // pnp info is null. we just return where we stopped
                actualParentInstanceId = previousCandidate;

                searchComplete = true;
            }
        }

        return true;

    }




}



