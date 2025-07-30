// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiBle1MidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiBle1MidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.Ble1MidiTransport",
        // 7c2237b1-fa15-530f-3b15-3cf59ef6542d from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.Ble1MidiTransport").Guid
        (0x7c2237b1,0xfa15,0x530f,0x3b,0x1f,0x3c,0xf5,0x9e,0xf6,0x54,0x2d))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2Ble1MidiTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2Ble1MidiTransport, &CLSID_Midi2Ble1MidiTransport>,
    public IMidiTransport
{
public:
    CMidi2Ble1MidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2BLE1MIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2Ble1MidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2Ble1MidiTransport), CMidi2Ble1MidiTransport)