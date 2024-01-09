// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiStartMessage.h"
#include "MidiStartMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    winrt::Windows::Foundation::TimeSpan MidiStartMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageType MidiStartMessage::Type()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Storage::Streams::IBuffer MidiStartMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
