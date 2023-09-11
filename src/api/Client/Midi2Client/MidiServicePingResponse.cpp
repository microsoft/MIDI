// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiServicePingResponse.h"
#include "MidiServicePingResponse.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    uint64_t MidiServicePingResponse::Id()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiServicePingResponse::SendMidiTimestamp()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiServicePingResponse::ReceiveMidiTimestamp()
    {
        throw hresult_not_implemented();
    }
    uint64_t MidiServicePingResponse::DeltaTimestamp()
    {
        throw hresult_not_implemented();
    }
}
