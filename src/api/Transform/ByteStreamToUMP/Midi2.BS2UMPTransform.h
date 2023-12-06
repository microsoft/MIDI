// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class MidiBS2UMPTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiBS2UMPTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.BS2UMPTransform",
        // {b4d50468-e9d8-5080-5b75-e49eb39fee9c}
        (0xb4d50468,0xe9d8,0x5080,0x5b,0x75,0xe4,0x9e,0xb3,0x9f,0xee,0x9c))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2BS2UMPTransform : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2BS2UMPTransform, &CLSID_Midi2BS2UMPTransform>,
    public IMidiTransform
{
public:
    CMidi2BS2UMPTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2BS2UMPTRANSFORM)

    BEGIN_COM_MAP(CMidi2BS2UMPTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2BS2UMPTransform), CMidi2BS2UMPTransform)

