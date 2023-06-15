// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiDiagnostics.h"
#include "MidiDiagnostics.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Windows::Storage::StorageFile MidiDiagnostics::DumpMidiServicesStateToFile(winrt::Windows::Storage::StorageFolder const& preferredFolder)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::StorageFile MidiDiagnostics::DumpMidiServicesStateToFile()
    {
        throw hresult_not_implemented();
    }
}
