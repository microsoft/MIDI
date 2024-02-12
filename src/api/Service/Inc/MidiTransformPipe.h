// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiPipe.h"

typedef struct MIDISRV_TRANSFORMCREATION_PARAMS
{
    GUID TransformGuid;
    MidiDataFormat DataFormatIn;
    MidiDataFormat DataFormatOut;
    MidiFlow Flow;
} MIDISRV_TRANSFORMCREATION_PARAMS, *PMIDISRV_TRANSFORMCREATION_PARAMS;

class CMidiTransformPipe : public CMidiPipe
{
public:
    
    HRESULT Initialize(_In_ handle_t,
                            _In_ LPCWSTR, // device it's associated to
                            _In_ PMIDISRV_TRANSFORMCREATION_PARAMS, // what transform to create
                            _In_ DWORD *, // mmcss
                            _In_ IUnknown* // MidiDeviceManager to provide to transforms that need to update device properties
    ); 
    HRESULT Cleanup();

    HRESULT SendMidiMessage(_In_ PVOID, _In_ UINT, _In_ LONGLONG);
    HRESULT SendMidiMessageNow(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    GUID TransformGuid();

private:
    wil::com_ptr_nothrow<IMidiDataTransform> m_MidiDataTransform;
    winrt::guid m_TransformGuid{};
    MidiDataFormat m_DataFormatIn{};
    MidiDataFormat m_DataFormatOut{};
    MidiFlow m_Flow{};
};

