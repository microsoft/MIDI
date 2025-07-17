// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiLoopbackMidiTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiLoopbackMidiTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.LoopbackMidiTransport",
        // {23324529-7840-5bb4-9d74-c61e306f00d0}
        (0x23324529,0x7840,0x5bb4,0x9d,0x74,0xc6,0x1e,0x30,0x6f,0x00,0xd0))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2LoopbackMidiTransport : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2LoopbackMidiTransport, &CLSID_Midi2LoopbackMidiTransport>,
    public IMidiTransport
{
public:
    CMidi2LoopbackMidiTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2LOOPBACKMIDITRANSPORT)

    BEGIN_COM_MAP(CMidi2LoopbackMidiTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:
    wil::com_ptr_nothrow<IMidiEndpointManager> m_EndpointManager;

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2LoopbackMidiTransport), CMidi2LoopbackMidiTransport)
