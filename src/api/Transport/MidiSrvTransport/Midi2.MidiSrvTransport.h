// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

using namespace ATL;

class ATL_NO_VTABLE CMidi2MidiSrvTransport : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2MidiSrvTransport, &CLSID_Midi2MidiSrvTransport>,
    public IMidiTransport
{
public:
    CMidi2MidiSrvTransport()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2MIDISRVTRANSPORT)

    BEGIN_COM_MAP(CMidi2MidiSrvTransport)
        COM_INTERFACE_ENTRY(IMidiTransport)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2MidiSrvTransport), CMidi2MidiSrvTransport)

