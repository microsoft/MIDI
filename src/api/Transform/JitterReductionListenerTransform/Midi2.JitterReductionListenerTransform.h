// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiJitterReductionListenerTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiJitterReductionListenerTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.JitterReductionListenerTransform",
        // eff07153-d49a-512a-dcbf-0ae44e9f6dd7 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.JitterReductionListenerTransform").Guid
        (0xeff07153,0xd49a,0x512a,0xdc,0xbf,0x0a,0xe4,0x4e,0x9f,0x6d,0xd7))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2JitterReductionListenerTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2JitterReductionListenerTransform, &CLSID_Midi2JitterReductionListenerTransform>,
    public IMidiTransform
{
public:
    CMidi2JitterReductionListenerTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2JITTERREDUCTIONLISTENERTRANSFORM)

    BEGIN_COM_MAP(CMidi2JitterReductionListenerTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2JitterReductionListenerTransform), CMidi2JitterReductionListenerTransform)