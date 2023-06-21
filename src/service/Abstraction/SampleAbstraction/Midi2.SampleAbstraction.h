// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiSampleAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSampleAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.SampleAbstraction",
        // {dade0063-97cf-5d85-91ba-6814e2faf549}
        (0xdade0063,0x97cf,0x5d85,0x91,0xba,0x68,0x14,0xe2,0xfa,0xf5,0x49))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2SampleAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2SampleAbstraction, &CLSID_Midi2SampleAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2SampleAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2SAMPLEABSTRACTION)

    BEGIN_COM_MAP(CMidi2SampleAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2SampleAbstraction), CMidi2SampleAbstraction)

