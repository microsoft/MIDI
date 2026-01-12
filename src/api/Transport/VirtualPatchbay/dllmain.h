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
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2VIRTUALPATCHBAYTRANSPORT, LIBID_QUOTED_Midi2VirtualPatchBayTransport)
};

extern class CMidi2VirtualPatchBayTransportModule _AtlModule;
