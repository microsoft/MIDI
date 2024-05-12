// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiKSAggregateAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiKSAggregateAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.KSAggregateAbstraction",
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.KSAggregateAbstraction").Guid
        // {c82e6dbe-4018-531e-3dc6-7a5df14b1d22}
        (0xc82e6dbe,0x4018,0x531e,0x3d,0xc6,0x7a,0x5d,0xf1,0x4b,0x1d,0x22))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2KSAggregateAbstraction :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2KSAggregateAbstraction, &CLSID_Midi2KSAggregateAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2KSAggregateAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2KSAGGREGATEABSTRACTION)

    BEGIN_COM_MAP(CMidi2KSAggregateAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2KSAggregateAbstraction), CMidi2KSAggregateAbstraction)

