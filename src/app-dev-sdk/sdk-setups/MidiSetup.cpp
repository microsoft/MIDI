// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiSetup.h"
#include "MidiSetup.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::Setups::implementation
{
    hstring MidiSetup::Id()
    {
        throw hresult_not_implemented();
    }
    void MidiSetup::Id(hstring const& value)
    {
        throw hresult_not_implemented();
    }
    hstring MidiSetup::FileName()
    {
        throw hresult_not_implemented();
    }
    void MidiSetup::FileName(hstring const& value)
    {
        throw hresult_not_implemented();
    }
}
