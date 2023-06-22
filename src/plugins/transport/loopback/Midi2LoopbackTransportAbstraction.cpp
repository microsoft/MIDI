// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2LoopbackTransportAbstraction::Activate(
    REFIID Iid,
    void** Interface
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    // TODO: For loopback , these always come in pairs, so need to sort out how I'm 
    // supposed to create and return two instances of bidi

    if (__uuidof(IMidiBiDi) == Iid)
    {
        TraceLoggingWrite(
            MidiLoopbackTransportAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi BiDi",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
        );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2LoopbackEndpoint>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}
