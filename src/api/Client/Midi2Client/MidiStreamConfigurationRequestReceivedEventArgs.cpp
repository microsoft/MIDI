// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiStreamConfigurationRequestReceivedEventArgs.h"
#include "MidiStreamConfigurationRequestReceivedEventArgs.g.cpp"

namespace MIDI_CPP_NAMESPACE::implementation
{
    _Use_decl_annotations_
    void MidiStreamConfigurationRequestReceivedEventArgs::InternalInitialize(
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
