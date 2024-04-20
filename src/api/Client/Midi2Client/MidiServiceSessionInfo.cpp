// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiServiceSessionInfo.h"
#include "MidiServiceSessionInfo.g.cpp"

namespace MIDI_CPP_NAMESPACE::implementation
{
    _Use_decl_annotations_
    void MidiServiceSessionInfo::InternalInitialize(
        winrt::guid sessionId,
        winrt::hstring sessionName,
        uint64_t processId,
        winrt::hstring processName,
        foundation::DateTime startTime)
    {
        m_sessionId = sessionId;
        m_processId = processId;
        m_processName = processName;
        m_sessionName = sessionName;
        m_startTime = startTime;
    }

    _Use_decl_annotations_
    void MidiServiceSessionInfo::InternalAddConnection(
        midi2::MidiServiceSessionConnectionInfo const& info
    )
    {
        m_connections.Append(info);
    }

}
