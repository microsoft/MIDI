// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiSetupsManager.g.h"


namespace winrt::Microsoft::Devices::Midi2::Setups::implementation
{
    struct MidiSetupsManager : MidiSetupsManagerT<MidiSetupsManager>
    {
        MidiSetupsManager() = default;

        winrt::Microsoft::Devices::Midi2::Setups::MidiSetup Current();
        void SwitchTo(winrt::Microsoft::Devices::Midi2::Setups::MidiSetup const& setup);
        void AddNew(winrt::Microsoft::Devices::Midi2::Setups::MidiSetup const& setup);
        void Save(winrt::Microsoft::Devices::Midi2::Setups::MidiSetup const& setup);
        void Delete(hstring const& setupId);
    };
}
namespace winrt::Microsoft::Devices::Midi2::Setups::factory_implementation
{
    struct MidiSetupsManager : MidiSetupsManagerT<MidiSetupsManager, implementation::MidiSetupsManager>
    {
    };
}
