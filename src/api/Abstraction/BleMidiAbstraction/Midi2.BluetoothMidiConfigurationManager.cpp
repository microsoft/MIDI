// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"

_Use_decl_annotations_ 
HRESULT 
CMidi2BluetoothMidiConfigurationManager::Initialize(
    GUID AbstractionId, 
    IUnknown* MidiDeviceManager,
    IUnknown* MidiServiceConfigurationManagerInterface
)
{
    UNREFERENCED_PARAMETER(MidiServiceConfigurationManagerInterface);


    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, MidiDeviceManager);
    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_abstractionId = AbstractionId;

    return S_OK;
}

_Use_decl_annotations_ 
HRESULT 
CMidi2BluetoothMidiConfigurationManager::UpdateConfiguration(
    LPCWSTR ConfigurationJsonSection, 
    BOOL IsFromConfigurationFile, 
    BSTR* Response)
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    UNREFERENCED_PARAMETER(ConfigurationJsonSection);
    UNREFERENCED_PARAMETER(IsFromConfigurationFile);
    UNREFERENCED_PARAMETER(Response);

    return E_FAIL;
}
 
HRESULT 
CMidi2BluetoothMidiConfigurationManager::Cleanup()
{
    TraceLoggingWrite(
        MidiBluetoothMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}