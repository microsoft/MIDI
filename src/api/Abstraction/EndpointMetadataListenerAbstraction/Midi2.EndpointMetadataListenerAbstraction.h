// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiEndpointMetadataListenerAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiEndpointMetadataListenerAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.EndpointMetadataListenerAbstraction",
        // 63cd2772-1a0d-5daa-d87e-545089607954 from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.EndpointMetadataListenerAbstraction").Guid
        (0x63cd2772, 0x1a0d, 0x5daa, 0xd8,0x7e, 0x54,0x50,0x89,0x60,0x79,0x54))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2EndpointMetadataListenerAbstraction :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2EndpointMetadataListenerAbstraction, &CLSID_Midi2EndpointMetadataListenerAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2EndpointMetadataListenerAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2ENDPOINTMETADATALISTENERABSTRACTION)

    BEGIN_COM_MAP(CMidi2EndpointMetadataListenerAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2EndpointMetadataListenerAbstraction), CMidi2EndpointMetadataListenerAbstraction)