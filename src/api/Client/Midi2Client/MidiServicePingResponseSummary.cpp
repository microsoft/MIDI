// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiServicePingResponseSummary.h"
#include "MidiServicePingResponseSummary.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    uint64_t MidiServicePingResponseSummary::TotalPingRoundTripMidiClock()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiServicePingResponse> MidiServicePingResponseSummary::Responses()
    {
        throw hresult_not_implemented();
    }
}
