// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSynthSoundSetInfo.h"
#include "Transports.Synth.MidiSynthSoundSetInfo.g.cpp"

#include "MidiSynthDrumKitInfo.h"
#include "..\..\..\Transport\MidiSynthTransport\midi_synth_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    _Use_decl_annotations_
    void MidiSynthSoundSetInfo::InternalInitialize(json::JsonObject const& responseJson)
    {
        if (responseJson == nullptr)
        {
            return;
        }

        m_name = responseJson.GetNamedString(MIDI_SYNTH_JSON_SOUND_SET_NAME_KEY, L"");
        m_version = responseJson.GetNamedString(MIDI_SYNTH_JSON_SOUND_SET_VERSION_KEY, L"");
        m_filePath = responseJson.GetNamedString(MIDI_SYNTH_JSON_SOUND_SET_PATH_KEY, L"");

        m_melodicInstrumentCount = static_cast<uint32_t>(
            responseJson.GetNamedNumber(MIDI_SYNTH_JSON_SOUND_SET_MELODIC_KEY, 0.0));

        m_waveCount = static_cast<uint32_t>(
            responseJson.GetNamedNumber(MIDI_SYNTH_JSON_SOUND_SET_WAVES_KEY, 0.0));

        auto const kits = responseJson.GetNamedArray(MIDI_SYNTH_JSON_SOUND_SET_KITS_KEY, nullptr);

        if (kits == nullptr)
        {
            return;
        }

        for (auto const& entry : kits)
        {
            // try_as is not a substitute here: it returns null for an element of a JsonArray.
            if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
            {
                continue;
            }

            auto const kitJson = entry.GetObject();

            auto kit = winrt::make_self<implementation::MidiSynthDrumKitInfo>();

            kit->InternalSet(
                kitJson.GetNamedString(MIDI_SYNTH_JSON_SOUND_SET_KIT_NAME_KEY, L""),
                static_cast<uint8_t>(kitJson.GetNamedNumber(MIDI_SYNTH_JSON_SOUND_SET_KIT_PROGRAM_KEY, 0.0)));

            m_drumKits.Append(*kit);
        }
    }
}
