// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiDiagnosticsTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiDiagnosticsTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.DiagnosticsTransport",
        // {84856b86-fe14-51a3-96fa-b9683533d40b}
        (0x84856b86,0xfe14,0x51a3,0x96,0xfa,0xb9,0x68,0x35,0x33,0xd4,0x0b))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2DiagnosticsTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2DiagnosticsTransport, &CLSID_Midi2DiagnosticsTransport>,
    public IMidiTransport
{
public:
    CMidi2DiagnosticsTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2DIAGNOSTICSTRANSPORT)

    BEGIN_COM_MAP(CMidi2DiagnosticsTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2DiagnosticsTransport), CMidi2DiagnosticsTransport)