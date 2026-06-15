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

    HRESULT ParseDeviceInstanceIntoVidPidSerial(
        _In_ std::wstring const& deviceInstanceId,
        _Inout_ uint16_t& vendorId,
        _Inout_ uint16_t& productId,
        _Inout_ std::wstring& serialNumber) noexcept;


    bool IsMidiInterfacePseudoParent(_In_ std::wstring const& deviceInstanceId) noexcept;

    bool IsPnpRootNode(_In_ std::wstring const& deviceInstanceId) noexcept;

    bool FindActualParentDeviceInstanceId(
        _In_ std::wstring const& startingParentInstanceId,
        _Inout_ std::wstring& actualParentInstanceId,
        _Inout_ std::wstring& relatedMidiPseudoParentInstanceId) noexcept;

}
