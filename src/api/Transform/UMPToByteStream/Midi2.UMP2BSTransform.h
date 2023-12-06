// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiUMP2BSTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiUMP2BSTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.UMP2BSTransform",
        // {9d78b191-f0f9-5542-7353-85ec5df01503}
        (0x9d78b191,0xf0f9,0x5542,0x73,0x53,0x85,0xec,0x5d,0xf0,0x15,0x03))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2UMP2BSTransform : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2UMP2BSTransform, &CLSID_Midi2UMP2BSTransform>,
    public IMidiTransform
{
public:
    CMidi2UMP2BSTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2UMP2BSTRANSFORM)

    BEGIN_COM_MAP(CMidi2UMP2BSTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2UMP2BSTransform), CMidi2UMP2BSTransform)

