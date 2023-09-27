// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiKSAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiKSAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.KSAbstraction",
        // {62a41de5-442c-5fe3-c088-1138121d82a8}
        (0x62a41de5,0x442c,0x5fe3,0xc0,0x88,0x11,0x38,0x12,0x1d,0x82,0xa8))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2KSAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2KSAbstraction, &CLSID_Midi2KSAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2KSAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2KSABSTRACTION)

    BEGIN_COM_MAP(CMidi2KSAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2KSAbstraction), CMidi2KSAbstraction)

