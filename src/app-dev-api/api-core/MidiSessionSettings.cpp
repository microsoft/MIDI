// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#include "pch.h"
#include "MidiSessionSettings.h"
#include "MidiSessionSettings.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiSessionSettings MidiSessionSettings::Default()
    {
        // right now, the default is just empty

        return *(winrt::make_self<implementation::MidiSessionSettings>());
    }

    bool MidiSessionSettings::UseMmcssThreads()
    {
        return m_useMmcss;
    }
    void MidiSessionSettings::UseMmcssThreads(bool value)
    {
        m_useMmcss = value;
    }

}
