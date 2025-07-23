// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiPipe.h"

typedef struct MIDISRV_TRANSFORMCREATION_PARAMS
{
    GUID TransformGuid;
    MidiDataFormats DataFormatIn;
    MidiDataFormats DataFormatOut;
    MidiFlow Flow;
    BYTE UmpGroupIndex;
} MIDISRV_TRANSFORMCREATION_PARAMS, *PMIDISRV_TRANSFORMCREATION_PARAMS;

class CMidiTransformPipe : public CMidiPipe
{
public:
    
    HRESULT Initialize(_In_ LPCWSTR, // device it's associated to
                            _In_ PMIDISRV_TRANSFORMCREATION_PARAMS, // what transform to create
                            _In_ DWORD *, // mmcss
                            _In_ IMidiDeviceManager* // MidiDeviceManager to provide to transforms that need to update device properties
    ); 
    HRESULT Shutdown();

    HRESULT SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID, _In_ UINT, _In_ LONGLONG);

    GUID TransformGuid();

private:
    wil::com_ptr_nothrow<IMidiDataTransform> m_MidiDataTransform;
    winrt::guid m_TransformGuid{};
};

