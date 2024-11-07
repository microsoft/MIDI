// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class MidiClientInitializerTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiClientInitializerTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiClientInitializer",
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.MidiClientInitializer").Guid
        // 34818be8-1464-5725-0cc6-d2289a28b8fb
        (0x34818be8,0x1464,0x5725,0x0c,0xc6,0xd2,0x28,0x9a,0x28,0xb8,0xfb))
};

using namespace ATL;

class ATL_NO_VTABLE CMidiClientInitializer :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidiClientInitializer, &CLSID_MidiClientInitializer>,
    public IMidiClientInitializer
{
public:
    CMidiClientInitializer()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDICLIENTINITIALIZER)

    BEGIN_COM_MAP(CMidiClientInitializer)
        COM_INTERFACE_ENTRY(IMidiClientInitializer)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Initialize)();
    STDMETHOD(GetInstalledWindowsMidiServicesSdkVersion)(_In_ PWINDOWSMIDISERVICESAPPSDKVERSION sdkVersion);
    STDMETHOD(EnsureServiceAvailable)();
    STDMETHOD(Shutdown)();

    // TODO: Method to check service version as well

private:
};

OBJECT_ENTRY_AUTO(__uuidof(MidiClientInitializer), CMidiClientInitializer)

