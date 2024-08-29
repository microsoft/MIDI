// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#include "pch.h"
#include "MidiVirtualDeviceManager.h"
#include "Endpoints.Virtual.MidiVirtualDeviceManager.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    bool MidiVirtualDeviceManager::IsTransportAvailable() noexcept
    {
        // TODO: Check to see if service abstraction is installed and running. May require a new service call
        return false;

    }

    _Use_decl_annotations_
    virt::MidiVirtualDevice MidiVirtualDeviceManager::CreateVirtualDevice(
        _In_ virt::MidiVirtualDeviceCreationConfig creationConfig
    ) noexcept
    {
        winrt::hstring deviceEndpointDeviceId{};
        //winrt::hstring clientEndpointDeviceId{};

        auto createResponse = svc::MidiServiceConfig::UpdateTransportPluginConfig(creationConfig);

        if (createResponse.Status == svc::MidiServiceConfigResponseStatus::Success)
        {
            json::JsonObject responseObject{};

            if (json::JsonObject::TryParse(createResponse.ResponseJson, responseObject))
            {
                auto responseArray = responseObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY, nullptr);

                if (responseArray == nullptr || responseArray.Size() == 0)
                {
                //    internal::LogGeneralError(__FUNCTION__, L"Unexpected empty response array");

                    return nullptr;
                }
                else
                {
                    // get the id. We should have only one item in the response array here so we'll just grab the first

                    auto firstObject = responseArray.GetObjectAt(0);

                    deviceEndpointDeviceId = firstObject.GetNamedString(
                        MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY,
                        L"");


                    // Create the MidiVirtualDevice
                    auto device = winrt::make_self<implementation::MidiVirtualDevice>();

                    // populate the virtual device with everything needed

                    // TODO: We need to get the client id
                    device->InternalInitialize(deviceEndpointDeviceId, creationConfig);

                    return *device;
                }
            }
            else
            {
                // couldn't parse the json
            }
        }
        else
        {
            // failure return from service. createResponse.Status(). Should share this
        }

        return nullptr;
    }

}
