// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include <windows.h>
//#include <Devpropdef.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <atlconv.h>
#include <string>

#include <setupapi.h>
#include <initguid.h>
#include <Devpkey.h>

#include "MidiDefs.h"
#include "hstring_util.h"
#include "json_defs.h"

#include "MidiEndpointCustomProperties.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>


#ifdef max
#pragma push_macro("max")
#undef max

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_PROPERTIES_PROPERTY_KEY                     L"customProperties"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY_KEY                           L"name"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY_KEY                    L"description"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_IMAGE_PROPERTY_KEY                          L"image"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_REQUIRES_NOTE_OFF_TRANSLATION_PROPERTY_KEY  L"requiresNoteOffTranslation"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SUPPORTS_MPE_PROPERTY_KEY                   L"supportsMidiPolyphonicExpression"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_RECOMMENDED_CC_INTERVAL_MS_PROPERTY_KEY     L"recommendedControlChangeIntervalMilliseconds"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_PORTS_PROPERTY_KEY                           L"midi1Ports"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_PROPERTY_KEY                       L"namingApproach"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_COMPATIBLE                   L"classicCompatible"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_NEW_STYLE                    L"newStyle"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_DEFAULT                      L"default"

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_SOURCES_ARRAY_PROPERTY_KEY                   L"sources"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_DESTINATIONS_ARRAY_PROPERTY_KEY              L"destinations"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_GROUP_INDEX_PROPERTY_KEY          L"groupIndex"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_CUSTOM_NAME_PROPERTY_KEY          L"name"


// should pull in the WinMM header to get this constant,
// but that's a lot of baggage just for that one value
#define MAX_WINMM_NAME_SIZE 32

namespace json = ::winrt::Windows::Data::Json;

namespace internal = ::WindowsMidiServicesInternal;

winrt::hstring MidiEndpointCustomProperties::PropertyKey{ MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_PROPERTIES_PROPERTY_KEY };


void MidiEndpointCustomProperties::Normalize()
{
    Name = internal::TrimmedHStringCopy(Name);
    Description = internal::TrimmedHStringCopy(Description);
    Image = internal::TrimmedHStringCopy(Image);

    for(auto& source : Midi1Sources)
    {
        if (source.second.Name.size() > MAX_WINMM_NAME_SIZE)
        {
            source.second.Name = internal::TruncateHStringCopy(source.second.Name, MAX_WINMM_NAME_SIZE);
        }
    }

    for (auto& dest : Midi1Destinations)
    {
        if (dest.second.Name.size() > MAX_WINMM_NAME_SIZE)
        {
            dest.second.Name = internal::TruncateHStringCopy(dest.second.Name, MAX_WINMM_NAME_SIZE);
        }
    }


}

