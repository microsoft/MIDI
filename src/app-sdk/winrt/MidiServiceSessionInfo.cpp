// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiServiceSessionInfo.h"
#include "Reporting.MidiServiceSessionInfo.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Reporting::implementation
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
        rept::MidiServiceSessionConnectionInfo const& info
    )
    {
        m_connections.Append(info);
    }

}
