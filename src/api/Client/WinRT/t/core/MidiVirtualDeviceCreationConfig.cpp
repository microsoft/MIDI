// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiVirtualDeviceCreationConfig.h"
#include "Transports.Virtual.MidiVirtualDeviceCreationConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Virtual::implementation
{
    _Use_decl_annotations_
    MidiVirtualDeviceCreationConfig::MidiVirtualDeviceCreationConfig(
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& manufacturer,
        midi2enum::MidiDeclaredEndpointInfo const& declaredEndpointInfo
    )
    {
        Name(name);
        Description(description);
        Manufacturer(manufacturer);

        m_declaredEndpointInfo = declaredEndpointInfo;
    }

    _Use_decl_annotations_
    MidiVirtualDeviceCreationConfig::MidiVirtualDeviceCreationConfig(
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& manufacturer,
        midi2enum::MidiDeclaredEndpointInfo const& declaredEndpointInfo,
        midi2enum::MidiDeclaredDeviceIdentity const& declaredDeviceIdentity
        ) : MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo)
    {
        m_declaredDeviceIdentity = declaredDeviceIdentity;
    }

    _Use_decl_annotations_
    MidiVirtualDeviceCreationConfig::MidiVirtualDeviceCreationConfig(
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& manufacturer,
        midi2enum::MidiDeclaredEndpointInfo const& declaredEndpointInfo,
        midi2enum::MidiDeclaredDeviceIdentity const& declaredDeviceIdentity,
        midi2enum::MidiEndpointUserSuppliedInfo const& userSuppliedInfo
    ) : MidiVirtualDeviceCreationConfig(name, description, manufacturer, declaredEndpointInfo, declaredDeviceIdentity)
    {
        m_userSuppliedInfo = userSuppliedInfo;
    }



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
    json::JsonObject MidiVirtualDeviceCreationConfig::ConfigJson() const noexcept
    {
        try
        {
        // create the json for creating the endpoint

        json::JsonObject endpointDefinitionObject;

        json::JsonArray virtualDevicesCreationArray;
        json::JsonObject transportObject;
        json::JsonObject topLevelTransportPluginSettingsObject;
        json::JsonObject outerWrapperObject;

        // set all of our properties.

        internal::JsonSetGuidProperty(
            endpointDefinitionObject,
            MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY,
            m_associationId);

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY,
            json::JsonValue::CreateStringValue(m_declaredEndpointInfo.ProductInstanceId()));  // don't truncate here. Happens in service

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY,
            json::JsonValue::CreateStringValue(m_endpointName));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY,
            json::JsonValue::CreateStringValue(m_description));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_MANUFACTURER_PROPERTY,
            json::JsonValue::CreateStringValue(m_manufacturer));

        endpointDefinitionObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_UMP_ONLY_PROPERTY,
            json::JsonValue::CreateBooleanValue(m_umpOnly));


        // TODO: Other props that have to be set at the service level and not in-protocol

        virtualDevicesCreationArray.Append(endpointDefinitionObject);

        // create the transport object with the child creation node

        transportObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY,
            virtualDevicesCreationArray);

        // create the main node with the transport id property as key to the array

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(virt::MidiVirtualDeviceManager::TransportId()),
            transportObject);

        // wrap it all up so the json is valid

        outerWrapperObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);


        return outerWrapperObject;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error building virtual device creation config json.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception building virtual device creation config json.");
            return nullptr;
        }
    }



}
