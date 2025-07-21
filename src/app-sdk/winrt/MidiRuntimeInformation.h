// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Utilities.RuntimeInformation.MidiRuntimeInformation.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::implementation
{
    struct MidiRuntimeInformation
    {
        MidiRuntimeInformation() = default;

        static winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::MidiRuntimeVersion GetInstalledVersion();
        static winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture GetInstalledArchitecture();
        static winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes GetInstalledReleaseType();
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::factory_implementation
{
    struct MidiRuntimeInformation : MidiRuntimeInformationT<MidiRuntimeInformation, implementation::MidiRuntimeInformation>
    {
    };
}
