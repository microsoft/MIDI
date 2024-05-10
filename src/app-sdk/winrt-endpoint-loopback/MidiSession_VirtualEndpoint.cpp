// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiEndpointConnection.h"

namespace MIDI_CPP_NAMESPACE::implementation
{


    // TODO: Wrap up all this JSON crud in another function 

    _Use_decl_annotations_
    midi2::MidiEndpointConnection MidiSession::TryCreateVirtualDeviceAndConnection(
        midi2::MidiVirtualEndpointDeviceDefinition const& deviceDefinition
    ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        winrt::hstring endpointDeviceId{};


        // create the json for creating the endpoint


        // todo: grab this from a constant
        winrt::hstring virtualDeviceAbstractionId = L"{8FEAAD91-70E1-4A19-997A-377720A719C1}";


        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject wrapperObject;
        json::JsonObject abstractionObject;
        json::JsonObject endpointCreationObject;
        json::JsonArray createVirtualDevicesArray;
        json::JsonObject endpointDefinitionObject;

        internal::LogInfo(__FUNCTION__, L"Creating association GUID");

        // Create association guid
        auto associationGuid = foundation::GuidHelper::CreateNewGuid();

        // set all of our properties.

        internal::LogInfo(__FUNCTION__, L"Setting json properties");

        internal::JsonSetGuidProperty(
            endpointDefinitionObject,
            MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY,
            associationGuid);


        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(deviceDefinition.EndpointProductInstanceId().c_str()));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(deviceDefinition.EndpointName().c_str()));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(deviceDefinition.TransportSuppliedDescription().c_str()));


        // TODO: Other props that have to be set at the service level and not in-protocol

        internal::LogInfo(__FUNCTION__, L"Appending endpoint definition");
        internal::LogInfo(__FUNCTION__, endpointDefinitionObject.Stringify().c_str());

        createVirtualDevicesArray.Append(endpointDefinitionObject);

        internal::LogInfo(__FUNCTION__, L"Adding virtual devices array to abstraction object");

        abstractionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY,
            createVirtualDevicesArray);

        internal::LogInfo(__FUNCTION__, L"Setting named value for virtual abstraction id");

        endpointCreationObject.SetNamedValue(virtualDeviceAbstractionId, abstractionObject);


        // wrap it all in the Transport GUID so it's the same format used in the configuration file
        wrapperObject.SetNamedValue(virtualDeviceAbstractionId, abstractionObject);

        topLevelTransportPluginSettingsObject.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, wrapperObject);


        // this is the format we're sending up
        // 
        //"endpointTransportPluginSettings":
        //{
        //    "{8FEAAD91-70E1-4A19-997A-377720A719C1}":
        //    {
        //       "create":
        //       [
        //           {
        //              ... properties ...
        //           }
        //       ]
        //    }
        //}
        //

        // send it up

        auto iid = __uuidof(IMidiAbstractionConfigurationManager);

        winrt::com_ptr<IMidiAbstractionConfigurationManager> configManager;

        auto activateConfigManagerResult = m_serviceAbstraction->Activate(iid, (void**)&configManager);

        internal::LogInfo(__FUNCTION__, L"Config manager activate call completed");


        if (FAILED(activateConfigManagerResult) || configManager == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to create device. Config manager is null.");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"Config manager activate call SUCCESS");

        auto initializeResult = configManager->Initialize(internal::StringToGuid(virtualDeviceAbstractionId.c_str()), nullptr, nullptr);


        if (FAILED(initializeResult))
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to initialize config manager");

            return nullptr;
        }


        internal::LogInfo(__FUNCTION__, L"Config manager initialize call SUCCESS");

        CComBSTR response;
        response.Empty();

        internal::LogInfo(__FUNCTION__, L"Sending json to UpdateConfiguration");
        internal::LogInfo(__FUNCTION__, topLevelTransportPluginSettingsObject.Stringify().c_str());

        auto configUpdateResult = configManager->UpdateConfiguration(topLevelTransportPluginSettingsObject.Stringify().c_str(), false, &response);

        internal::LogInfo(__FUNCTION__, L"UpdateConfiguration returned");
        internal::LogInfo(__FUNCTION__, response);

        if (FAILED(configUpdateResult))
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to configure endpoint");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"configManager->UpdateConfiguration success");

        json::JsonObject responseObject;

        if (!internal::JsonObjectFromBSTR(&response, responseObject))
        {
            internal::LogGeneralError(__FUNCTION__, L"Failed to read json response object from virtual device creation");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"JsonObjectFromBSTR success");

        // check for actual success
        auto successResult = responseObject.GetNamedBoolean(MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY, false);

        if (successResult)
        {
            internal::LogInfo(__FUNCTION__, L"JSON payload indicates success");

            auto responseArray = responseObject.GetNamedArray(MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY, nullptr);

            if (responseArray != nullptr && responseArray.Size() == 0)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unexpected empty response array");

                return nullptr;
            }

            // get the id. We should have only one item in the response array here so we'll just grab the first

            auto firstObject = responseArray.GetObjectAt(0);

            endpointDeviceId = firstObject.GetNamedString(
                MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY,
                L"");

            internal::LogInfo(__FUNCTION__, endpointDeviceId.c_str());
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Virtual device creation failed (payload has false success value)");

            return nullptr;
        }

        internal::LogInfo(__FUNCTION__, L"Virtual device creation worked.");

        // Creating the device succeeded, so create the connection
        auto connection = TryCreateEndpointConnection(endpointDeviceId, false, nullptr);

        internal::LogInfo(__FUNCTION__, L"Created endpoint connection");

        if (connection)
        {
            auto virtualDevice = winrt::make_self<implementation::MidiVirtualEndpointDevice>();

            virtualDevice->InternalSetDeviceDefinition(deviceDefinition);
            virtualDevice->IsEnabled(true);

            // add the listener
            connection.AddMessageProcessingPlugin(*virtualDevice);
        }
        else
        {
            internal::LogGeneralError(__FUNCTION__, L"Could not add MidiVirtualEndpointDevice because connection was null");
        }

        return connection;
    }


}