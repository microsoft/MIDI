// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiStopMessage.h"
#include "MidiStopMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    foundation::TimeSpan MidiStopMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiStopMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiStopMessage::RawData()
    {
        throw hresult_not_implemented();
    }

}
