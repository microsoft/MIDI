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
CMidi2MidiSrvConfigurationManager::UpdateConfiguration(LPCWSTR configurationJson, BSTR* response)
{
    UNREFERENCED_PARAMETER(configurationJson);
    UNREFERENCED_PARAMETER(response);

    return S_OK;
}




_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::Initialize(GUID abstractionGuid, IUnknown* deviceManagerInterface)
{
    UNREFERENCED_PARAMETER(abstractionGuid);
    UNREFERENCED_PARAMETER(deviceManagerInterface);

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2MidiSrvConfigurationManager::Cleanup()
{
    return S_OK;
}

