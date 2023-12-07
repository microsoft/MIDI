// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2SampleTransform::Activate(
    REFIID Iid,
    void **Interface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiDataTransform) == Iid)
    {
        TraceLoggingWrite(
            MidiSampleTransformTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi Transform",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiDataTransform> midiTransform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2SampleMidiTransform>(&midiTransform));
        *Interface = midiTransform.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

