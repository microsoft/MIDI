// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <string>

namespace KsaUsbStrings
{
    struct UsbDeviceStrings
    {
        std::wstring Manufacturer{ };
        std::wstring SerialNumber{ };
    };

    // The iManufacturer and iSerialNumber strings for the device a KS filter belongs to. Windows
    // surfaces neither reliably: there is no manufacturer property, and the serial can only be
    // guessed at from the device instance id, which is synthesized when the device supplies none.
    //
    // Both are read in one hub session. Either or both come back empty for a device which does not
    // declare them, for anything that is not a plain USB device, and for every kind of failure.
    // This never throws and never blocks longer than the timeout it applies to each request: a
    // nice-to-have string must not be able to hold up enumeration.
    UsbDeviceStrings GetUsbDeviceStrings(_In_ std::wstring const& usbDeviceInstanceId) noexcept;
}
