// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostUpdateConfig.h"
#include "Transports.Network.MidiNetworkHostUpdateConfig.g.cpp"

// when this component goes in-box, move the json defs to the common json_defs.h
#include "..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

#include "midi_network_adapters.h"

namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    _Use_decl_annotations_
    void MidiNetworkHostUpdateConfig::NetworkAdapterId(winrt::guid const& value) noexcept
    {
        m_networkAdapterId = value;
        m_networkAdapterName = winrt::hstring{};
        m_networkAdapterPhysicalAddress = winrt::hstring{};

        try
        {
            ::WindowsMidiServicesInternal::MidiNetworkAdapterInfo adapter{};

            if (::WindowsMidiServicesInternal::TryGetMidiNetworkAdapter(value, adapter))
            {
                m_networkAdapterName = winrt::hstring{ adapter.Name };
                m_networkAdapterPhysicalAddress = winrt::hstring{ adapter.PhysicalAddress };
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception looking up a network adapter.");
        }
    }

    // The same host entry shape as MidiNetworkHostCreationConfig, under the "update" key. Only the
    // properties the caller set are written: the merge into the configuration file cannot delete a
    // key, so the rest of the entry, including the port and the service instance name, is left alone.
    json::JsonObject MidiNetworkHostUpdateConfig::ConfigJson() const noexcept
    {
        try
        {
            json::JsonObject hostObject{};

            if (m_createMidi1Ports.has_value())
            {
                hostObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY,
                    json::JsonValue::CreateBooleanValue(*m_createMidi1Ports));
            }

            if (m_fallbackMidi1PortCount.has_value())
            {
                hostObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
                    json::JsonValue::CreateNumberValue(*m_fallbackMidi1PortCount));
            }

            // The id and the hardware address go together, or the service could match an old
            // adapter's hardware address against the new id
            if (m_networkAdapterId.has_value())
            {
                hostObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_ID_KEY,
                    json::JsonValue::CreateStringValue(winrt::hstring{ ::WindowsMidiServicesInternal::MidiNetworkAdapterIdToString(*m_networkAdapterId) }));

                hostObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_PHYSICAL_ADDRESS_KEY,
                    json::JsonValue::CreateStringValue(m_networkAdapterPhysicalAddress));
            }

            if (m_networkAdapterName.has_value())
            {
                hostObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_ADAPTER_NAME_KEY,
                    json::JsonValue::CreateStringValue(*m_networkAdapterName));
            }

            if (m_allowNetworkAdapterFallback.has_value())
            {
                hostObject.SetNamedValue(
                    MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOW_NETWORK_ADAPTER_FALLBACK_KEY,
                    json::JsonValue::CreateBooleanValue(*m_allowNetworkAdapterFallback));
            }

            json::JsonObject hostsContainer{};
            hostsContainer.SetNamedValue(
                winrt::to_hstring(HostId()),
                hostObject);

            // "hosts": { ... }
            json::JsonObject updateObject{};
            updateObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY,
                hostsContainer);

            // "update": { ... }
            json::JsonObject transportObject{};
            transportObject.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_UPDATE_ENTRIES_KEY,
                updateObject);

            // "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": { ... }
            json::JsonObject transportSettingsObject{};
            transportSettingsObject.SetNamedValue(
                internal::GuidToString(network::MidiNetworkTransportManager::TransportId()),
                transportObject);

            // "endpointTransportPluginSettings": { ... }
            json::JsonObject wrapperObject{};
            wrapperObject.SetNamedValue(
                MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT,
                transportSettingsObject);

            return wrapperObject;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            return json::JsonObject{};
        }
    }
}
