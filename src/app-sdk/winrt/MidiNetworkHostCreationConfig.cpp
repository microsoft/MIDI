// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkHostCreationConfig.h"
#include "Endpoints.Network.MidiNetworkHostCreationConfig.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{

    winrt::hstring MidiNetworkHostCreationConfig::GetConfigJson() const noexcept
    {
		// TEMP
		return L"";
    }
}
