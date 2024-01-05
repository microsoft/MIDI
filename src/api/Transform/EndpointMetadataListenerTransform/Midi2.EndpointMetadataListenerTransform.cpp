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
CMidi2EndpointMetadataListenerTransform::Activate(
    REFIID Iid,
    void **Interface
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiDataTransform) == Iid)
    {
       OutputDebugString(L"" __FUNCTION__ " Activating IMidiDataTransform");

        TraceLoggingWrite(
            MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiDataTransform",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiDataTransform> transform;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2EndpointMetadataListenerMidiTransform>(&transform));
        *Interface = transform.detach();
    }
    else
    {
       TraceLoggingWrite(
           MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
           __FUNCTION__ " Returning E_NOINTERFACE. Was an interface added that isn't handled in the Transform?",
           TraceLoggingLevel(WINEVENT_LEVEL_INFO),
           TraceLoggingValue(__FUNCTION__),
           TraceLoggingPointer(this, "this")
        );

        return E_NOINTERFACE;
    }

    return S_OK;
}

