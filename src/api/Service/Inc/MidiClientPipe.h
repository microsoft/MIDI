// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiPipe.h"

class CMidiClientPipe : public CMidiPipe
{
public:
    HRESULT Initialize(   _In_ handle_t BindingHandle,
                            _In_ HANDLE,
                            _In_ LPCWSTR,
                            _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                            _In_ PMIDISRV_CLIENT,
                            _In_ DWORD *);
    HRESULT Cleanup();

    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);
    HRESULT SendMidiMessageNow(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    // client pipe must have the same format for both in and out, so
    // setting the format for one or the other sets for both.
    virtual HRESULT SetDataFormatIn(MidiDataFormat DataFormat)
    {
        RETURN_IF_FAILED(CMidiPipe::SetDataFormatIn(DataFormat));
        if (CMidiPipe::DataFormatOut() != MidiDataFormat_Invalid)
        {
            RETURN_IF_FAILED(CMidiPipe::SetDataFormatOut(DataFormat));
        }
        return S_OK;
    }

    virtual HRESULT SetDataFormatOut(MidiDataFormat DataFormat)
    {
        RETURN_IF_FAILED(CMidiPipe::SetDataFormatOut(DataFormat));
        if (CMidiPipe::DataFormatIn() != MidiDataFormat_Invalid)
        {
            RETURN_IF_FAILED(CMidiPipe::SetDataFormatIn(DataFormat));
        }
        return S_OK;
    }

private:
    HRESULT AdjustForBufferingRequirements(_In_ PMIDISRV_CLIENTCREATION_PARAMS CreationParams);

    wil::critical_section m_ClientPipeLock;
    MidiClientHandle m_ClientHandle{ 0 };
    std::unique_ptr<CMidiXProc> m_MidiPump;
};

