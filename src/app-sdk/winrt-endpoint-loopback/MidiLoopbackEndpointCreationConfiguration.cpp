// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiLoopbackEndpointCreationConfiguration.h"
#include "MidiLoopbackEndpointCreationConfiguration.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::Endpoints::Loopback::implementation
{
    _Use_decl_annotations_
    MidiLoopbackEndpointCreationConfiguration::MidiLoopbackEndpointCreationConfiguration(
        winrt::guid associationId,
        loop::MidiLoopbackEndpointDefinition endpointDefinitionA,
        loop::MidiLoopbackEndpointDefinition endpointDefinitionB
    )
    {
        m_associationId = associationId;

        m_definitionA = endpointDefinitionA;
        m_definitionB = endpointDefinitionB;
    }



    // "endpointTransportPluginSettings":
    // {
    //   endpoint abstraction guid :
    //   {
    //     "create"
    //     {
    //       "{associationGuid}":
    //       {
    //         "endpointA":
    //         {
    //           ... endpoint properties ...
    //         },
    //         "endpointB":
    //         {
    //           ... endpoint properties ...
    //         }
    //       }
    //     }
    //   }
    // }

    winrt::hstring MidiLoopbackEndpointCreationConfiguration::GetConfigurationJson()
    {
        json::JsonObject endpointAssociationObject;
        json::JsonObject endpointDeviceAObject;
        json::JsonObject endpointDeviceBObject;
        json::JsonObject endpointCreationObject;
        json::JsonObject abstractionObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject outerWrapperObject;

        // build Endpoint A

        endpointDeviceAObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definitionA.Name).c_str()));

        endpointDeviceAObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definitionA.Description).c_str()));

        endpointDeviceAObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definitionA.UniqueId).c_str()));

        // build Endpoint B

        endpointDeviceBObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definitionB.Name).c_str()));

        endpointDeviceBObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definitionB.Description).c_str()));

        endpointDeviceBObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(internal::TrimmedHStringCopy(m_definitionB.UniqueId).c_str()));

        // create the association object with the two devices as children

        endpointAssociationObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY,
            endpointDeviceAObject);

        endpointAssociationObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY,
            endpointDeviceBObject);

        // create the creation node with the association object as the child property

        endpointCreationObject.SetNamedValue(
            internal::GuidToString(m_associationId),
            endpointAssociationObject);

        // create the abstraction object with the child creation node

        abstractionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY,
            endpointCreationObject);

        // create the main node

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(loop::MidiLoopbackEndpointManager::AbstractionId()),
            endpointCreationObject);

        // wrap it all up so the json is valid

        outerWrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);


        return outerWrapperObject.Stringify();
    }

}
