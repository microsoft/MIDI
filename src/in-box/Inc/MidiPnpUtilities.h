// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <wil/result_macros.h>
#include <string>

namespace WindowsMidiServicesInternal
{

    bool IsStandardUsbDeviceInstanceId(_In_ std::wstring const& deviceInstanceId) noexcept;

    HRESULT ParseUsbDeviceInstanceIntoVidPidSerial(
        _In_ std::wstring const& deviceInstanceId,
        _Inout_ uint16_t& vendorId,
        _Inout_ uint16_t& productId,
        _Inout_ std::wstring& serialNumber) noexcept;


    std::wstring ParseBusEnumeratorFromInstanceId(_In_ std::wstring const& deviceInstanceId) noexcept;

    bool DeviceInstanceIdsHaveSameUsbVidPid(
        _In_ std::wstring const& deviceInstanceId1, 
        _In_ std::wstring const& deviceInstanceId2) noexcept;

    bool DeviceInstanceIdHasUsbVidPid(
        _In_ std::wstring const& deviceInstanceId,
        _In_ uint16_t const expectedVid,
        _In_ uint16_t const expectedPid) noexcept;

    // returns the &MI_nn "device"
    bool IsMediaDriverDeviceParent(_In_ std::wstring const& deviceInstanceId) noexcept;

    bool IsPnpRootNode(_In_ std::wstring const& deviceInstanceId) noexcept;

    bool FindActualParentDeviceInstanceId(
        _In_ std::wstring const& startingParentInstanceId,                              // the parent device that System.Devices.Parent provides for the MIDI port/endpoint. 
        _Inout_ std::wstring& actualParentInstanceId,                                   // returned parent
        _Inout_ std::wstring& relatedMediaDriverDeviceParentInstanceId) noexcept;       // the &MI_00 device found when walking up the tree

}
