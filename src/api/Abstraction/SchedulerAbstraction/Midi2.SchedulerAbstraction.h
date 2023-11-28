// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiSchedulerAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSchedulerAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.SchedulerAbstraction",
        // 9ce181f3-0054-5b11-801d-c4d3a1a820b0 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.SchedulerAbstraction").Guid
        (0x9ce181f3,0x0054,0x5b11,0x80,0x1d,0xc4,0xd3,0xa1,0xa8,0x20,0xb0))
};

//using namespace ATL;
//
//class ATL_NO_VTABLE CMidi2SchedulerAbstraction :
//    public CComObjectRootEx<CComMultiThreadModel>,
//    public CComCoClass<CMidi2SchedulerAbstraction, &CLSID_Midi2SchedulerAbstraction>,
//    public IMidiAbstraction
//{
//public:
//    CMidi2SchedulerAbstraction()
//    {
//    }
//
//    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2SCHEDULERABSTRACTION)
//
//    BEGIN_COM_MAP(CMidi2SchedulerAbstraction)
//        COM_INTERFACE_ENTRY(IMidiAbstraction)
//    END_COM_MAP()
//
//    DECLARE_PROTECT_FINAL_CONSTRUCT()
//
//    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
//private:
//};
//
//OBJECT_ENTRY_AUTO(__uuidof(Midi2SchedulerAbstraction), CMidi2SchedulerAbstraction)