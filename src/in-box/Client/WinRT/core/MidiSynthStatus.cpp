// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSynthStatus.h"
#include "Transports.Synth.MidiSynthStatus.g.cpp"

#include "..\..\..\Transport\MidiSynthTransport\midi_synth_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    _Use_decl_annotations_
    void MidiSynthStatus::InternalInitializeFromJson(json::JsonObject const& responseJson)
    {
        if (responseJson == nullptr)
        {
            return;
        }

        m_isEnabled = responseJson.GetNamedBoolean(MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY, false);
        m_areEffectsEnabled = responseJson.GetNamedBoolean(MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY, true);
        m_volumeDecibels = responseJson.GetNamedNumber(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY, 0.0);

        auto const renderMode = responseJson.GetNamedString(MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY, L"");

        m_renderMode = (renderMode == MIDI_SYNTH_JSON_SYNTH_MODE_COMPATIBLE)
            ? synth::MidiSynthRenderMode::Compatible
            : synth::MidiSynthRenderMode::Modern;

        auto const audioMode = responseJson.GetNamedString(MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY, L"");

        if (audioMode == MIDI_SYNTH_JSON_AUDIO_MODE_SHARED_LOW_LATENCY)
        {
            m_audioOutputMode = synth::MidiSynthAudioOutputMode::WasapiSharedLowLatency;
        }
        else if (audioMode == MIDI_SYNTH_JSON_AUDIO_MODE_EXCLUSIVE)
        {
            m_audioOutputMode = synth::MidiSynthAudioOutputMode::WasapiExclusive;
        }
        else if (audioMode == MIDI_SYNTH_JSON_AUDIO_MODE_ASIO)
        {
            m_audioOutputMode = synth::MidiSynthAudioOutputMode::Asio;
        }
        else
        {
            m_audioOutputMode = synth::MidiSynthAudioOutputMode::WasapiShared;
        }

        auto const bankSelect = responseJson.GetNamedString(MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY, L"");

        if (bankSelect == MIDI_SYNTH_JSON_BANK_SELECT_GS)
        {
            m_bankSelectMode = synth::MidiSynthBankSelectMode::RolandGS;
        }
        else if (bankSelect == MIDI_SYNTH_JSON_BANK_SELECT_XG)
        {
            m_bankSelectMode = synth::MidiSynthBankSelectMode::YamahaXG;
        }
        else if (bankSelect == MIDI_SYNTH_JSON_BANK_SELECT_GM2)
        {
            m_bankSelectMode = synth::MidiSynthBankSelectMode::GeneralMidi2;
        }
        else
        {
            m_bankSelectMode = synth::MidiSynthBankSelectMode::Automatic;
        }
    }
}
