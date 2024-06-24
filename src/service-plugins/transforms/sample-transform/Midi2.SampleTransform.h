// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiSampleTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiSampleTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.SampleTransform",
        // {3234a00b-e870-5593-cbdc-1e97828f2622}
        (0x3234a00b,0xe870,0x5593,0xcb,0xdc,0x1e,0x97,0x82,0x8f,0x26,0x22))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2SampleTransform : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2SampleTransform, &CLSID_Midi2SampleTransform>,
    public IMidiTransform
{
public:
    CMidi2SampleTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2SAMPLETRANSFORM)

    BEGIN_COM_MAP(CMidi2SampleTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2SampleTransform), CMidi2SampleTransform)

