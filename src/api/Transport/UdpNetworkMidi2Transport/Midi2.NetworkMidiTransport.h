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
        "Microsoft.Windows.Midi2.NetworkMidiTransport",
        // 69964598-a2da-5f65-9288-a394b9633704 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.NetworkMidiTransport").Guid
        (0x69964598,0xa2da,0x5f65,0x92,0x88,0xa3,0x94,0xb9,0x63,0x37,0x04))
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