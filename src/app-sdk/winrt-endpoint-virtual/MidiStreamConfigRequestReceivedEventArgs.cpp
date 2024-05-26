// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiStreamConfigRequestReceivedEventArgs.h"
#include "MidiStreamConfigRequestReceivedEventArgs.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual::implementation
{
    _Use_decl_annotations_
    void MidiStreamConfigRequestReceivedEventArgs::InternalInitialize(
        internal::MidiTimestamp timestamp,
        midi2::MidiProtocol protocol,
        bool requestReceiveJRTimestamps,
        bool requestTransmitJRTimestamps
        )
    {
        m_timestamp = timestamp;
        m_protocol = protocol;
        m_requestReceiveJRTimestamps = requestReceiveJRTimestamps;
        m_requestTransmitJRTimestamps = requestTransmitJRTimestamps;
    }

}
