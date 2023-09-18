// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#pragma once
#include "MidiSessionSettings.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSessionSettings : MidiSessionSettingsT<MidiSessionSettings>
    {
        MidiSessionSettings() = default;

        bool UseMmcssThreads() const { return m_useMmcss; }
        void UseMmcssThreads(_In_ bool const value);


    private:
        bool m_useMmcss{ true };
    };
}

namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiSessionSettings : MidiSessionSettingsT<MidiSessionSettings, implementation::MidiSessionSettings>
    {
    };
}