#include "pch.h"
#include "MidiServiceEndpointCustomizationRemovalConfig.h"
#include "ServiceConfig.MidiServiceEndpointCustomizationRemovalConfig.g.cpp"

#include "MidiServiceConfigEndpointMatchCriteria.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    _Use_decl_annotations_
    MidiServiceEndpointCustomizationRemovalConfig::MidiServiceEndpointCustomizationRemovalConfig(
        winrt::guid const& transportId)
    {
        m_transportId = transportId;
    }

    _Use_decl_annotations_
    MidiServiceEndpointCustomizationRemovalConfig::MidiServiceEndpointCustomizationRemovalConfig(
        winrt::guid const& transportId,
        svc::MidiServiceConfigEndpointMatchCriteria const& matchCriteria)
    {
        m_transportId = transportId;
        m_matchCriteria = matchCriteria;
    }

    json::JsonObject MidiServiceEndpointCustomizationRemovalConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject matchObject;

            if (m_matchCriteria == nullptr || !json::JsonObject::TryParse(m_matchCriteria.GetConfigJson(), matchObject))
            {
                return nullptr;
            }

            json::JsonObject entryObject;
            entryObject.SetNamedValue(MidiServiceConfigEndpointMatchCriteria::MatchObjectKey(), matchObject);

            json::JsonArray entriesArray;
            entriesArray.Append(entryObject);

            json::JsonObject removeObject;
            removeObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, entriesArray);

            json::JsonObject transportObject;
            transportObject.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, removeObject);

            json::JsonObject topLevelTransportPluginSettingsObject;
            topLevelTransportPluginSettingsObject.SetNamedValue(
                internal::GuidToString(m_transportId),
                transportObject);

            json::JsonObject outerWrapperObject;
            outerWrapperObject.SetNamedValue(
                MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
                topLevelTransportPluginSettingsObject);

            return outerWrapperObject;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error building endpoint customization removal config json.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception building endpoint customization removal config json.");
            return nullptr;
        }
    }
}
