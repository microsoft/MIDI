// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiLoopbackUpdateConfig.h"
#include "Transports.Loopback.MidiLoopbackUpdateConfig.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    // The shape MidiLoopbackCreationConfig saves, holding only what was set. The configuration
    // file merges it into the saved entry, so everything left out stays as it is.
    json::JsonObject MidiLoopbackUpdateConfig::ConfigJson() const noexcept
    {
        try
        {
            auto buildEndpoint = [](
                bool const hasName, winrt::hstring const& name,
                bool const hasDescription, winrt::hstring const& description,
                bool const hasImage, winrt::hstring const& image)
                {
                    json::JsonObject endpoint{};

                    if (hasName)
                    {
                        endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY, json::JsonValue::CreateStringValue(name));
                    }

                    if (hasDescription)
                    {
                        endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY, json::JsonValue::CreateStringValue(description));
                    }

                    // written even when empty, which is what removes a saved picture
                    if (hasImage)
                    {
                        endpoint.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_IMAGE_PROPERTY, json::JsonValue::CreateStringValue(image));
                    }

                    return endpoint;
                };

            json::JsonObject association{};

            auto const endpointA = buildEndpoint(
                m_hasEndpointAName, m_endpointAName,
                m_hasEndpointADescription, m_endpointADescription,
                m_hasEndpointAImageFileName, m_endpointAImageFileName);

            if (endpointA.Size() > 0)
            {
                association.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY, endpointA);
            }

            auto const endpointB = buildEndpoint(
                m_hasEndpointBName, m_endpointBName,
                m_hasEndpointBDescription, m_endpointBDescription,
                m_hasEndpointBImageFileName, m_endpointBImageFileName);

            if (endpointB.Size() > 0)
            {
                association.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY, endpointB);
            }

            // on the association, because one flag mutes both directions
            if (m_hasIsMuted)
            {
                association.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_MUTED_PROPERTY, json::JsonValue::CreateBooleanValue(m_isMuted));
            }

            // anything that is not Off is treated as Mute, as the creation config does
            if (m_hasFeedbackProtection)
            {
                association.SetNamedValue(
                    MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_PROTECTION_PROPERTY,
                    json::JsonValue::CreateStringValue(internal::FeedbackProtectionJsonValue(
                        m_feedbackProtection != loop::MidiLoopbackFeedbackProtection::Off)));
            }

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
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception building loopback update config json.");
            return nullptr;
        }
    }
}
