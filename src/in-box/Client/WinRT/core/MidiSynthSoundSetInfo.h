// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Synth.MidiSynthSoundSetInfo.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    struct MidiSynthSoundSetInfo : MidiSynthSoundSetInfoT<MidiSynthSoundSetInfo>
    {
        MidiSynthSoundSetInfo() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Version() const noexcept { return m_version; }
        winrt::hstring FilePath() const noexcept { return m_filePath; }

        uint32_t MelodicInstrumentCount() const noexcept { return m_melodicInstrumentCount; }
        uint32_t WaveCount() const noexcept { return m_waveCount; }

        foundation::Collections::IVectorView<synth::MidiSynthDrumKitInfo> DrumKits() const noexcept
        {
            return m_drumKits.GetView();
        }

        void InternalInitialize(_In_ json::JsonObject const& responseJson);

    private:
        winrt::hstring m_name{};
        winrt::hstring m_version{};
        winrt::hstring m_filePath{};

        uint32_t m_melodicInstrumentCount{ 0 };
        uint32_t m_waveCount{ 0 };

        foundation::Collections::IVector<synth::MidiSynthDrumKitInfo> m_drumKits{
            winrt::single_threaded_vector<synth::MidiSynthDrumKitInfo>() };
    };
}
