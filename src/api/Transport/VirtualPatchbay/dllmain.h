// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2VirtualPatchBayTransportModule : public ATL::CAtlDllModuleT<CMidi2VirtualPatchBayTransportModule>
{
public :
    DECLARE_LIBID(LIBID_Midi2VirtualPatchBayTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2VIRTUALPATCHBAYTRANSPORT, "{51F2C928-D846-45E9-BFA8-058BC804FF3E}")
};

extern class CMidi2VirtualPatchBayTransportModule _AtlModule;
