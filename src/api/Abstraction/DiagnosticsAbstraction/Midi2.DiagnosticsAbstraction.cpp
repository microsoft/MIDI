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
CMidi2DiagnosticsAbstraction::Activate(
    REFIID Riid,
    void **Interface
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

    if (__uuidof(IMidiBiDi) == Riid)
    {
       OutputDebugString(L"" __FUNCTION__ " Activating IMidiBiDi");

        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi BiDi",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2LoopbackMidiBiDi>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }
    else if (__uuidof(IMidiEndpointManager) == Riid)
    {
        OutputDebugString(L"" __FUNCTION__ " Activating IMidiEndpointManager");

        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi Endpoint Manager",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
        );

        wil::com_ptr_nothrow<IMidiEndpointManager> midiEndpointManager;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2DiagnosticsEndpointManager>(&midiEndpointManager));
        *Interface = midiEndpointManager.detach();
    }

    else if (__uuidof(IMidiServiceAbstractionPluginMetadataProvider) == Riid)
    {
        TraceLoggingWrite(
            MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiServiceAbstractionPluginMetadataProvider",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
        );

        wil::com_ptr_nothrow<IMidiServiceAbstractionPluginMetadataProvider> metadataProvider;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2DiagnosticsPluginMetadataProvider>(&metadataProvider));
        *Interface = metadataProvider.detach();
    }

    else
    {
        OutputDebugString(L"" __FUNCTION__ " Unrecognized interface");

        TraceLoggingWrite(
           MidiDiagnosticsAbstractionTelemetryProvider::Provider(),
           __FUNCTION__ " Returning E_NOINTERFACE. Was an interface added that isn't handled in the Abstraction?",
           TraceLoggingLevel(WINEVENT_LEVEL_INFO),
           TraceLoggingValue(__FUNCTION__),
           TraceLoggingPointer(this, "this")
        );

        return E_NOINTERFACE;
    }

    return S_OK;
}

