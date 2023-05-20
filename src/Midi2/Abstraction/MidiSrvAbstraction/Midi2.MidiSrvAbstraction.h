// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiSrvAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSrvAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiSrvAbstraction",
        // {6e9d2090-31a4-5a2c-da35-fd606d7d6ac3}
        (0x6e9d2090,0x31a4,0x5a2c,0xda,0x35,0xfd,0x60,0x6d,0x7d,0x6a,0xc3))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2MidiSrvAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2MidiSrvAbstraction, &CLSID_Midi2MidiSrvAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2MidiSrvAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2MIDISRVABSTRACTION)

    BEGIN_COM_MAP(CMidi2MidiSrvAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2MidiSrvAbstraction), CMidi2MidiSrvAbstraction)

