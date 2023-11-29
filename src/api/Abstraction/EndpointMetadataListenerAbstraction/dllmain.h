// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2EndpointMetadataListenerAbstractionModule : public ATL::CAtlDllModuleT< CMidi2EndpointMetadataListenerAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2EndpointMetadataListenerAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2ENDPOINTMETADATALISTENERABSTRACTION, "{f9d8d093-27d4-44b8-a458-f057603b4115}")
};

extern class CMidi2EndpointMetadataListenerAbstractionModule _AtlModule;
