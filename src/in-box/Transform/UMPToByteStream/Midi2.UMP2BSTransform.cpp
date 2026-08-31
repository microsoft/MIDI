// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2UMP2BSTransform::Activate(
    REFIID iid,
    void **activatedInterface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == activatedInterface);

    if (__uuidof(IMidiDataTransform) == iid)
    {
        TraceLoggingWrite(
            MidiUMP2BSTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"IMidiDataTransform", MIDI_TRACE_EVENT_INTERFACE_FIELD)

            );

        wil::com_ptr_nothrow<IMidiDataTransform> midiTransform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2UMP2BSMidiTransform>(&midiTransform));
        *activatedInterface = midiTransform.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

