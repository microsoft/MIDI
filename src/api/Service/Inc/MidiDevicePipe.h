// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiPipe.h"

typedef struct MIDISRV_DEVICECREATION_PARAMS
{
    MidiDataFormats DataFormat;
    MidiFlow Flow;
    ULONG BufferSize;
} MIDISRV_DEVICECREATION_PARAMS, *PMIDISRV_DEVICECREATION_PARAMS;

class CMidiDevicePipe : public CMidiPipe
{
public:
    
    HRESULT Initialize(_In_ LPCWSTR,
                       _In_ PMIDISRV_DEVICECREATION_PARAMS,
                       _In_ DWORD *);
    HRESULT Shutdown();

    // called by the client
    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    // called by the scheduler
    HRESULT SendMidiMessageNow(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

private:
    wil::critical_section m_DevicePipeLock;
    winrt::guid m_TransportGuid{};
    wil::com_ptr_nothrow<IMidiBiDi> m_MidiBiDiDevice;
    wil::com_ptr_nothrow<IMidiIn> m_MidiInDevice;
    wil::com_ptr_nothrow<IMidiOut> m_MidiOutDevice;

    // TODO: This needs to be read from the device properties
    bool m_endpointSupportsMulticlient{ true };

};

