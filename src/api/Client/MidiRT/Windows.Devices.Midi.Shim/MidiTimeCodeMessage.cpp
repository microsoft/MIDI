// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiTimeCodeMessage.h"
#include "MidiTimeCodeMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiTimeCodeMessage::MidiTimeCodeMessage(uint8_t /*frameType*/, uint8_t /*values*/)
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiTimeCodeMessage::FrameType()
    {
        throw hresult_not_implemented();
    }
    uint8_t MidiTimeCodeMessage::Values()
    {
        throw hresult_not_implemented();
    }
    foundation::TimeSpan MidiTimeCodeMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiTimeCodeMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiTimeCodeMessage::RawData()
    {
        throw hresult_not_implemented();
    }

}
