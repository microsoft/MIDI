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
CMidi2NetworkMidiAbstraction::Activate(
    REFIID Riid,
    void **Interface
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    RETURN_HR_IF(E_INVALIDARG, nullptr == Interface);

   /*if (__uuidof(IMidiIn) == Riid)
    {
       OutputDebugString(L"" __FUNCTION__ " Activating IMidiIn");

        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi in",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiIn> midiIn;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiIn>(&midiIn));
        *Interface = midiIn.detach();
    }
    else if (__uuidof(IMidiOut) == Riid)
    {
       OutputDebugString(L"" __FUNCTION__ " Activating IMidiOut");
       
       TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- Midi Out",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingValue(__FUNCTION__),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiOut> midiOut;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiOut>(&midiOut));
        *Interface = midiOut.detach();
    }
    else*/ if (__uuidof(IMidiBiDi) == Riid)
    {
       OutputDebugString(L"" __FUNCTION__ " Activating IMidiBiDi");

        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiBiDi",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"IMidiBiDi", "requested interface"),
            TraceLoggingPointer(this, "this")
            );

        wil::com_ptr_nothrow<IMidiBiDi> midiBiDi;
        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2NetworkMidiBiDi>(&midiBiDi));
        *Interface = midiBiDi.detach();
    }


    else if (__uuidof(IMidiEndpointManager) == Riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__ "- IMidiEndpointManager",
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"IMidiEndpointManager", "requested interface"),
            TraceLoggingPointer(this, "this")
        );

        // check to see if this is the first time we're creating the endpoint manager. If so, create it.
        if (AbstractionState::Current().GetEndpointManager() == nullptr)
        {
            AbstractionState::Current().ConstructEndpointManager();
        }

        RETURN_IF_FAILED(AbstractionState::Current().GetEndpointManager()->QueryInterface(Riid, Interface));
    }


    else if (__uuidof(IMidiAbstractionConfigurationManager) == Riid)
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"IMidiAbstractionConfigurationManager", "requested interface"),
            TraceLoggingPointer(this, "this")
        );

        // check to see if this is the first time we're creating the configuration manager. If so, create it.
        if (AbstractionState::Current().GetConfigurationManager() == nullptr)
        {
            AbstractionState::Current().ConstructConfigurationManager();
        }

        RETURN_IF_FAILED(AbstractionState::Current().GetConfigurationManager()->QueryInterface(Riid, Interface));
    }

    else
    {
        TraceLoggingWrite(
            MidiNetworkMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"Unknown or invalid interface request", "message"),
            TraceLoggingPointer(this, "this")
        );

        return E_NOINTERFACE;
    }

    return S_OK;
}

