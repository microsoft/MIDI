// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include "Midi2LoopbackTransportAbstraction_h.h"


// When creating your own transport, change the GUID below to something new. The value
// is not important, so any new generated GUID is fine.
class MidiLoopbackTransportAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiLoopbackTransportAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.LoopbackTransportAbstraction",
        // {Cace0063-55ae-5d85-91ba-6814e2fa5555}
        (0xcace0063, 0x55ae, 0x5d85, 0x91, 0xba, 0x68, 0x14, 0xe2, 0xfa, 0x55, 0x55))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2LoopbackTransportAbstraction :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2LoopbackTransportAbstraction, &CLSID_Midi2LoopbackTransportAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2LoopbackTransportAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2LOOPBACKTRANSPORTABSTRACTION)

    BEGIN_COM_MAP(CMidi2LoopbackTransportAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2LoopbackTransportAbstraction), CMidi2LoopbackTransportAbstraction)