_Use_decl_annotations_
std::shared_ptr<MidiEndpointCustomProperties> MidiEndpointCustomProperties::FromJson(json::JsonObject const& customPropertiesObject)
{
    try
    {
        auto props = std::make_shared<MidiEndpointCustomProperties>();

        // user-supplied name
        props->Name = customPropertiesObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY_KEY, L"");

        // user-supplied description
        props->Description = customPropertiesObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY_KEY, L"");

        // user-supplied image
        props->Image = customPropertiesObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_IMAGE_PROPERTY_KEY, L"");

        // requires note off translation
        props->RequiresNoteOffTranslation = customPropertiesObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_REQUIRES_NOTE_OFF_TRANSLATION_PROPERTY_KEY, false);

        // supports MPE
        props->SupportsMidiPolyphonicExpression = customPropertiesObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SUPPORTS_MPE_PROPERTY_KEY, false);

        // recommended CC automation interval
        auto ccval = customPropertiesObject.GetNamedNumber(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_RECOMMENDED_CC_INTERVAL_MS_PROPERTY_KEY, 0);
        if (ccval > 0 && ccval <= std::numeric_limits<uint16_t>::max())
        {
            props->RecommendedControlChangeIntervalMilliseconds = static_cast<uint16_t>(ccval);
        }
        else
        {
            props->RecommendedControlChangeIntervalMilliseconds = 0;
        }


        if (customPropertiesObject.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_PORTS_PROPERTY_KEY))
        {
            auto midi1Ports = customPropertiesObject.GetNamedObject(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_PORTS_PROPERTY_KEY);

            // get the naming approach to use for MIDI 1 ports. Custom names will always override. 
            // "default" means to use the default for Windows all-up.

            auto naming = customPropertiesObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_PROPERTY_KEY, MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_DEFAULT);

            if (naming == MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_DEFAULT)
            {
                props->Midi1NamingApproach = MidiEndpointCustomMidi1NamingApproach::Default;
            }
            else if (naming == MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_COMPATIBLE)
            {
                props->Midi1NamingApproach = MidiEndpointCustomMidi1NamingApproach::UseClassicCompatible;
            }
            else if (naming == MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_NEW_STYLE)
            {
                props->Midi1NamingApproach = MidiEndpointCustomMidi1NamingApproach::UseNewStyle;
            }
            else
            {
                props->Midi1NamingApproach = MidiEndpointCustomMidi1NamingApproach::Default;
            }

            // read MIDI 1 source ports info

            for (auto const& namedArrayKey : { 
                winrt::hstring{ MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_SOURCES_ARRAY_PROPERTY_KEY }, 
                winrt::hstring{ MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_DESTINATIONS_ARRAY_PROPERTY_KEY } })
            {
                if (customPropertiesObject.HasKey(namedArrayKey))
                {
                    auto ports = customPropertiesObject.GetNamedArray(namedArrayKey);

                    for (auto const& it : ports)
                    {
                        auto obj = it.as<json::JsonObject>();

                        if (obj.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_GROUP_INDEX_PROPERTY_KEY) &&
                            obj.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_CUSTOM_NAME_PROPERTY_KEY))
                        {
                            auto groupIndex = static_cast<int>(obj.GetNamedNumber(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_GROUP_INDEX_PROPERTY_KEY));
                            auto name = internal::TrimmedHStringCopy(obj.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_CUSTOM_NAME_PROPERTY_KEY));

                            // if invalid group index, reject the entry
                            if (groupIndex < 0 || groupIndex > 15)
                            {
                                continue;
                            }

                            // if no name, reject this entry
                            if (name.empty())
                            {
                                continue;
                            }

                            MidiEndpointCustomMidi1PortProperties portProperties{};

                            portProperties.Name = name;
                            portProperties.GroupIndex = static_cast<uint8_t>(groupIndex);


                            if (namedArrayKey == MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_SOURCES_ARRAY_PROPERTY_KEY)
                            {
                                props->Midi1Sources.emplace(portProperties.GroupIndex, portProperties);
                            }
                            else if (namedArrayKey == MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_DESTINATIONS_ARRAY_PROPERTY_KEY)
                            {
                                props->Midi1Destinations.emplace(portProperties.GroupIndex, portProperties);
                            }
                        }
                    }
                }
            }
        }

        props->Normalize();

        return props;

    }
    catch (...)
    {
        OutputDebugString(L"Exception parsing MIDI Endpoint Custom Properties JSON.");
    }

    return nullptr;
}

_Use_decl_annotations_
bool MidiEndpointCustomProperties::WriteJson(json::JsonObject& customPropertiesObject)
{
    Normalize();

    try
    {
        if (!Name.empty())
        {
            customPropertiesObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY_KEY, 
                json::JsonValue::CreateStringValue(Name));
        }

        if (!Description.empty())
        {
            customPropertiesObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY_KEY, 
                json::JsonValue::CreateStringValue(Description));
        }

        if (!Image.empty())
        {
            customPropertiesObject.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_IMAGE_PROPERTY_KEY, 
                json::JsonValue::CreateStringValue(Image));
        }

        customPropertiesObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_REQUIRES_NOTE_OFF_TRANSLATION_PROPERTY_KEY, 
            json::JsonValue::CreateBooleanValue(RequiresNoteOffTranslation));

        customPropertiesObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SUPPORTS_MPE_PROPERTY_KEY, 
            json::JsonValue::CreateBooleanValue(SupportsMidiPolyphonicExpression));

        customPropertiesObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_RECOMMENDED_CC_INTERVAL_MS_PROPERTY_KEY, 
            json::JsonValue::CreateNumberValue(RecommendedControlChangeIntervalMilliseconds));


        // naming approach for ports

        winrt::hstring namingApproach{};

        switch (Midi1NamingApproach)
        {
        case MidiEndpointCustomMidi1NamingApproach::Default:
            namingApproach = MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_DEFAULT;
            break;
        case MidiEndpointCustomMidi1NamingApproach::UseClassicCompatible:
            namingApproach = MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_COMPATIBLE;
            break;
        case MidiEndpointCustomMidi1NamingApproach::UseNewStyle:
            namingApproach = MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_VALUE_NEW_STYLE;
            break;
        }


        json::JsonObject midi1Ports;
        json::JsonArray sourcesArray;
        json::JsonArray destinationsArray;

        midi1Ports.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAMING_APPROACH_PROPERTY_KEY,
            json::JsonValue::CreateStringValue(namingApproach));

        // MIDI 1 source port properties
        for (auto const& source : Midi1Sources)
        {
            json::JsonObject obj;

            obj.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_GROUP_INDEX_PROPERTY_KEY, 
                json::JsonValue::CreateNumberValue(source.second.GroupIndex));

            obj.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_CUSTOM_NAME_PROPERTY_KEY,
                json::JsonValue::CreateStringValue(source.second.Name));

            sourcesArray.Append(obj);
        }

        for (auto const& dest : Midi1Destinations)
        {
            json::JsonObject obj;

            obj.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_GROUP_INDEX_PROPERTY_KEY,
                json::JsonValue::CreateNumberValue(dest.second.GroupIndex));

            obj.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_NAME_ENTRY_CUSTOM_NAME_PROPERTY_KEY,
                json::JsonValue::CreateStringValue(dest.second.Name));

            destinationsArray.Append(obj);
        }

        // only write sources if there are any
        if (sourcesArray.Size() > 0)
        {
            midi1Ports.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_SOURCES_ARRAY_PROPERTY_KEY,
                sourcesArray);
        }

        // only write destinations if there are any
        if (destinationsArray.Size() > 0)
        {
            midi1Ports.SetNamedValue(
                MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_DESTINATIONS_ARRAY_PROPERTY_KEY,
                destinationsArray);
        }

        // we write this no matter what because it has the namingApproach value in it

        customPropertiesObject.SetNamedValue(
            MIDI_CONFIG_JSON_ENDPOINT_COMMON_MIDI1_PORTS_PROPERTY_KEY,
            midi1Ports);

        return true;
    }
    catch (...)
    {
        OutputDebugString(L"Exception writing MIDI Endpoint Custom Properties JSON.");
    }

    return false;
}


