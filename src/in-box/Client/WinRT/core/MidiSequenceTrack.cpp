// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSequenceTrack.h"
#include "Utilities.Sequencing.MidiSequenceTrack.g.cpp"

#include "wstring_util.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    _Use_decl_annotations_
    void MidiSequenceTrack::InternalInitialize(uint16_t const trackIndex, ::midifile::Track const& track)
    {
        m_trackIndex = trackIndex;

        m_name = winrt::hstring{ ::WindowsMidiServicesInternal::WStringFromUtf8(track.Name) };
        m_instrumentName = winrt::hstring{ ::WindowsMidiServicesInternal::WStringFromUtf8(track.InstrumentName) };
        m_suggestedDeviceName = winrt::hstring{ ::WindowsMidiServicesInternal::WStringFromUtf8(track.DeviceName) };

        m_usedChannelMask = track.ChannelMask;
        m_noteCount = track.NoteCount;
        m_eventCount = track.EventCount;
        m_lastTick = track.LastTick;
    }
}
