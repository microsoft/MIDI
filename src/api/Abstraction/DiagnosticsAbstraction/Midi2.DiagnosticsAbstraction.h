// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiDiagnosticsAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiDiagnosticsAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.DiagnosticsAbstraction",
        //  29251cdc-995a-53e6-b1a3-fe11e7ffe893 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.DiagnosticsAbstraction").Guid
        (0x29251cdc,0x995a,0x53e6,0xb1,0xa3,0xfe,0x11,0xe7,0xff,0xe8,0x93))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2DiagnosticsAbstraction :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2DiagnosticsAbstraction, &CLSID_Midi2DiagnosticsAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2DiagnosticsAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2DIAGNOSTICSABSTRACTION)

    BEGIN_COM_MAP(CMidi2DiagnosticsAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2DiagnosticsAbstraction), CMidi2DiagnosticsAbstraction)