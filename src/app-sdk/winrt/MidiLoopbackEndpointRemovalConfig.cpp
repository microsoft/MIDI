// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiLoopbackEndpointRemovalConfig.h"
#include "Endpoints.Loopback.MidiLoopbackEndpointRemovalConfig.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    _Use_decl_annotations_
    MidiLoopbackEndpointRemovalConfig::MidiLoopbackEndpointRemovalConfig(winrt::guid const& associationId)
    {
        m_associationId = associationId;
    }

    // "endpointTransportPluginSettings":
    // {
    //   loopback endpoint transport guid :
    //   {
    //     "remove"
    //     {
    //        "{associationGuid}"
    //     }
    //   }
    // }
    json::JsonObject MidiLoopbackEndpointRemovalConfig::GetConfigJson()
    {
        json::JsonArray endpointDeletionArray;
        json::JsonObject transportObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject outerWrapperObject;

        json::JsonValue endpointDeletionAssociationId = json::JsonValue::CreateStringValue(internal::GuidToString(m_associationId));
        endpointDeletionArray.Append(endpointDeletionAssociationId);

        // create the transport object with the child creation node

        transportObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY,
            endpointDeletionArray);

        // create the main node

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(loop::MidiLoopbackEndpointManager::TransportId()),
            transportObject);

        // wrap it all up so the json is valid

        outerWrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);

        return outerWrapperObject;
    }
}
