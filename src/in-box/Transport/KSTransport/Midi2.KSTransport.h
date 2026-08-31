// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiKSTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiKSTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.KSTransport",
        // {a8d144fe-be0c-5484-25a9-512d2f6445d5}
        (0xa8d144fe,0xbe0c,0x5484,0x25,0xa9,0x51,0x2d,0x2f,0x64,0x45,0xd5))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2KSTransport : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2KSTransport, &CLSID_Midi2KSTransport>,
    public IMidiTransport
{
public:
    CMidi2KSTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2KSTRANSPORT)

    BEGIN_COM_MAP(CMidi2KSTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2KSTransport), CMidi2KSTransport)

