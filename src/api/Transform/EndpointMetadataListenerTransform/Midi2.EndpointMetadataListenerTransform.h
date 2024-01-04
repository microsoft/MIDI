// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiEndpointMetadataListenerTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiEndpointMetadataListenerTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.EndpointMetadataListenerTransform",
        // d5649582-c09a-55e8-90c0-b507720f8176 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.EndpointMetadataListenerTransform").Guid
        (0xd5649582, 0xc09a, 0x55e8, 0x90,0xc0, 0xb5,0x07,0x72,0x0f,0x81,0x76))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2EndpointMetadataListenerTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2EndpointMetadataListenerTransform, &CLSID_Midi2EndpointMetadataListenerTransform>,
    public IMidiTransform
{
public:
    CMidi2EndpointMetadataListenerTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2ENDPOINTMETADATALISTENERTRANSFORM)

    BEGIN_COM_MAP(CMidi2EndpointMetadataListenerTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2EndpointMetadataListenerTransform), CMidi2EndpointMetadataListenerTransform)