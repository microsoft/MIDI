// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

//#include "SWDevice.h"
//#include <setupapi.h>
//#include <initguid.h>
//#include <Devpkey.h>
//#include "MidiDefs.h"

#include "MidiEndpointCustomProperties.h"

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
        m_props->Name = internal::TrimmedHStringCopy(name);
        m_props->Description = internal::TrimmedHStringCopy(description);
    }

    _Use_decl_annotations_
    MidiServiceEndpointCustomizationConfig::MidiServiceEndpointCustomizationConfig(
        winrt::guid const& transportId, 
        winrt::hstring const& name, 
        winrt::hstring const& description, 
        winrt::hstring const& imageFileName) noexcept
        : MidiServiceEndpointCustomizationConfig(transportId, name, description)
    {
        m_props->Image = internal::TrimmedHStringCopy(imageFileName);
    }

    _Use_decl_annotations_
    MidiServiceEndpointCustomizationConfig::MidiServiceEndpointCustomizationConfig(
        winrt::guid const& transportId,
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& imageFileName,
        bool const requiresNoteOffTranslation,
        bool const supportsMidiPolyphonicExpression,
        uint16_t const recommendedControlChangeIntervalMilliseconds) noexcept
        : MidiServiceEndpointCustomizationConfig(transportId, name, description, imageFileName)
    {
        m_props->RequiresNoteOffTranslation = requiresNoteOffTranslation;
        m_props->SupportsMidiPolyphonicExpression = supportsMidiPolyphonicExpression;
        m_props->RecommendedControlChangeIntervalMilliseconds = recommendedControlChangeIntervalMilliseconds;
    }


    winrt::hstring MidiServiceEndpointCustomizationConfig::GetConfigJson() const noexcept
    {
        json::JsonObject matchObject;
        json::JsonObject customPropertiesObject;
        json::JsonArray endpointUpdateArray;
        json::JsonObject endpointUpdateObject;
        json::JsonObject transportObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject outerWrapperObject;


        // match object within the update object. What is supported will vary by transport
        if (m_matchCriteria != nullptr && json::JsonObject::TryParse(m_matchCriteria.GetConfigJson(), matchObject))
        {
            endpointUpdateObject.SetNamedValue(MidiServiceConfigEndpointMatchCriteria::MatchObjectKey(), matchObject);
        }

        if (m_props->WriteJson(customPropertiesObject))
        {
            endpointUpdateObject.SetNamedValue(MidiEndpointCustomProperties::PropertyKey, customPropertiesObject);
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
