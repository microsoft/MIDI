// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkClientMatchCriteria.h"
#include "Endpoints.Network.MidiNetworkClientMatchCriteria.g.cpp"

#include "..\..\..\api\Transport\UdpNetworkMidi2Transport\network_json_defs.h"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{



	winrt::hstring MidiNetworkClientMatchCriteria::GetConfigJson() const noexcept
	{
		json::JsonObject matchObject{};

		if (!DeviceId().empty())
		{
			matchObject.SetNamedValue(
				MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID_KEY,
				json::JsonValue::CreateStringValue(DeviceId()));

		}
		else if (!DirectHostNameOrIPAddress().empty())
		{
			matchObject.SetNamedValue(
				MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME_OR_IP_ADDRESS_KEY,
				json::JsonValue::CreateStringValue(DirectHostNameOrIPAddress()));

			matchObject.SetNamedValue(
				MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT_KEY,
				json::JsonValue::CreateStringValue(winrt::to_hstring(DirectPort())));
		}

		return matchObject.Stringify();
	}

}
