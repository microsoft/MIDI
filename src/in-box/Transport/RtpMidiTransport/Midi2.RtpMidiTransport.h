// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// The transport's COM class, which hands the service each of its parts.
// ============================================================================

#pragma once

class MidiRtpMidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiRtpMidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.RtpMidiTransport",
        // dd23cc44-8a30-593d-157f-db9ceae3452a from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.RtpMidiTransport").Guid
        (0xdd23cc44,0x8a30,0x593d,0x15,0x7f,0xdb,0x9c,0xea,0xe3,0x45,0x2a))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2RtpMidiTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2RtpMidiTransport, &CLSID_Midi2RtpMidiTransport>,
    public IMidiTransport
{
public:
    CMidi2RtpMidiTransport() = default;

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2RTPMIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2RtpMidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID riid, _Out_ void** requestedInterface);
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2RtpMidiTransport), CMidi2RtpMidiTransport)
