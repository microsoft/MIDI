// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSimpleLoopbackMidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSimpleLoopbackMidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.SimpleLoopbackMidiTransport",
        // {1d455bc6-4e81-5a60-04ee-aabb9455b9f7}
        // From PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SimpleLoopbackMidiTransport").Guid
        (0x1d455bc6,0x4e81,0x5a60,0x04,0xee,0xaa,0xbb,0x94,0x55,0xb9,0xf7))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2SimpleLoopbackMidiTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2SimpleLoopbackMidiTransport, &CLSID_Midi2SimpleLoopbackMidiTransport>,
    public IMidiTransport
{
public:
    CMidi2SimpleLoopbackMidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2SIMPLELOOPBACKMIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2SimpleLoopbackMidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:
    wil::com_ptr_nothrow<IMidiEndpointManager> m_EndpointManager;

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2SimpleLoopbackMidiTransport), CMidi2SimpleLoopbackMidiTransport)
