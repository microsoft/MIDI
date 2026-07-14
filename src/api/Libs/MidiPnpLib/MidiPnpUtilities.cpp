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

#define BUS_ENUMERATOR_PCI      L"PCI"
#define BUS_ENUMERATOR_USB      L"USB"
#define BUS_ENUMERATOR_SWD      L"SWD"

// these are hard-coded due to how these drivers enumerate
#define BUS_ENUMERATOR_MOTU     L"MOTUUSB64"
#define BUS_ENUMERATOR_BOME     L"BOMEBUS"

#define INSTANCE_ID_USB_VID_PREFIX  L"VID_"
#define INSTANCE_ID_USB_PID_PREFIX  L"PID_"

#undef max

namespace internal = WindowsMidiServicesInternal;

namespace WindowsMidiServicesInternal
{
    _Use_decl_annotations_
    bool IsMediaDriverDeviceParent(std::wstring const& deviceInstanceId) noexcept
    {
        // this is a magic string. Usually it's &MI_00.
        return internal::ToUpperTrimmedWStringCopy(deviceInstanceId).find(L"&MI_") != std::wstring::npos;
    }

    _Use_decl_annotations_
    bool IsPnpRootNode(std::wstring const& deviceInstanceId) noexcept
    {
        // this has to handle all types of devices, including USB, PNP, BLE, etc.
        auto cleanId = internal::ToUpperTrimmedWStringCopy(deviceInstanceId);

        return 
            cleanId.starts_with(L"HTREE\\ROOT\\") ||
       /*   cleanId.starts_with(L"ROOT\\") || */
            cleanId.starts_with(L"ACPI_HAL\\") ||
            cleanId.starts_with(L"ACPI\\");
    }


    _Use_decl_annotations_
    bool IsStandardUsbDeviceInstanceId(std::wstring const& deviceInstanceId) noexcept
    {
        auto cleanId = internal::ToUpperTrimmedWStringCopy(deviceInstanceId);

        return cleanId.starts_with(L"USB\\") && !cleanId.starts_with(L"USB\\ROOT_HUB");
    }


    _Use_decl_annotations_
    bool DeviceInstanceIdsHaveSameUsbVidPid(
        std::wstring const& deviceInstanceId1,
        std::wstring const& deviceInstanceId2) noexcept
    {
        uint16_t vid{ 0 };
        uint16_t pid{ 0 };
        std::wstring serial {};

        auto hr = ParseUsbDeviceInstanceIntoVidPidSerial(deviceInstanceId1, vid, pid, serial);

        if (FAILED(hr))
        {
            return false;
        }

        return DeviceInstanceIdHasUsbVidPid(deviceInstanceId2, vid, pid);
    }

    _Use_decl_annotations_
    bool DeviceInstanceIdHasUsbVidPid(
        std::wstring const& deviceInstanceId,
        uint16_t const expectedVid,
        uint16_t const expectedPid) noexcept
    {
        uint16_t vid { 0 };
        uint16_t pid { 0 };
        std::wstring serial{};

        auto hr = ParseUsbDeviceInstanceIntoVidPidSerial(deviceInstanceId, vid, pid, serial);

        if (FAILED(hr))
        {
            return false;
        }

        return vid == expectedVid && pid == expectedPid;
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
    HRESULT ParseUsbDeviceInstanceIntoVidPidSerial(
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
        if (vidPidString1.starts_with(INSTANCE_ID_USB_VID_PREFIX))
        {
            vidString = vidPidString1.substr(4);
            pidString = vidPidString2.substr(4);
        }
        else if (vidPidString2.starts_with(INSTANCE_ID_USB_VID_PREFIX))
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
    std::wstring ParseBusEnumeratorFromInstanceId(std::wstring const& deviceInstanceId) noexcept
    {
        auto pos = deviceInstanceId.find(L'\\');

        // only if found, and not in the first position in the string.
        // we're looking for things like USB\ and PCI\ or SWD\ but only return
        // USB, PCI, SWD without the backslash
        
        if (pos == std::wstring::npos || pos == 0)
        {
            return L"";
        }
        
        // return everything up until the backslash
        return internal::ToUpperTrimmedWStringCopy(deviceInstanceId.substr(0,pos));
    }



    _Use_decl_annotations_
    bool FindActualParentDeviceInstanceId(
        std::wstring const& startingParentInstanceId, 
        std::wstring& actualParentInstanceId,
        std::wstring& relatedMediaDriverDeviceParentInstanceId
        ) noexcept
    {
        std::wstring currentParentCandidate = internal::ToUpperTrimmedWStringCopy(startingParentInstanceId);
        std::wstring previousCandidate = internal::ToUpperTrimmedWStringCopy(startingParentInstanceId);

        bool haveStartingVidPid{ false };
        uint16_t startingVendorId{}; 
        uint16_t startingProductId{};
        std::wstring startingSerialNumber{}; // needed, but typically empty
        //std::wstring startingEnumerator{};

        while (!currentParentCandidate.empty() && !internal::IsPnpRootNode(currentParentCandidate))
        {
            // check to see if we're already at the &MI_00 or similar parent device
            if (internal::IsMediaDriverDeviceParent(currentParentCandidate))
            {
                relatedMediaDriverDeviceParentInstanceId = currentParentCandidate;
            }

            auto enumerator = internal::ParseBusEnumeratorFromInstanceId(currentParentCandidate);

            // the parent info for PCI is much more complex, so just stick with the parent provided
            // same with BOME virtual, and also MOTUBUS devices. I fully expect to find more examples
            // from vendor drivers which need to be added to this over time.
            if (enumerator == BUS_ENUMERATOR_PCI || enumerator == BUS_ENUMERATOR_MOTU || enumerator == BUS_ENUMERATOR_BOME)
            {
                actualParentInstanceId = currentParentCandidate;
                return true;
            }


            // see if we're on a regular USB node.
            if (!haveStartingVidPid && internal::IsStandardUsbDeviceInstanceId(currentParentCandidate))
            {
                haveStartingVidPid = (SUCCEEDED(internal::ParseUsbDeviceInstanceIntoVidPidSerial(
                    currentParentCandidate,
                    startingVendorId,
                    startingProductId,
                    startingSerialNumber)
                ));
            }

            auto pnpInfo = internal::MidiPnpDeviceInfo::CreateFromInstanceId(currentParentCandidate);

            if (pnpInfo)
            {   
                std::wstring nextParent = internal::ToUpperTrimmedWStringCopy(pnpInfo->ParentInstanceId().c_str());

                // check to see if next device up is a different actual device
                if (haveStartingVidPid)
                {
                    // validate that the node we're at has same vid/pid. If it doesn't, it's a hub or something.
                    // and we need to bail because we've gone too far.
                    if (!internal::DeviceInstanceIdHasUsbVidPid(nextParent, startingVendorId, startingProductId))
                    {
                        actualParentInstanceId = currentParentCandidate;
                        return true;
                    }
                }

                // check to see if we're one level down from the top of the tree
                if (internal::IsPnpRootNode(nextParent))
                {
                    // next parent is root, so we're done. Current candidate is as high up as we go.
                    actualParentInstanceId = currentParentCandidate;
                    return true;
                }

                // move up to the next parent
                previousCandidate = currentParentCandidate;
                currentParentCandidate = nextParent;
            }
            else
            {
                // pnp info is null. we just return where we stopped
                actualParentInstanceId = previousCandidate;

                return true;
            }
        }

        return false;

    }




}



