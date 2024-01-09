// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiTimingClockMessage.h"
#include "MidiTimingClockMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    foundation::TimeSpan MidiTimingClockMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiTimingClockMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiTimingClockMessage::RawData()
    {
        throw hresult_not_implemented();
    }

}
