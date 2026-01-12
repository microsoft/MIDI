// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI2_CHANNEL_TRANSFORM_ACTIVATION_H
#define MIDI2_CHANNEL_TRANSFORM_ACTIVATION_H


#include "pch.h"


class Midi2ChannelTransformTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        Midi2ChannelTransformTelemetryProvider,
        "Microsoft.Windows.Midi2.ChannelTransform",
        // {f7b8d182-32c9-5be2-3325-590192fd9763} from:
        // PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Windows.Midi2.ChannelTransform").Guid
        (0xf7b8d182,0x32c9,0x5be2,0x33,0x25,0x59,0x01,0x92,0xfd,0x97,0x63))
};

using namespace ATL;

class ATL_NO_VTABLE CMidi2ChannelTransform :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMidi2ChannelTransform, &CLSID_Midi2ChannelTransform>,
    public IMidiTransform
{
public:
    CMidi2ChannelTransform()
    {
    }

    DECLARE_REGISTRY_RESOURCEID(IDR_MIDI2_CHANNEL_TRANSFORM)

    BEGIN_COM_MAP(CMidi2ChannelTransform)
        COM_INTERFACE_ENTRY(IMidiTransform)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    STDMETHOD(Activate)(_In_ REFIID, _Out_  void**);
private:
};

OBJECT_ENTRY_AUTO(__uuidof(Midi2ChannelTransform), CMidi2ChannelTransform)


#endif