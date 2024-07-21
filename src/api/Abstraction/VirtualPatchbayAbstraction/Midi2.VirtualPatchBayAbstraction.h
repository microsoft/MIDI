// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiVirtualPatchBayAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiVirtualPatchBayAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.VirtualPatchBayAbstraction",
        //  53e8558d-340c-5a2f-37e9-8e5d56758792 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.VirtualPatchBayAbstraction").Guid
        (0x53e8558d,0x340c,0x5a2f,0x37,0xe9,0x8e,0x5d,0x56,0x75,0x87,0x92))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2VirtualPatchBayAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2VirtualPatchBayAbstraction, &CLSID_Midi2VirtualPatchBayAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2VirtualPatchBayAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2VIRTUALPATCHBAYABSTRACTION)

    BEGIN_COM_MAP(CMidi2VirtualPatchBayAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2VirtualPatchBayAbstraction), CMidi2VirtualPatchBayAbstraction)