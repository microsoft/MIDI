// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

enum class MidiSynthAudioMode
{
    // Opens the endpoint with the mix format it already reports, so no device setting is changed
    // and every other application keeps playing.
    WasapiShared = 0,

    // Same, but asks IAudioClient3 for the smallest period the engine will allow.
    WasapiSharedLowLatency,

    // Not implemented yet. Both take the device away from everything else on the machine, which
    // needs a user facing decision before it can be offered.
    //
    // Asio is deliberately absent from the public API: it also needs a device selection, since
    // there is no default ASIO device, and the SDK and branding that come with it. The hook is
    // kept here so the work can return without reshaping the settings.
    WasapiExclusive,
    Asio,
};

struct MidiSynthSettings
{
    MidiSynth::SynthMode SynthMode{ MidiSynth::SynthMode::Modern };
    MidiSynthAudioMode AudioMode{ MidiSynthAudioMode::WasapiShared };

    MidiSynth::BankSelectMode BankSelect{ MidiSynth::BankSelectMode::Automatic };

    // The customer's own trim. The synthesizer has no slider in the Windows Volume Mixer, because
    // the service renders from session 0, so this is the only volume control it has.
    double VolumeDecibels{ 0.0 };

    bool EffectsEnabled{ true };

    // A disabled synthesizer has no endpoint at all, so an application which opens every MIDI port
    // it can find cannot hold the audio device away from an application that wants it in WASAPI
    // exclusive mode, or through ASIO.
    bool Enabled{ true };

    bool AudioModeIsImplemented() const noexcept
    {
        return AudioMode == MidiSynthAudioMode::WasapiShared
            || AudioMode == MidiSynthAudioMode::WasapiSharedLowLatency;
    }

    bool WantsLowLatency() const noexcept
    {
        return AudioMode == MidiSynthAudioMode::WasapiSharedLowLatency;
    }

    static PCWSTR SynthModeToString(_In_ MidiSynth::SynthMode const mode) noexcept
    {
        return mode == MidiSynth::SynthMode::Compatible
            ? MIDI_SYNTH_JSON_SYNTH_MODE_COMPATIBLE
            : MIDI_SYNTH_JSON_SYNTH_MODE_MODERN;
    }

    static PCWSTR AudioModeToString(_In_ MidiSynthAudioMode const mode) noexcept
    {
        switch (mode)
        {
        case MidiSynthAudioMode::WasapiSharedLowLatency: return MIDI_SYNTH_JSON_AUDIO_MODE_SHARED_LOW_LATENCY;
        case MidiSynthAudioMode::WasapiExclusive:        return MIDI_SYNTH_JSON_AUDIO_MODE_EXCLUSIVE;
        case MidiSynthAudioMode::Asio:                   return MIDI_SYNTH_JSON_AUDIO_MODE_ASIO;
        default:                                         return MIDI_SYNTH_JSON_AUDIO_MODE_SHARED;
        }
    }

    static bool TryParseSynthMode(_In_ std::wstring const& text, _Out_ MidiSynth::SynthMode& mode) noexcept
    {
        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_SYNTH_MODE_COMPATIBLE) == 0)
        {
            mode = MidiSynth::SynthMode::Compatible;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_SYNTH_MODE_MODERN) == 0)
        {
            mode = MidiSynth::SynthMode::Modern;
            return true;
        }

        return false;
    }

    static bool TryParseAudioMode(_In_ std::wstring const& text, _Out_ MidiSynthAudioMode& mode) noexcept
    {        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_AUDIO_MODE_SHARED) == 0)
        {
            mode = MidiSynthAudioMode::WasapiShared;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_AUDIO_MODE_SHARED_LOW_LATENCY) == 0)
        {
            mode = MidiSynthAudioMode::WasapiSharedLowLatency;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_AUDIO_MODE_EXCLUSIVE) == 0)
        {
            mode = MidiSynthAudioMode::WasapiExclusive;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_AUDIO_MODE_ASIO) == 0)
        {
            mode = MidiSynthAudioMode::Asio;
            return true;
        }

        return false;
    }

    static PCWSTR BankSelectModeToString(_In_ MidiSynth::BankSelectMode const mode) noexcept
    {
        switch (mode)
        {
        case MidiSynth::BankSelectMode::RolandGS:     return MIDI_SYNTH_JSON_BANK_SELECT_GS;
        case MidiSynth::BankSelectMode::YamahaXG:     return MIDI_SYNTH_JSON_BANK_SELECT_XG;
        case MidiSynth::BankSelectMode::GeneralMidi2: return MIDI_SYNTH_JSON_BANK_SELECT_GM2;
        default:                                      return MIDI_SYNTH_JSON_BANK_SELECT_AUTOMATIC;
        }
    }

    static bool TryParseBankSelectMode(
        _In_ std::wstring const& text,
        _Out_ MidiSynth::BankSelectMode& mode) noexcept
    {
        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_BANK_SELECT_GS) == 0)
        {
            mode = MidiSynth::BankSelectMode::RolandGS;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_BANK_SELECT_XG) == 0)
        {
            mode = MidiSynth::BankSelectMode::YamahaXG;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_BANK_SELECT_GM2) == 0)
        {
            mode = MidiSynth::BankSelectMode::GeneralMidi2;
            return true;
        }

        if (_wcsicmp(text.c_str(), MIDI_SYNTH_JSON_BANK_SELECT_AUTOMATIC) == 0)
        {
            mode = MidiSynth::BankSelectMode::Automatic;
            return true;
        }

        return false;
    }
};
