// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiBasicLoopbackEndpointCreationConfig.h"
#include "Endpoints.BasicLoopback.MidiBasicLoopbackEndpointCreationConfig.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    _Use_decl_annotations_
    MidiBasicLoopbackEndpointCreationConfig::MidiBasicLoopbackEndpointCreationConfig(
        winrt::guid const& associationId,
        bloop::MidiBasicLoopbackEndpointDefinition const& endpointDefinition)
    {

        m_associationId = associationId;
        m_definition = endpointDefinition;
    }


    // "endpointTransportPluginSettings":
    // {
    //   basic loopback endpoint transport guid :
    //   {
    //     "create"
    //     {
    //       "{associationGuid}":
    //       {
    //         "endpoint":
    //         {
    //           ... endpoint properties ...
    //         }
    //       }
    //     }
    //   }
    // }    
     
    json::JsonObject MidiBasicLoopbackEndpointCreationConfig::GetConfigJson()
    {
        json::JsonObject endpointAssociationObject;
        json::JsonObject endpointDeviceObject;

        json::JsonObject endpointCreationObject;
        json::JsonObject transportObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject outerWrapperObject;

        // build Endpoint

        endpointDeviceObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definition.Name).c_str()));

        endpointDeviceObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definition.Description).c_str()));

        endpointDeviceObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definition.UniqueId).c_str()));

        // create the association object (this is here just to keep the structure apx the same as the main loopback types, for simplicity

        endpointAssociationObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_ENDPOINT_KEY,
            endpointDeviceObject);

        // create the creation node with the association object as the child property

        endpointCreationObject.SetNamedValue(
            internal::GuidToString(m_associationId),
            endpointAssociationObject);

        // create the transport object with the child creation node

        transportObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY,
            endpointCreationObject);

        // create the main node

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(bloop::MidiBasicLoopbackEndpointManager::TransportId()),
            transportObject);

        // wrap it all up so the json is valid

        outerWrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);


        return outerWrapperObject;

    }
}
