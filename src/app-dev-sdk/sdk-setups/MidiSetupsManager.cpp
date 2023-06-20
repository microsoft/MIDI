// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiSetupsManager.h"
#include "MidiSetupsManager.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::Setups::implementation
{
    winrt::Microsoft::Devices::Midi2::Setups::MidiSetup MidiSetupsManager::Current()
    {
        throw hresult_not_implemented();
    }
    void MidiSetupsManager::SwitchTo(winrt::Microsoft::Devices::Midi2::Setups::MidiSetup const& setup)
    {
        throw hresult_not_implemented();
    }
    void MidiSetupsManager::AddNew(winrt::Microsoft::Devices::Midi2::Setups::MidiSetup const& setup)
    {
        throw hresult_not_implemented();
    }
    void MidiSetupsManager::Save(winrt::Microsoft::Devices::Midi2::Setups::MidiSetup const& setup)
    {
        throw hresult_not_implemented();
    }
    void MidiSetupsManager::Delete(hstring const& setupId)
    {
        throw hresult_not_implemented();
    }
}
