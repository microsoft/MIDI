// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

struct MidiBluetoothDeviceDefinition
{
    uint64_t BluetoothAddress{};

    winrt::hstring TransportSuppliedName{};
    winrt::hstring TransportSuppliedDescription{};

    winrt::hstring ManufacturerName{};

//    winrt::hstring UserSuppliedName{};
//    winrt::hstring UserSuppliedDescription{};

    std::wstring CreatedEndpointInterfaceId{};
    std::wstring CreatedMidiDeviceInstanceId{};


    void SetDeactivated()
    {
        CreatedEndpointInterfaceId = L"";
        CreatedMidiDeviceInstanceId = L"";
    }

};