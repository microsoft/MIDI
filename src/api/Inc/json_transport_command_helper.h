// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef JSON_TRANSPORT_COMMAND_HELPER_H
#define JSON_TRANSPORT_COMMAND_HELPER_H

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

//#include "MidiDefs.h"
#include "wstring_util.h"
#include "json_defs.h"

namespace json = ::winrt::Windows::Data::Json;

namespace WindowsMidiServicesInternal
{
    namespace internal = ::WindowsMidiServicesInternal;


    class MidiTransportCommandHelper
    {
    public:
        inline static bool TransportObjectContainsCommand(_In_ json::JsonObject transportObject)
        {
            if (transportObject.GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_KEY, nullptr) != nullptr)
            {
                return true;
            }

            return false;
        }

        // pulls the command out from inside this object. You provide the transport wrapper object
        inline static MidiTransportCommandHelper ParseCommand(_In_ json::JsonObject transportObject)
        {
            MidiTransportCommandHelper result;

            // parses into a new instance of this object

            auto commandObject = transportObject.GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_KEY, nullptr);

            if (commandObject == nullptr)
            {
                result.m_command = L"";
            }
            else
            {
                auto commandName = transportObject.GetNamedString(MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_NAME_KEY, L"");
                result.m_command = commandName.c_str();

                auto arguments = transportObject.GetNamedObject(MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_ARGUMENTS_KEY, nullptr);
                if (arguments != nullptr)
                {
                    for (auto const& argument : arguments)
                    {
                        std::wstring key { argument.Key().c_str() };
                        std::wstring value {};

                        value = arguments.GetNamedString(argument.Key()).c_str();

                        result.m_arguments.emplace(key, value);
                    }
                }

            }

            return result;
        }

        inline std::wstring Command()
        {
            return m_command;
        }

        inline std::map<std::wstring, std::wstring>* Arguments()
        {
            return &m_arguments;
        }

    private:
        std::wstring m_command{};
        std::map<std::wstring, std::wstring> m_arguments;

    };

}

#pragma pop_macro("max")
#endif  // ifdef max

#endif  // ifdef JSON_TRANSPORT_COMMAND_HELPER_H