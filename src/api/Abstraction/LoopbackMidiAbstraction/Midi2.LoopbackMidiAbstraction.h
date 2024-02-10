// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiLoopbackMidiAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiLoopbackMidiAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.LoopbackMidiAbstraction",
        //  5a36c0b3-5d4c-545b-b9e7-461470957699 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.LoopbackMidiAbstraction").Guid
        (0x5a36c0b3,0x5d4,0x545b,0xb9,0xe7,0x46,0x14,0x70,0x95,0x76,0x99))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2LoopbackMidiAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2LoopbackMidiAbstraction, &CLSID_Midi2LoopbackMidiAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2LoopbackMidiAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2LOOPBACKMIDIABSTRACTION)

    BEGIN_COM_MAP(CMidi2LoopbackMidiAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:
    wil::com_ptr_nothrow<IMidiEndpointManager> m_EndpointManager;

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2LoopbackMidiAbstraction), CMidi2LoopbackMidiAbstraction)