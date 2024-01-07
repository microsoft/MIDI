// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiJitterReductionGeneratorTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiJitterReductionGeneratorTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.JitterReductionGeneratorTransform",
        // 130f5031-fd9e-5218-3df6-42b5e90f816b from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.JitterReductionGeneratorTransform").Guid
        (0x130f5031,0xfd9e,0x5218,0x3d,0xf6,0x42,0xb5,0xe9,0x0f,0x81,0x6b))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2JitterReductionGeneratorTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2JitterReductionGeneratorTransform, &CLSID_Midi2JitterReductionGeneratorTransform>,
    public IMidiTransform
{
public:
    CMidi2JitterReductionGeneratorTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2JITTERREDUCTIONGENERATORTRANSFORM)

    BEGIN_COM_MAP(CMidi2JitterReductionGeneratorTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2JitterReductionGeneratorTransform), CMidi2JitterReductionGeneratorTransform)