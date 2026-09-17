// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Synth.MidiSynthStatus.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    struct MidiSynthStatus : MidiSynthStatusT<MidiSynthStatus>
    {
        MidiSynthStatus() = default;

        bool IsEnabled() const noexcept { return m_isEnabled; }
        synth::MidiSynthRenderMode RenderMode() const noexcept { return m_renderMode; }
        synth::MidiSynthAudioOutputMode AudioOutputMode() const noexcept { return m_audioOutputMode; }
        synth::MidiSynthBankSelectMode BankSelectMode() const noexcept { return m_bankSelectMode; }
        double VolumeDecibels() const noexcept { return m_volumeDecibels; }
        bool AreEffectsEnabled() const noexcept { return m_areEffectsEnabled; }

        void InternalInitializeFromJson(_In_ json::JsonObject const& responseJson);

    private:
        bool m_isEnabled{ false };
        synth::MidiSynthRenderMode m_renderMode{ synth::MidiSynthRenderMode::Modern };
        synth::MidiSynthAudioOutputMode m_audioOutputMode{ synth::MidiSynthAudioOutputMode::WasapiShared };
        synth::MidiSynthBankSelectMode m_bankSelectMode{ synth::MidiSynthBankSelectMode::Automatic };
        double m_volumeDecibels{ 0.0 };
        bool m_areEffectsEnabled{ true };
    };
}
