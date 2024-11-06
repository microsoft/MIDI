// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSrvTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSrvTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiSrvTransport",
        // {c22d26fd-947b-5df1-a0f8-bc62d26d188d}
        (0xc22d26fd,0x947b,0x5df1,0xa0,0xf8,0xbc,0x62,0xd2,0x6d,0x18,0x8d))
};





HRESULT GetMidiSrvBindingHandle(_In_ handle_t* BindingHandle);

using namespace ATL;

class ATL_NO_VTABLE CMidi2MidiSrvTransport : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2MidiSrvTransport, &CLSID_Midi2MidiSrvTransport>,
    public IMidiTransport
{
public:
    CMidi2MidiSrvTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2MIDISRVTRANSPORT)

    BEGIN_COM_MAP(CMidi2MidiSrvTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2MidiSrvTransport), CMidi2MidiSrvTransport)

