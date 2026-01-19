// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI2_GROUP_TRANSFORM_H
#define MIDI2_GROUP_TRANSFORM_H


#include "pch.h"


class Midi2GroupTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        Midi2GroupTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.GroupTransform",
        // {c5e2f4f2-8580-560b-dfb8-2e3354a23689} from:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.GroupTransform").Guid
        (0xc5e2f4f2,0x8580,0x560b,0xdf,0xb8,0x2e,0x33,0x54,0xa2,0x36,0x89))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2GroupTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2GroupTransform, &CLSID_Midi2GroupTransform>,
    public IMidiTransform
{
public:
    CMidi2GroupTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2_GROUP_TRANSFORM)

    BEGIN_COM_MAP(CMidi2GroupTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2GroupTransform), CMidi2GroupTransform)


#endif