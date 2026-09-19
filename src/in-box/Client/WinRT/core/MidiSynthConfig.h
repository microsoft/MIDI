// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Synth.MidiSynthConfig.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    struct MidiSynthConfig : MidiSynthConfigT<MidiSynthConfig>
    {
        MidiSynthConfig() = default;
        MidiSynthConfig(_In_ synth::MidiSynthStatus const& currentStatus);

        winrt::guid TransportId() const noexcept;

        json::JsonObject ConfigJson();

        bool IsEnabled() const noexcept { return m_isEnabled; }
        void IsEnabled(_In_ bool const value) noexcept { m_isEnabled = value; }

        synth::MidiSynthRenderMode RenderMode() const noexcept { return m_renderMode; }
        void RenderMode(_In_ synth::MidiSynthRenderMode const value) noexcept { m_renderMode = value; }

        synth::MidiSynthAudioOutputMode AudioOutputMode() const noexcept { return m_audioOutputMode; }
        void AudioOutputMode(_In_ synth::MidiSynthAudioOutputMode const value) noexcept { m_audioOutputMode = value; }

        synth::MidiSynthBankSelectMode BankSelectMode() const noexcept { return m_bankSelectMode; }
        void BankSelectMode(_In_ synth::MidiSynthBankSelectMode const value) noexcept { m_bankSelectMode = value; }

        double VolumeDecibels() const noexcept { return m_volumeDecibels; }
        void VolumeDecibels(_In_ double const value) noexcept { m_volumeDecibels = value; }

        bool AreEffectsEnabled() const noexcept { return m_areEffectsEnabled; }
        void AreEffectsEnabled(_In_ bool const value) noexcept { m_areEffectsEnabled = value; }

    private:
        // Defaults match what the transport starts with, so a default-constructed config sends the
        // shipped settings rather than something arbitrary.
        bool m_isEnabled{ true };
        synth::MidiSynthRenderMode m_renderMode{ synth::MidiSynthRenderMode::Modern };
        synth::MidiSynthAudioOutputMode m_audioOutputMode{ synth::MidiSynthAudioOutputMode::WasapiShared };
        synth::MidiSynthBankSelectMode m_bankSelectMode{ synth::MidiSynthBankSelectMode::Automatic };
        double m_volumeDecibels{ 0.0 };
        bool m_areEffectsEnabled{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Synth::factory_implementation
{
    struct MidiSynthConfig : MidiSynthConfigT<MidiSynthConfig, implementation::MidiSynthConfig>
    {
    };
}
