// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2JitterReductionListenerTransform::Activate(
    REFIID Iid,
    void **Interface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiDataTransform) == Iid)
    {
        TraceLoggingWrite(
            MidiJitterReductionListenerTransformTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi Transform",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
        );

        wil::com_ptr_nothrow<IMidiDataTransform> midiTransform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2JitterReductionListenerMidiTransform>(&midiTransform));
        *Interface = midiTransform.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

