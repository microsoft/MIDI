// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiBluetoothMidiAbstractionTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiBluetoothMidiAbstractionTelemetryProvider,
        "Microsoft.Windows.Midi2.BluetoothMidiAbstraction",
        //  a08e7533-60c7-5149-f3 58-3f46b097e52a from hash of name using:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.BluetoothMidiAbstraction").Guid
        (0xa08e7533,0x60c7,0x5149,0xf3,0x58,0x3f,0x46,0xb0,0x97,0xe5,0x2a))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2BluetoothMidiAbstraction : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2BluetoothMidiAbstraction, &CLSID_Midi2BluetoothMidiAbstraction>,
    public IMidiAbstraction
{
public:
    CMidi2BluetoothMidiAbstraction()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2BLUETOOTHMIDIABSTRACTION)

    BEGIN_COM_MAP(CMidi2BluetoothMidiAbstraction)
        COM_INTERFACE_ENTRY(IMidiAbstraction)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);

private:

};

OBJECT_ENTRY_AUTO(__uuidof(Midi2BluetoothMidiAbstraction), CMidi2BluetoothMidiAbstraction)