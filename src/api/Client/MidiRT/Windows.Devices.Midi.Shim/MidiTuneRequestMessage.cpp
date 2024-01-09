// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiTuneRequestMessage.h"
#include "MidiTuneRequestMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    winrt::Windows::Foundation::TimeSpan MidiTuneRequestMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiTuneRequestMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiTuneRequestMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
