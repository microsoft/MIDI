// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSynthTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSynthTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiSynthTransport",
        // {a4c41ca7-d5ec-50c2-5102-56c0be0387d0}
        // From PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.MidiSynthTransport").Guid
        (0xa4c41ca7,0xd5ec,0x50c2,0x51,0x02,0x56,0xc0,0xbe,0x03,0x87,0xd0))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2MidiSynthTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2MidiSynthTransport, &CLSID_Midi2MidiSynthTransport>,
    public IMidiTransport
{
public:
    CMidi2MidiSynthTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2MIDISYNTHTRANSPORT)

    BEGIN_COM_MAP(CMidi2MidiSynthTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_ void**);
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2MidiSynthTransport), CMidi2MidiSynthTransport)
