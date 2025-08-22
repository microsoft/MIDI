// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceConfigEndpointMatchCriteria.h"
#include "ServiceConfig.MidiServiceConfigEndpointMatchCriteria.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    winrt::hstring MidiServiceConfigEndpointMatchCriteria::GetConfigJson() const noexcept
    {
        json::JsonObject matchObject;

        if (m_match->WriteJson(matchObject))
        {
            return matchObject.Stringify();
        }
        else
        {
            return L"";
        }

    }
}
