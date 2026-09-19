// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSynthConfig.h"
#include "Transports.Synth.MidiSynthConfig.g.cpp"

#include "MidiSynthManager.h"
#include "MidiSynthStatus.h"
#include "..\..\..\Transport\MidiSynthTransport\midi_synth_json_defs.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    _Use_decl_annotations_
    MidiSynthConfig::MidiSynthConfig(synth::MidiSynthStatus const& currentStatus)
    {
        // A null status means the transport did not answer. Leaving the defaults in place is
        // better than refusing to construct, and the caller still has to set what it wants.
        if (currentStatus == nullptr)
        {
            return;
        }

        m_isEnabled = currentStatus.IsEnabled();
        m_renderMode = currentStatus.RenderMode();
        m_audioOutputMode = currentStatus.AudioOutputMode();
        m_bankSelectMode = currentStatus.BankSelectMode();
        m_volumeDecibels = currentStatus.VolumeDecibels();
        m_areEffectsEnabled = currentStatus.AreEffectsEnabled();
    }

    winrt::guid MidiSynthConfig::TransportId() const noexcept
    {
        return implementation::MidiSynthManager::TransportId();
    }

    json::JsonObject MidiSynthConfig::ConfigJson()
    {
        json::JsonObject config;

        config.SetNamedValue(MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY,
            json::JsonValue::CreateBooleanValue(m_isEnabled));

        config.SetNamedValue(MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY,
            json::JsonValue::CreateBooleanValue(m_areEffectsEnabled));

        config.SetNamedValue(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY,
            json::JsonValue::CreateNumberValue(m_volumeDecibels));

        config.SetNamedValue(MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY,
            json::JsonValue::CreateStringValue(
                m_renderMode == synth::MidiSynthRenderMode::Compatible
                    ? MIDI_SYNTH_JSON_SYNTH_MODE_COMPATIBLE
                    : MIDI_SYNTH_JSON_SYNTH_MODE_MODERN));

        {
            winrt::hstring text{ MIDI_SYNTH_JSON_AUDIO_MODE_SHARED };

            switch (m_audioOutputMode)
            {
            case synth::MidiSynthAudioOutputMode::WasapiSharedLowLatency:
                text = MIDI_SYNTH_JSON_AUDIO_MODE_SHARED_LOW_LATENCY;
                break;
            case synth::MidiSynthAudioOutputMode::WasapiExclusive:
                text = MIDI_SYNTH_JSON_AUDIO_MODE_EXCLUSIVE;
                break;
            default:
                break;
            }

            config.SetNamedValue(MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY,
                json::JsonValue::CreateStringValue(text));
        }

        {
            winrt::hstring text{ MIDI_SYNTH_JSON_BANK_SELECT_AUTOMATIC };

            switch (m_bankSelectMode)
            {
            case synth::MidiSynthBankSelectMode::RolandGS:
                text = MIDI_SYNTH_JSON_BANK_SELECT_GS;
                break;
            case synth::MidiSynthBankSelectMode::YamahaXG:
                text = MIDI_SYNTH_JSON_BANK_SELECT_XG;
                break;
            case synth::MidiSynthBankSelectMode::GeneralMidi2:
                text = MIDI_SYNTH_JSON_BANK_SELECT_GM2;
                break;
            default:
                break;
            }

            config.SetNamedValue(MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY,
                json::JsonValue::CreateStringValue(text));
        }

        return config;
    }
}
