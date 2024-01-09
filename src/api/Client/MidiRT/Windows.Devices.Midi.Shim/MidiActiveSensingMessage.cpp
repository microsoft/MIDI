// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiActiveSensingMessage.h"
#include "MidiActiveSensingMessage.g.cpp"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    winrt::Windows::Foundation::TimeSpan MidiActiveSensingMessage::Timestamp()
    {
        throw hresult_not_implemented();
    }
    midi1::MidiMessageType MidiActiveSensingMessage::Type()
    {
        throw hresult_not_implemented();
    }
    streams::IBuffer MidiActiveSensingMessage::RawData()
    {
        throw hresult_not_implemented();
    }
}
