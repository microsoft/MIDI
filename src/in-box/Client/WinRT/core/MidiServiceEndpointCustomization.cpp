// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceEndpointCustomization.h"
#include "ServiceConfig.MidiServiceEndpointCustomization.g.cpp"

#include "MidiServiceConfigEndpointMatchCriteria.h"
#include "MidiServiceEndpointCustomizationProvenance.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    midi2enum::Midi1PortNamingApproach MidiServiceEndpointCustomization::Midi1PortNamingApproach() const noexcept
    {
        switch (m_properties->Midi1NamingApproach)
        {
        case WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseLegacyWinMM:
            return midi2enum::Midi1PortNamingApproach::UseClassicCompatible;
        case WindowsMidiServicesNamingLib::Midi1PortNameSelection::UseNewStyleName:
            return midi2enum::Midi1PortNamingApproach::UseNewStyle;
        default:
            return midi2enum::Midi1PortNamingApproach::Default;
        }
    }


    _Use_decl_annotations_
    void MidiServiceEndpointCustomization::InternalInitializeFromJson(
        winrt::guid const& transportId,
        json::JsonObject const& entryObject) noexcept
    {
        try
        {
            m_transportId = transportId;

            if (entryObject == nullptr)
            {
                return;
            }

            auto const matchObject = entryObject.GetNamedObject(
                WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::PropertyKey, nullptr);

            if (matchObject != nullptr)
            {
                m_matchCriteria = MidiServiceConfigEndpointMatchCriteria::FromJson(matchObject.Stringify());
            }

            auto const provenanceObject = entryObject.GetNamedObject(
                WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance::PropertyKey, nullptr);

            if (provenanceObject != nullptr)
            {
                auto const parsed = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomizationProvenance::FromJson(provenanceObject);

                if (parsed != nullptr)
                {
                    winrt::get_self<MidiServiceEndpointCustomizationProvenance>(m_provenance)->InternalSetProvenanceObject(parsed);
                }
            }

            auto const propertiesObject = entryObject.GetNamedObject(
                WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::PropertyKey, nullptr);

            if (propertiesObject != nullptr)
            {
                auto const parsed = WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties::FromJson(propertiesObject);

                if (parsed != nullptr)
                {
                    m_properties = parsed;

                    for (auto const& source : m_properties->Midi1Sources)
                    {
                        m_sourceNames.Insert(source.second.GroupIndex, source.second.Name);
                    }

                    for (auto const& destination : m_properties->Midi1Destinations)
                    {
                        m_destinationNames.Insert(destination.second.GroupIndex, destination.second.Name);
                    }
                }
            }

            m_resolvedEndpointDeviceId = entryObject.GetNamedString(
                MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATION_RESOLVED_ENDPOINT_DEVICE_ID_KEY, L"");

            m_hasUserContent = entryObject.GetNamedBoolean(
                MIDI_CONFIG_JSON_ENDPOINT_CUSTOMIZATION_HAS_USER_CONTENT_KEY, false);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error reading endpoint customization entry.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception reading endpoint customization entry.");
        }
    }
}
