// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiNetworkMidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiNetworkMidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.NetworkMidiAbstraction",
        // ee50a899-6caf-526c-e261-1f00403716f3 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.NetworkMidiAbstraction").Guid
        (0xee50a899,0x6caf,0x526c,0xe2,0x61,0x1f,0x00,0x40,0x37,0x16,0xf3))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2NetworkMidiTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2NetworkMidiTransport, &CLSID_Midi2NetworkMidiTransport>,
    public IMidiTransport
{
public:
    CMidi2NetworkMidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2NETWORKMIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2NetworkMidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2NetworkMidiTransport), CMidi2NetworkMidiTransport)