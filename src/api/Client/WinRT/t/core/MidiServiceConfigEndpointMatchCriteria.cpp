// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceConfigEndpointMatchCriteria.h"
#include "ServiceConfig.MidiServiceConfigEndpointMatchCriteria.g.cpp"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    winrt::hstring MidiServiceConfigEndpointMatchCriteria::GetConfigJson() const noexcept
    {
        try
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error getting match criteria config json.");
            return L"";
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception getting match criteria config json.");
            return L"";
        }

    }


    _Use_decl_annotations_
    bool MidiServiceConfigEndpointMatchCriteria::Matches(midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria const& other) const noexcept
    {
        try
        {
        auto otherMatch = winrt::get_self<MidiServiceConfigEndpointMatchCriteria>(other)->InternalGetMatchObject();

        return m_match->Matches(*otherMatch);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error matching endpoint match criteria.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception matching endpoint match criteria.");
            return false;
        }
    }


    _Use_decl_annotations_
    midi2::ServiceConfig::MidiServiceConfigEndpointMatchCriteria MidiServiceConfigEndpointMatchCriteria::FromJson(_In_ winrt::hstring const& matchObjectJson) noexcept
    {
        try
        {
        auto winrtMatch = winrt::make_self<MidiServiceConfigEndpointMatchCriteria>();

        json::JsonObject jsonObject;

        if (json::JsonObject::TryParse(matchObjectJson, jsonObject))
        {
            auto match = WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria::FromJson(jsonObject);

            winrtMatch->InternalSetMatchObject(match);

            return *winrtMatch;
        }

        return nullptr;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error creating match criteria from json.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception creating match criteria from json.");
            return nullptr;
        }
    }


}