// write only the properties which aren't in the Common Properties structure at endpoint creation time
_Use_decl_annotations_
bool MidiEndpointCustomProperties::WriteNonCommonProperties(_In_ std::vector<DEVPROPERTY>& destination)
{
    Normalize();

    // image file name / path
    if (!Image.empty())
    {
        destination.push_back({ {PKEY_MIDI_CustomImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((Image.size() + 1) * sizeof(WCHAR)), (PVOID)Image.c_str() });
    }
    else
    {
        // delete any existing property value, because it is blank in the config
        destination.push_back({ {PKEY_MIDI_CustomImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_EMPTY, 0, nullptr });
    }

    // requires note off translation
    if (RequiresNoteOffTranslation)
    {
        destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, sizeof(DEVPROP_BOOLEAN), (PVOID)&m_devPropTrue });
    }
    else
    {
        destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, sizeof(DEVPROP_BOOLEAN), (PVOID)&m_devPropFalse });
    }

    // supports MPE
    if (SupportsMidiPolyphonicExpression)
    {
        destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, sizeof(DEVPROP_BOOLEAN), (PVOID)&m_devPropTrue });
    }
    else
    {
        destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, sizeof(DEVPROP_BOOLEAN), (PVOID)&m_devPropFalse });
    }

    // cc automation interval
    if (RecommendedControlChangeIntervalMilliseconds != 0)
    {
        destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_UINT16, sizeof(uint16_t), (PVOID)&RecommendedControlChangeIntervalMilliseconds });
    }
    else
    {
        destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_UINT16, sizeof(uint16_t), (PVOID)&RecommendedControlChangeIntervalMilliseconds });
    }

    // naming
    switch (Midi1NamingApproach)
    {
    case MidiEndpointCustomMidi1NamingApproach::UseNewStyle:
        m_selectedPortNamingDevProperty = Midi1PortNameSelectionProperty::PortName_UseFilterPlusBlockName;
        break;

    case MidiEndpointCustomMidi1NamingApproach::UseClassicCompatible:
        m_selectedPortNamingDevProperty = Midi1PortNameSelectionProperty::PortName_UseLegacyWinMM;
        break;

    default:
        m_selectedPortNamingDevProperty = Midi1PortNameSelectionProperty::PortName_UseGlobalDefault;
        break;
    }

    destination.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
        DEVPROP_TYPE_UINT32, (ULONG)sizeof(Midi1PortNameSelectionProperty), (PVOID)&m_selectedPortNamingDevProperty });

    return true;
}


_Use_decl_annotations_
bool MidiEndpointCustomProperties::WriteAllProperties(std::vector<DEVPROPERTY>& destination)
{
    if (WriteNonCommonProperties(destination))
    {
        // name
        if (!Name.empty())
        {
            destination.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, static_cast<ULONG>((Name.size() + 1) * sizeof(WCHAR)), (PVOID)Name.c_str() });
        }
        else
        {
            // delete any existing property value, because it is blank in the config
            destination.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_EMPTY, 0, nullptr });
        }


        // description
        if (!Description.empty())
        {
            destination.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_STRING, static_cast<ULONG>((Description.size() + 1) * sizeof(WCHAR)), (PVOID)Description.c_str() });
        }
        else
        {
            // delete any existing property value, because it is empty
            destination.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                    DEVPROP_TYPE_EMPTY, 0, nullptr });
        }

        return true;
    }

    return false;
}



#pragma pop_macro("max")
#endif  // ifdef max
