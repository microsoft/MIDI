// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiBasicLoopbackUpdateConfig.h"
#include "Transports.BasicLoopback.MidiBasicLoopbackUpdateConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    // The shape MidiBasicLoopbackCreationConfig saves, holding only what was set. The
    // configuration file merges it into the saved entry, so everything left out stays as it is.
    json::JsonObject MidiBasicLoopbackUpdateConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject endpoint{};

            if (m_hasName)
            {
                endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, json::JsonValue::CreateStringValue(m_name));
            }

            if (m_hasDescription)
            {
                endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, json::JsonValue::CreateStringValue(m_description));
            }

            // written even when empty, which is what removes a saved picture
            if (m_hasImageFileName)
            {
                endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_IMAGE_PROPERTY, json::JsonValue::CreateStringValue(m_imageFileName));
            }

            // on the endpoint, which is where a basic loopback keeps both
            if (m_hasIsMuted)
            {
                endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MUTED_PROPERTY, json::JsonValue::CreateBooleanValue(m_isMuted));
            }

            if (m_hasFeedbackProtection)
            {
                endpoint.SetNamedValue(
                    MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_PROPERTY,
                    json::JsonValue::CreateStringValue(internal::FeedbackProtectionJsonValue(
                        m_feedbackProtection != bloop::MidiBasicLoopbackFeedbackProtection::Off)));
            }

            json::JsonObject association{};
            association.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_ENDPOINT_KEY, endpoint);

            json::JsonObject create{};
            create.SetNamedValue(internal::GuidToString(m_associationId), association);

            json::JsonObject transport{};
            transport.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, create);

            json::JsonObject transportSettings{};
            transportSettings.SetNamedValue(internal::GuidToString(TransportId()), transport);

            json::JsonObject wrapper{};
            wrapper.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, transportSettings);

            return wrapper;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception building basic loopback update config json.");
            return nullptr;
        }
    }
}
