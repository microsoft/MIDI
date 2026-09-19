// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Synth.MidiSynthManager.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    struct MidiSynthManager
    {
        MidiSynthManager() = default;

        static bool IsTransportAvailable() noexcept;

        static winrt::guid TransportId() noexcept { return m_transportId; }

        static synth::MidiSynthStatus GetStatus() noexcept;
        static synth::MidiSynthSoundSetInfo GetSoundSetInfo() noexcept;

        static foundation::Collections::IVectorView<synth::MidiSynthInstrumentInfo> GetMelodicInstruments() noexcept;

        static winrt::hstring EndpointDeviceId() noexcept;

        static bool SetDrumChannel(_In_ uint8_t const channelIndex, _In_ bool const isDrumChannel) noexcept;

    private:
        // Must match the CLSID the synthesizer transport is registered under.
        inline static const winrt::guid m_transportId{ 0x7605713e, 0xfea9, 0x409d,
            { 0xa9, 0x0f, 0xa8, 0x12, 0x33, 0x20, 0x0d, 0x0a } };
    };
}

namespace winrt::Windows::Devices::Midi2::Transports::Synth::factory_implementation
{
    struct MidiSynthManager : MidiSynthManagerT<MidiSynthManager, implementation::MidiSynthManager, winrt::static_lifetime>
    {
    };
}
