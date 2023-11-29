// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiJitterReductionAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiJitterReductionAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.JitterReductionAbstraction",
        // e0e0432a-75cb-5c98-c107-431e92c5c675 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.JitterReductionAbstraction").Guid
        (0xe0e0432a,0x75cb,0x5c98,0xc1,0x07,0x43,0x1e,0x92,0xc5,0xc6,0x75))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2JitterReductionAbstraction :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2JitterReductionAbstraction, &CLSID_Midi2JitterReductionAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2JitterReductionAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2JITTERREDUCTIONABSTRACTION)

    BEGIN_COM_MAP(CMidi2JitterReductionAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2JitterReductionAbstraction), CMidi2JitterReductionAbstraction)