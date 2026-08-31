// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiVirtualMidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiVirtualMidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.VirtualMidiTransport",
        //PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.VirtualMidiTransport").Guid
        // {8d08cf09-eca5-58d0-f722-4ef1eca95be2}
        (0x8d08cf09,0xeca5,0x58d0,0xf7,0x22,0x4e,0xf1,0xec,0xa9,0x5b,0xe2))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2VirtualMidiTransport : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2VirtualMidiTransport, &CLSID_Midi2VirtualMidiTransport>,
    public IMidiTransport
{
public:
    CMidi2VirtualMidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2VIRTUALMIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2VirtualMidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2VirtualMidiTransport), CMidi2VirtualMidiTransport)