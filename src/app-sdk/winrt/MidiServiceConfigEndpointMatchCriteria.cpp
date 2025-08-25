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


    _Use_decl_annotations_
    bool MidiServiceConfigEndpointMatchCriteria::Matches(midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria const& other) const noexcept
    {
        auto otherMatch = winrt::get_self<MidiServiceConfigEndpointMatchCriteria>(other)->InternalGetMatchObject();

        return m_match->Matches(*otherMatch);
    }


    _Use_decl_annotations_
    midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria MidiServiceConfigEndpointMatchCriteria::FromJson(_In_ winrt::hstring const& matchObjectJson) noexcept
    {
        auto winrtMatch = winrt::make_self<MidiServiceConfigEndpointMatchCriteria>();

        json::JsonObject jsonObject;

        if (json::JsonObject::TryParse(matchObjectJson, jsonObject))
        {
            auto match = MidiEndpointMatchCriteria::FromJson(jsonObject);

            winrtMatch->InternalSetMatchObject(match);

            return *winrtMatch;
        }

        return nullptr;
    }


}
