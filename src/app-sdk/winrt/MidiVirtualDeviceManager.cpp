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


// TODO: Should just reference the source file from the service plugin
#define MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX L"MIDIU_APPDEV_"
#define MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX L"MIDIU_APPPUB_"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    bool MidiVirtualDeviceManager::IsTransportAvailable() noexcept
    {
        // TODO: Check to see if service transport is installed and running. May require a new service call
        return true;
    }



    _Use_decl_annotations_
    winrt::hstring MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(winrt::guid associationId) noexcept
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator);
        additionalProperties.Append(STRING_PKEY_MIDI_TransportLayer);

        // we can't include custom properties in the selector, so we need to get all
        // the midi interfaces and run through them, checking the associator
        auto devices = enumeration::DeviceInformation::FindAllAsync(
            MidiEndpointConnection::GetDeviceSelector(),
            additionalProperties,
            enumeration::DeviceInformationKind::DeviceInterface
        ).get();

        winrt::hstring stringifiedAssociationId { internal::GuidToString(associationId) };

        if (devices != nullptr && devices.Size() > 0)
        {
            for (auto const& di : devices)
            {
                auto transportLayer = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::guid>(STRING_PKEY_MIDI_TransportLayer, di, foundation::GuidHelper::Empty());

                if (transportLayer == TransportId())
                {
                    // it's a virtual device, let's check the associator id and the type
                    auto associator = internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, di, L"");

                    if (!associator.empty() && associator == stringifiedAssociationId)
                    {
                        std::wstring searchId = internal::ToLowerTrimmedHStringCopy(di.Id()).c_str();
                        std::wstring prefix = internal::ToLowerTrimmedWStringCopy(MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX);

                        if (searchId.find(prefix) != searchId.npos)
                        {
                            return internal::NormalizeEndpointInterfaceIdHStringCopy(di.Id());
                        }
                    }
                }
            }
        }

        return L""; // not found
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
