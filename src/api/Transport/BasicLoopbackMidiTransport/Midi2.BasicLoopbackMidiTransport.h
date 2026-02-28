// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiBasicLoopbackMidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiBasicLoopbackMidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.BasicLoopbackMidiTransport",
        // {9590999c-248c-5425-cd81-44bcf6df74c8}
        // From PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.BasicLoopbackMidiTransport").Guid
        (0x9590999c,0x248c,0x5425,0xcd,0x81,0x44,0xbc,0xf6,0xdf,0x74,0xc8))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2BasicLoopbackMidiTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2BasicLoopbackMidiTransport, &CLSID_Midi2BasicLoopbackMidiTransport>,
    public IMidiTransport
{
public:
    CMidi2BasicLoopbackMidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2BASICLOOPBACKMIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2BasicLoopbackMidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:
    wil::com_ptr_nothrow<IMidiEndpointManager> m_EndpointManager;

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2BasicLoopbackMidiTransport), CMidi2BasicLoopbackMidiTransport)
