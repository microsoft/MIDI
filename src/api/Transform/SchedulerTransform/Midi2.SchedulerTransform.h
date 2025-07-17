// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiSchedulerTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSchedulerTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.SchedulerTransform",
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SchedulerTransform").Guid
        // 8e536c97-6c77-5695-9dbe-ff05129b564e
        (0x8e536c97,0x6c77,0x5695,0x9d,0xbe,0xff,0x05,0x12,0x9b,0x56,0x4e))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2SchedulerTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2SchedulerTransform, &CLSID_Midi2SchedulerTransform>,
    public IMidiTransform
{
public:
    CMidi2SchedulerTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2SCHEDULERTRANSFORM)

    BEGIN_COM_MAP(CMidi2SchedulerTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2SchedulerTransform), CMidi2SchedulerTransform)

