// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiInteropHelper.h"
#include "MidiInteropHelper.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp> MidiInteropHelper::CreateEmptySingleThreadedUmpWithTimestampList()
    {
        return winrt::single_threaded_vector< winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp>();
    }

    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp> MidiInteropHelper::CreateEmptyMultiThreadedUmpWithTimestampList()
    {
        return winrt::multi_threaded_vector< winrt::Microsoft::Devices::Midi2::MidiUmpWithTimestamp>();
    }


}
