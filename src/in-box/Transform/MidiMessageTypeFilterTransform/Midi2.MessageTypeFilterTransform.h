// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI2_MESSAGE_TYPE_FILTER_TRANSFORM_H
#define MIDI2_MESSAGE_TYPE_FILTER_TRANSFORM_H


#include "pch.h"


class Midi2MessageTypeFilterTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        Midi2MessageTypeFilterTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.MessageTypeFilterTransform",
        // {2178467a-00d8-57c6-647a-eb57dfc362c8} from:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.MessageTypeFilterTransform").Guid
        (0x2178467a, 0x00d8, 0x57c6, 0x64, 0x7a, 0xeb, 0x57, 0xdf, 0xc3, 0x62, 0xc8))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2MessageTypeFilterTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2MessageTypeFilterTransform, &CLSID_Midi2MessageTypeFilterTransform>,
    public IMidiTransform
{
public:
    CMidi2MessageTypeFilterTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2_MESSAGE_TYPE_FILTER_TRANSFORM)

    BEGIN_COM_MAP(CMidi2MessageTypeFilterTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2MessageTypeFilterTransform), CMidi2MessageTypeFilterTransform)


#endif