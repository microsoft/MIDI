// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSongSelectMessage.h"
#include "MidiSongSelectMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    _Use_decl_annotations_
    MidiSongSelectMessage::MidiSongSelectMessage(uint8_t /*song*/)
    {
        throw hresult_not_implemented();
    }


    // TODO: Need to implement this and any methods required to populate it
    streams::IBuffer MidiSongSelectMessage::RawData()
    {
        return nullptr;
    }
}
