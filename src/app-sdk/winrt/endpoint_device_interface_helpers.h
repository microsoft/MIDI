// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace WindowsMidiServicesInternal
{

    inline bool IsValidWindowsMidiServicesEndpointId(_In_ winrt::hstring endpointDeviceId)
    {
        auto normalizedDeviceId = winrt::to_hstring(internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId.c_str()).c_str());

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

        return false;
    }

    inline bool IsWindowsMidiServicesEndpointPresent(_In_ winrt::hstring endpointDeviceId)
    {
        auto normalizedDeviceId = winrt::to_hstring(internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId.c_str()).c_str());

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

        return false;
    }
}