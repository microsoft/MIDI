// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceTransportCommand.h"
#include "ServiceConfig.MidiServiceTransportCommand.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    MidiServiceTransportCommand::MidiServiceTransportCommand(winrt::guid const& transportId) noexcept
    {
        m_transportId = transportId;
    }

    _Use_decl_annotations_
    MidiServiceTransportCommand::MidiServiceTransportCommand(
        winrt::guid const& transportId, 
        winrt::hstring const& verb) noexcept
        : MidiServiceTransportCommand(transportId)
    {
        m_verb = verb;
    }

    _Use_decl_annotations_
    MidiServiceTransportCommand::MidiServiceTransportCommand(
        winrt::guid const& transportId, 
        winrt::hstring const& verb, 
        collections::IMap<winrt::hstring, winrt::hstring> const& arguments) noexcept
        : MidiServiceTransportCommand(transportId, verb)
    {
        m_arguments = arguments;
    }




    json::JsonObject MidiServiceTransportCommand::GetConfigJson() const noexcept
    {
        json::JsonObject outerWrapper;
        json::JsonObject transportObject;
        json::JsonObject topLevelTransportPluginSettingsObject;

        json::JsonObject commandObject;
        json::JsonObject argumentsObject;


        commandObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_NAME_KEY,
            json::JsonValue::CreateStringValue(m_verb));

        if (m_arguments.Size() > 0)
        {
            for (auto const& arg : m_arguments)
            {
                argumentsObject.SetNamedValue(
                    arg.Key(),
                    json::JsonValue::CreateStringValue(arg.Value()));
            }

            commandObject.SetNamedValue(
                MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_ARGUMENTS_KEY,
                argumentsObject
            );
        }

        // create the transport object with the child commandObject

        transportObject.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_COMMON_COMMAND_KEY,
            commandObject);

        // create the main node with the transport id property as key to the array

        topLevelTransportPluginSettingsObject.SetNamedValue(
            internal::GuidToString(m_transportId),
            transportObject);

        // wrap it all up so the json is valid

        outerWrapper.SetNamedValue(
            MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
            topLevelTransportPluginSettingsObject);

        return outerWrapper;
    }
}
