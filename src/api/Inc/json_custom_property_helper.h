// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef JSON_CUSTOM_PROPERTY_HELPER_H
#define JSON_CUSTOM_PROPERTY_HELPER_H

// this def messes with json so we need to undef it here
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#ifdef max
#pragma push_macro("max")
#undef max


//#include <winrt/Windows.Foundation.h>
//#include <winrt/Windows.Foundation.Collections.h>

#include "MidiDefs.h"
#include "wstring_util.h"
#include "json_defs.h"

namespace json = ::winrt::Windows::Data::Json;

namespace WindowsMidiServicesInternal
{
    namespace internal = ::WindowsMidiServicesInternal;


    class MidiEndpointCustomPropertiesHelper
    {
    public:

        _Success_(return == true)
        inline bool ReadFromJsonObject(_In_ json::JsonObject updateObject)
        {
            try
            {
                // user-supplied name
                m_name = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_NAME_PROPERTY, L"").c_str());

                // user-supplied description
                m_description = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_DESCRIPTION_PROPERTY, L"").c_str());

                // user-supplied small image
                m_smallImagePath = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SMALL_IMAGE_PROPERTY, L"").c_str());

                // user-supplied large image
                m_largeImagePath = internal::TrimmedWStringCopy(updateObject.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_LARGE_IMAGE_PROPERTY, L"").c_str());

                // requires note off translation
                m_requiresNoteOffTranslation = updateObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_REQUIRES_NOTE_OFF_TRANSLATION_PROPERTY, false);

                // supports MPE
                m_supportsMidiPolyphonicExpression = updateObject.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_SUPPORTS_MPE_PROPERTY, false);

                // recommended CC automation interval
                auto ccval = updateObject.GetNamedNumber(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CUSTOM_RECOMMENDED_CC_INTERVAL_MS_PROPERTY, 0);
                if (ccval > 0 && ccval <= std::numeric_limits<uint16_t>::max())
                {
                    m_recommendedCCInterval = static_cast<uint16_t>(ccval);
                }
                else
                {
                    m_recommendedCCInterval = 0;
                }

                return true;

            }
            catch (...)
            {
                return false;
            }

        }

        _Success_(return == true)
        inline bool WriteToPropertiesVector(_In_ std::vector<DEVPROPERTY>& destination)
        {
            // name
            if (!m_name.empty())
            {
                destination.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_STRING, static_cast<ULONG>((m_name.length() + 1) * sizeof(WCHAR)), (PVOID)m_name.c_str() });
            }
            else
            {
                // delete any existing property value, because it is blank in the config
                destination.push_back({ {PKEY_MIDI_CustomEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_EMPTY, 0, nullptr });
            }


            // description
            if (!m_description.empty())
            {
                destination.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_STRING, static_cast<ULONG>((m_description.length() + 1) * sizeof(WCHAR)), (PVOID)m_description.c_str() });
            }
            else
            {
                // delete any existing property value, because it is empty
                destination.push_back({ {PKEY_MIDI_CustomDescription, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // small image path
            if (!m_smallImagePath.empty())
            {
                destination.push_back({ {PKEY_MIDI_CustomSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_STRING, static_cast<ULONG>((m_smallImagePath.length() + 1) * sizeof(WCHAR)), (PVOID)m_smallImagePath.c_str() });
            }
            else
            {
                // delete any existing property value, because it is blank in the config
                destination.push_back({ {PKEY_MIDI_CustomSmallImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // large image path
            if (!m_largeImagePath.empty())
            {
                destination.push_back({ {PKEY_MIDI_CustomLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_STRING, static_cast<ULONG>((m_largeImagePath.length() + 1) * sizeof(WCHAR)), (PVOID)m_largeImagePath.c_str() });
            }
            else
            {
                // delete any existing property value, because it is blanko in the config
                destination.push_back({ {PKEY_MIDI_CustomLargeImagePath, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // requires note off translation
            if (m_requiresNoteOffTranslation)
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
            if (m_supportsMidiPolyphonicExpression)
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
            if (m_recommendedCCInterval != 0)
            {
                destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_UINT16, sizeof(uint16_t), (PVOID)&m_recommendedCCInterval });
            }
            else
            {
                destination.push_back({ {PKEY_MIDI_RequiresNoteOffTranslation, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_UINT16, sizeof(uint16_t), (PVOID)&m_recommendedCCInterval });
            }


            return true;
        }

    private:
        std::wstring m_name{};
        std::wstring m_description{};
        std::wstring m_smallImagePath{};
        std::wstring m_largeImagePath{};
        bool m_requiresNoteOffTranslation{ false };
        bool m_supportsMidiPolyphonicExpression{ false };
        uint16_t m_recommendedCCInterval{ 0 };


        DEVPROP_BOOLEAN m_devPropTrue{ DEVPROP_TRUE };
        DEVPROP_BOOLEAN m_devPropFalse{ DEVPROP_FALSE };

    };


}

#pragma pop_macro("max")
#endif  // ifdef max

#endif  // ifdef JSON_CUSTOM_PROPERTY_HELPER_H