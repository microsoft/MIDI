// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiVirtualMidiAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiVirtualMidiAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.VirtualMidiAbstraction",
        //  9ce595de-7672-55c3-6529-20 4e aa e0 bd 29 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.VirtualMidiAbstraction").Guid
        (0x9ce595de,0x7672,0x55c3,0x65,0x29,0x20,0x4e,0xaa,0xe0,0xbd,0x29))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2VirtualMidiAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2VirtualMidiAbstraction, &CLSID_Midi2VirtualMidiAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2VirtualMidiAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2VIRTUALMIDIABSTRACTION)

    BEGIN_COM_MAP(CMidi2VirtualMidiAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2VirtualMidiAbstraction), CMidi2VirtualMidiAbstraction)