// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiBasicLoopbackRemovalConfig.h"
#include "MidiBasicLoopbackRemovalConfig.g.cpp"


namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{

    _Use_decl_annotations_
    MidiBasicLoopbackRemovalConfig::MidiBasicLoopbackRemovalConfig(winrt::guid const& associationId)
    {
        m_associationId = associationId;
    }


    // "endpointTransportPluginSettings":
    // {
    //   basic loopback endpoint transport guid :
    //   {
    //     "remove"
    //     {
    //        "{associationGuid}"
    //     }
    //   }
    // }
    json::JsonObject MidiBasicLoopbackRemovalConfig::ConfigJson()
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
            internal::GuidToString(bloop::MidiBasicLoopbackManager::TransportId()),
            transportObject);

        // wrap it all up so the json is valid

        outerWrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);

        return outerWrapperObject;

    }
}
