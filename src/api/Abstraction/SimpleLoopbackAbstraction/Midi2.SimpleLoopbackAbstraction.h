// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiSimpleLoopbackAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSimpleLoopbackAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.SimpleLoopbackAbstraction",
        //  ad787da6-8272-5a1c-832f-122e2324703d from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SimpleLoopbackAbstraction").Guid
        (0xad787da6,0x8272,0x5a1c,0x83,0x2f,0x12,0x2e,0x23,0x24,0x70,0x3d))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2SimpleLoopbackAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2SimpleLoopbackAbstraction, &CLSID_Midi2SimpleLoopbackAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2SimpleLoopbackAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2SIMPLELOOPBACKABSTRACTION)

    BEGIN_COM_MAP(CMidi2SimpleLoopbackAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2SimpleLoopbackAbstraction), CMidi2SimpleLoopbackAbstraction)