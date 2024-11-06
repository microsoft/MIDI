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
        // {64ed8ddd-0036-5739-8591-ffc4c2eaa975}
        (0x64ed8ddd,0x0036,0x5739,0x85,0x91,0xff,0xc4,0xc2,0xea,0xa9,0x75))
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