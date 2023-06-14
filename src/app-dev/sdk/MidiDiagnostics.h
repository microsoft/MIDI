// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiDiagnostics.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiDiagnostics : MidiDiagnosticsT<MidiDiagnostics>
    {
        MidiDiagnostics() = default;

        static winrt::Windows::Storage::StorageFile DumpMidiServicesStateToFile(winrt::Windows::Storage::StorageFolder const& preferredFolder);
        static winrt::Windows::Storage::StorageFile DumpMidiServicesStateToFile();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiDiagnostics : MidiDiagnosticsT<MidiDiagnostics, implementation::MidiDiagnostics>
    {
    };
}
