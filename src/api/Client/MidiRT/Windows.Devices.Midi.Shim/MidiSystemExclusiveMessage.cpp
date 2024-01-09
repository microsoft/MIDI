// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveMessage.h"
#include "MidiSystemExclusiveMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiSystemExclusiveMessage::MidiSystemExclusiveMessage(streams::IBuffer const& /*rawData*/)
    {
        throw hresult_not_implemented();
    }
    foundation::TimeSpan MidiSystemExclusiveMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiSystemExclusiveMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiSystemExclusiveMessage::RawData()
    {
        throw hresult_not_implemented();
    }

}
