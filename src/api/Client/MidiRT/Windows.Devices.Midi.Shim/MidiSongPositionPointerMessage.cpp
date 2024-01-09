// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSongPositionPointerMessage.h"
#include "MidiSongPositionPointerMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiSongPositionPointerMessage::MidiSongPositionPointerMessage(uint16_t /*beats*/)
    {
        throw hresult_not_implemented();
    }
    uint16_t MidiSongPositionPointerMessage::Beats()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::TimeSpan MidiSongPositionPointerMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiSongPositionPointerMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiSongPositionPointerMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
