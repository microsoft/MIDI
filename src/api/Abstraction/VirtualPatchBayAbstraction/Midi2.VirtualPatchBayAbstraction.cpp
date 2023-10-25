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
CMidi2VirtualPatchBayAbstraction::Activate(
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
            MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiBiDi",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2VirtualPatchBayBiDi>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }


    // IMidiEndpointManager and IMidiApiEndpointManagerExtension are interfaces implemented by the same class
    // We want to make sure we're always returning the same instance for these calls
    else if (__uuidof(IMidiEndpointManager) == Riid/* || __uuidof(IMidiApiEndpointManagerExtension) == Riid*/)
    {
        TraceLoggingWrite(
            MidiVirtualPatchBayAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiEndpointManager",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (m_endpointManager == nullptr)
        {
            RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2VirtualPatchBayEndpointManager>(&m_endpointManager));
        }

        // TODO: Not sure if this is the right pattern for this or not. There's no detach call here, so does this leak?
        RETURN_IF_FAILED(m_endpointManager->QueryInterface(Riid, Interface));
    }

    else
    {
        OutputDebugString(L"" __FUNCTION__ " Returning E_NOINTERFACE. Was an interface added that isn't handled in the Abstraction?");

        return E_NOINTERFACE;
    }

    return S_OK;
}

