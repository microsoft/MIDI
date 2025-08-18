// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceEndpointCustomizationConfig.h"
#include "ServiceConfig.MidiServiceEndpointCustomizationConfig.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    MidiServiceEndpointCustomizationConfig::MidiServiceEndpointCustomizationConfig(
        winrt::guid const& transportId) noexcept
    {
        m_transportId = transportId;
    }


    _Use_decl_annotations_
    MidiServiceEndpointCustomizationConfig::MidiServiceEndpointCustomizationConfig(
        winrt::guid const& transportId, 
        winrt::hstring const& name, 
        winrt::hstring const& description) noexcept
        : MidiServiceEndpointCustomizationConfig(transportId)
    {
        m_name = internal::TrimmedHStringCopy(name);
        m_description = internal::TrimmedHStringCopy(description);
    }

    _Use_decl_annotations_
    MidiServiceEndpointCustomizationConfig::MidiServiceEndpointCustomizationConfig(
        winrt::guid const& transportId, 
        winrt::hstring const& name, 
        winrt::hstring const& description, 
        winrt::hstring const& largeImageFileName, 
        winrt::hstring const& smallImageFileName) noexcept
        : MidiServiceEndpointCustomizationConfig(transportId, name, description)
    {
        m_largeImageFileName = internal::TrimmedHStringCopy(largeImageFileName);
        m_smallImageFileName = internal::TrimmedHStringCopy(smallImageFileName);
    }

    _Use_decl_annotations_
    MidiServiceEndpointCustomizationConfig::MidiServiceEndpointCustomizationConfig(
        winrt::guid const& transportId,
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& largeImageFileName,
        winrt::hstring const& smallImageFileName,
        bool const requiresNoteOffTranslation,
        bool const supportsMidiPolyphonicExpression,
        uint16_t const recommendedControlChangeIntervalMilliseconds) noexcept
        : MidiServiceEndpointCustomizationConfig(transportId, name, description, largeImageFileName, smallImageFileName)
    {
        m_requiresNoteOffTranslation = requiresNoteOffTranslation;
        m_supportsMidiPolyphonicExpression = supportsMidiPolyphonicExpression;
        m_recommendedControlChangeIntervalMilliseconds = recommendedControlChangeIntervalMilliseconds;
    }



// "endpointTransportPluginSettings":
// {
//   endpoint transport guid :
//   {
//     "update"
//     [
//       {
//         "match" :
//         {
//           "SWD": "..."
//         },
//         "name" : "new name",
//         "description" : "kdjflskjdfsdkjf" ...
//       }
//     ]
//   }
// }
    winrt::hstring MidiServiceEndpointCustomizationConfig::GetConfigJson() const noexcept
    {
        json::JsonObject matchObject;
        json::JsonArray endpointUpdateArray;
        json::JsonObject endpointUpdateObject;
        json::JsonObject transportObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject outerWrapperObject;


        // update object
        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY, json::JsonValue::CreateStringValue(m_name));
        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY, json::JsonValue::CreateStringValue(m_description));
        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SMALL_IMAGE_PROPERTY, json::JsonValue::CreateStringValue(m_smallImageFileName));
        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_LARGE_IMAGE_PROPERTY, json::JsonValue::CreateStringValue(m_largeImageFileName));

        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_REQUIRES_NOTE_OFF_TRANSLATION_PROPERTY, json::JsonValue::CreateBooleanValue(m_requiresNoteOffTranslation));
        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SUPPORTS_MPE_PROPERTY, json::JsonValue::CreateBooleanValue(m_supportsMidiPolyphonicExpression));
        endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_RECOMMENDED_CC_INTERVAL_MS_PROPERTY, json::JsonValue::CreateNumberValue(m_recommendedControlChangeIntervalMilliseconds));

        // match object within the update object. What is supported will vary by transport
        if (m_matchCriteria != nullptr && json::JsonObject::TryParse(m_matchCriteria.GetConfigJson(), matchObject))
        {
            endpointUpdateObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MATCH_OBJECT_KEY, matchObject);
        }


        // add the endpoint update object to the array
        endpointUpdateArray.Append(endpointUpdateObject);

        // updates are an array, so we add this as an anonymous object
        transportObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY,
            endpointUpdateArray);

        // create the main node

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(m_transportId),
            transportObject);

        // wrap it all up so the json is valid

        outerWrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);


        return outerWrapperObject.Stringify();
    }
}
