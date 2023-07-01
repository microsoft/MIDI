// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiInteropHelper.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiInteropHelper : MidiInteropHelperT<MidiInteropHelper>
    {
        MidiInteropHelper() = default;

        static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp> CreateEmptySingleThreadedUmpWithTimestampList();
        static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp> CreateEmptyMultiThreadedUmpWithTimestampList();
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiInteropHelper : MidiInteropHelperT<MidiInteropHelper, implementation::MidiInteropHelper>
    {
    };
}
