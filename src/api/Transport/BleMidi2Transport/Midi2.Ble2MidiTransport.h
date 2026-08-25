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
        "Microsoft.Windows.Midi2.BluetoothMidiTransport",
        // ad3b4351-d4a9-5be4-5f69-4fa6c5db220c from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.BluetoothMidiTransport").Guid
        (0xad3b4351,0xd4a9,0x5be4,0x5f,0x69,0x4f,0xa6,0xc5,0xdb,0x22,0x0c))
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