// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_ENDPOINT_CUSTOM_PROPERTIES_H
#define MIDI_ENDPOINT_CUSTOM_PROPERTIES_H

//#include <winrt/Windows.Foundation.h>
//#include <winrt/Windows.Foundation.Collections.h>

// this def messes with json so we need to undef it here
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")


#include <windows.h>


#include "MidiDefs.h"
#include "wstring_util.h"
#include "json_defs.h"

// The "customProperties" section is shared with all endpoints
//
// "endpointTransportPluginSettings":
// {
//   "{transport guid}" :
//   {
//     "update"
//     [
//       {
//         "match" :
//         {
//           "SWD" : "..."
//         },
// 
//         "customProperties" :
//         {
//           "name" : "new name",
//           "description" : "kdjflskjdfsdkjf" ... ,
//           "image" : "ksddkfjkdjffjk", 
//           ...
//           "midi1Ports" :
//           {
//             "namingApproach" : "classicCompatible",
//             "sources" :
//             [
//               {
//                 "groupIndex" : 0,
//                 "name" : "New Port Name"
//               },
//               {
//                 "groupIndex" : 2,
//                 "name" : "New Port Name2"
//               }
//             ],
//             "destinations" :
//             [
//               {
//                 "groupIndex" : 0,
//                 "name" : "New Port Name3"
//               }
//             ]
//           }
// 
//         }
// 
//       }
//     ]
//   }
// }
//

struct MidiEndpointCustomMidi1PortProperties
{
    uint8_t GroupIndex;
    winrt::hstring Name;
};

enum MidiEndpointCustomMidi1NamingApproach
{
    Default = 0,
    UseClassicCompatible = 1,
    UseNewStyle = 2
};

class MidiEndpointCustomProperties
{
public:
    static winrt::hstring PropertyKey;  // main property key for this type. defined in impl file

    MidiEndpointCustomProperties() = default;

    _Success_(return != nullptr)
    static std::shared_ptr<MidiEndpointCustomProperties> FromJson(_In_::winrt::Windows::Data::Json::JsonObject const& customPropertiesObject);

    _Success_(return == true)
    bool WriteAllProperties(_In_ std::vector<DEVPROPERTY>& destination);

    _Success_(return == true)
    bool WriteNonCommonProperties(_In_ std::vector<DEVPROPERTY>&destination);


    _Success_(return == true)
    bool WriteJson(_In_::winrt::Windows::Data::Json::JsonObject& customPropertiesObject);

    winrt::hstring Name{};
    winrt::hstring Description{};
    winrt::hstring Image{};
    bool RequiresNoteOffTranslation{};
    bool SupportsMidiPolyphonicExpression{};
    uint16_t RecommendedControlChangeIntervalMilliseconds{};

 //   bool UmpOnly{ false };
    MidiEndpointCustomMidi1NamingApproach Midi1NamingApproach{ MidiEndpointCustomMidi1NamingApproach::Default };

    std::map<uint8_t, MidiEndpointCustomMidi1PortProperties> Midi1Sources{};
    std::map<uint8_t, MidiEndpointCustomMidi1PortProperties> Midi1Destinations{};

private:

    void Normalize();

    DEVPROP_BOOLEAN m_devPropTrue{ DEVPROP_TRUE };
    DEVPROP_BOOLEAN m_devPropFalse{ DEVPROP_FALSE };

    //MidiEndpointCustomMidi1NamingApproach m_selectedPortNamingDevProperty{ MidiEndpointCustomMidi1NamingApproach::Default };
};



#endif  // ifdef JSON_CUSTOM_PROPERTY_HELPER_H