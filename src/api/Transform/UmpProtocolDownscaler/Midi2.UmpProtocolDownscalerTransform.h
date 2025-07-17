// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI2_UMPPROTOCOLDOWNSCALERTRANSFORM_H
#define MIDI2_UMPPROTOCOLDOWNSCALERTRANSFORM_H


#include "pch.h"


class MidiUmpProtocolDownscalerTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiUmpProtocolDownscalerTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.UmpProtocolDownscalerTransform",
        // {b4d50468-e9d8-5080-5b75-e49eb39fee9c}
        (0xb4d50468,0xe9d8,0x5080,0x5b,0x75,0xe4,0x9e,0xb3,0x9f,0xee,0x9c))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2UmpProtocolDownscalerTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2UmpProtocolDownscalerTransform, &CLSID_Midi2UmpProtocolDownscalerTransform>,
    public IMidiTransform
{
public:
    CMidi2UmpProtocolDownscalerTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2UMPPROTOCOLDOWNSCALERTRANSFORM)

    BEGIN_COM_MAP(CMidi2UmpProtocolDownscalerTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2UmpProtocolDownscalerTransform), CMidi2UmpProtocolDownscalerTransform)


#endif