// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2NetworkMidiAbstractionModule : public ATL::CAtlDllModuleT< CMidi2NetworkMidiAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2NetworkMidiAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2NETWORKMIDIABSTRACTION, "{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}")
};

extern class CMidi2NetworkMidiAbstractionModule _AtlModule;
