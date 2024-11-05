// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiKSAggregateTransportTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiKSAggregateTransportTelemetryProvider,
        "Microsoft.Windows.Midi2.KSAggregateTransport",
        // {07e90998-c326-56f8-e99b-20afb12e1b4f}
        (0x07e90998,0xc326,0x56f8,0xe9,0x9b,0x20,0xaf,0xb1,0x2e,0x1b,0x4f))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2KSAggregateTransport :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2KSAggregateTransport, &CLSID_Midi2KSAggregateTransport>,
    public IMidiTransport
{
public:
    CMidi2KSAggregateTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2KSAGGREGATETRANSPORT)

    BEGIN_COM_MAP(CMidi2KSAggregateTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2KSAggregateTransport), CMidi2KSAggregateTransport)

