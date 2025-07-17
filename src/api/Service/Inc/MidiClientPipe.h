// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiPipe.h"

class CMidiClientPipe : public CMidiPipe
{
public:
    HRESULT Initialize(   _In_ HANDLE,
                            _In_ LPCWSTR,
                            _In_ BYTE,
                            _In_ GUID,
                            _In_ DWORD,
                            _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                            _In_ PMIDISRV_CLIENT,
                            _In_ DWORD *,
                            _In_ BOOL);
    HRESULT Shutdown();

    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);
    HRESULT SendMidiMessageNow(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    // client pipe must have the same format for both in and out, so
    // setting the format for one or the other sets for both.
    virtual HRESULT SetDataFormatIn(MidiDataFormats DataFormat)
    {
        RETURN_IF_FAILED(CMidiPipe::SetDataFormatIn(DataFormat));
        if (CMidiPipe::DataFormatOut() != MidiDataFormats_Invalid)
        {
            RETURN_IF_FAILED(CMidiPipe::SetDataFormatOut(DataFormat));
        }

        if (m_MidiPump)
        {
            // Set the format(s) on the midi pump
            RETURN_IF_FAILED(m_MidiPump->SetDataFormatIn(DataFormat));
            RETURN_IF_FAILED(m_MidiPump->SetDataFormatOut(DataFormat));
        }

        return S_OK;
    }

    virtual HRESULT SetDataFormatOut(MidiDataFormats DataFormat)
    {
        RETURN_IF_FAILED(CMidiPipe::SetDataFormatOut(DataFormat));
        if (CMidiPipe::DataFormatIn() != MidiDataFormats_Invalid)
        {
            RETURN_IF_FAILED(CMidiPipe::SetDataFormatIn(DataFormat));
        }

        if (m_MidiPump)
        {
            // Set the format(s) on the midi pump
            RETURN_IF_FAILED(m_MidiPump->SetDataFormatIn(DataFormat));
            RETURN_IF_FAILED(m_MidiPump->SetDataFormatOut(DataFormat));
        }
        return S_OK;
    }

    GUID SessionId() { return m_sessionId; }
    DWORD ClientProcessId() { return m_clientProcessId; }
    //LPCWSTR Device() { return m_device.c_str(); }

    virtual BOOL IsGroupFiltered() { return m_GroupFiltered; }
    virtual BYTE GroupIndex() { return m_GroupIndex; }

private:
    HRESULT AdjustForBufferingRequirements(_In_ PMIDISRV_CLIENTCREATION_PARAMS CreationParams);

    wil::critical_section m_ClientPipeLock;
    MidiClientHandle m_ClientHandle{ 0 };
    std::unique_ptr<CMidiXProc> m_MidiPump;

    GUID m_sessionId{};         // client session id for tracking
    DWORD m_clientProcessId{};  // also for session tracking

    BOOL m_GroupFiltered{FALSE};
    BYTE m_GroupIndex{0};
};

