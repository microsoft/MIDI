// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2KSAbstraction::Activate(
    REFIID Riid,
    void **Interface
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiIn) == Riid)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi in",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiIn> midiIn;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiIn>(&midiIn));
        *Interface = midiIn.detach();
    }
    else if (__uuidof(IMidiOut) == Riid)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi Out",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiOut> midiOut;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiOut>(&midiOut));
        *Interface = midiOut.detach();
    }
    else if (__uuidof(IMidiBiDi) == Riid)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi BiDi",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiBiDi>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }
    else if (__uuidof(IMidiEndpointManager) == Riid)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi Endpoint Manager",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiEndpointManager> midiEndpointManager;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiEndpointManager>(&midiEndpointManager));
        *Interface = midiEndpointManager.detach();
    }
    else if (__uuidof(IMidiAbstractionConfigurationManager) == Riid)
    {
        TraceLoggingWrite(
            MidiKSAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiConfigurationManager",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
        );

        // TODO: Need to use existing endpoint manager

        wil::com_ptr_nothrow<IMidiAbstractionConfigurationManager> midiConfigurationManager;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSMidiConfigurationManager>(&midiConfigurationManager));
        *Interface = midiConfigurationManager.detach();
    }

    else
    {
        return E_NOINTERFACE;
    }

    return S_OK;
}

