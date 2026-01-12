// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiBle2MidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiBle2MidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.Ble2MidiTransport",
        // a62ca586-f59a-5046-0c50-1e905dfe9355 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Ble2MidiTransport").Guid
        (0xa62ca586,0xf59a,0x5046,0x0c,0x50,0x1e,0x90,0x5d,0xfe,0x93,0x55))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2Ble2MidiTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2Ble2MidiTransport, &CLSID_Midi2Ble2MidiTransport>,
    public IMidiTransport
{
public:
    CMidi2Ble2MidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2BLE2MIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2Ble2MidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2Ble2MidiTransport), CMidi2Ble2MidiTransport)