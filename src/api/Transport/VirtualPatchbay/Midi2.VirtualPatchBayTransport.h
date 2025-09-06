// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiVirtualPatchBayTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiVirtualPatchBayTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.VirtualPatchBayTransport",
        //  74d7e33b-2093-5dd1-64d7-541b04070d00 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.VirtualPatchBayTransport").Guid
        (0x74d7e33b,0x2093,0x5dd1,0x64,0xd7,0x54,0x1b,0x04,0x07,0x0d,0x00))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2VirtualPatchBayTransport : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2VirtualPatchBayTransport, &CLSID_Midi2VirtualPatchBayTransport>,
    public IMidiTransport
{
public:
    CMidi2VirtualPatchBayTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2VIRTUALPATCHBAYTRANSPORT)

    BEGIN_COM_MAP(CMidi2VirtualPatchBayTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2VirtualPatchBayTransport), CMidi2VirtualPatchBayTransport)