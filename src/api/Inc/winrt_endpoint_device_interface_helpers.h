// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

//#include "SetupAPI.h"
//#include "MidiDefs.h"   // for DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI. Unfortunately, pulls in a lot of other baggage

namespace WindowsMidiServicesInternal
{
    inline bool IsValidWindowsMidiServicesEndpointId(_In_ winrt::hstring endpointDeviceId)
    {
        try
        {
            auto normalizedDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);

            //auto category = DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI;
            //SetupDiGetClassDevs(interfaceCategory, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT)

            auto additionalTestProperties = winrt::single_threaded_vector<winrt::hstring>();
            additionalTestProperties.Append(STRING_PKEY_MIDI_TransportLayer);

            auto testDevice = enumeration::DeviceInformation::CreateFromIdAsync(
                normalizedDeviceId,
                additionalTestProperties,
                enumeration::DeviceInformationKind::DeviceInterface).get();

            if (testDevice != nullptr)
            {
                auto testTX = testDevice.Properties().Lookup(STRING_PKEY_MIDI_TransportLayer);

                if (testTX != nullptr)
                {
                    auto layer = testTX.try_as<winrt::guid>();

                    if (layer != std::nullopt)
                    {
                        return true;
                    }
                }
            }
        }
        catch (winrt::hresult_error const&)
        {
        }

        return false;
    }

    inline bool IsWindowsMidiServicesEndpointPresent(_In_ winrt::hstring endpointDeviceId)
    {
        try
        {
            auto normalizedDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);

            auto additionalTestProperties = winrt::single_threaded_vector<winrt::hstring>();
            additionalTestProperties.Append(L"System.Devices.InterfaceEnabled");


            auto testDevice = enumeration::DeviceInformation::CreateFromIdAsync(
                normalizedDeviceId,
                additionalTestProperties,
                enumeration::DeviceInformationKind::DeviceInterface).get();

            if (testDevice != nullptr)
            {
                auto enabledProp = testDevice.Properties().Lookup(L"System.Devices.InterfaceEnabled");

                if (enabledProp != nullptr)
                {
                    auto enabled = enabledProp.try_as<bool>();

                    if (enabled != std::nullopt && enabled.has_value() && enabled.value())
                    {
                        return true;
                    }
                }
            }
        }
        catch (winrt::hresult_error const&)
        {
        }

        return false;
    }
}