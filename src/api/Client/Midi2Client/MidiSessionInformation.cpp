#include "pch.h"
#include "MidiSessionInformation.h"
#include "MidiSessionInformation.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiSessionInformation::InternalInitialize(
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
    void MidiSessionInformation::InternalAddConnection(
        midi2::MidiSessionConnectionInformation const& info
    )
    {
        m_connections.Append(info);
    }

}
