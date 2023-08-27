// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2SimpleLoopbackAbstractionModule : public ATL::CAtlDllModuleT< CMidi2SimpleLoopbackAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2SimpleLoopbackAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2SIMPLELOOPBACKABSTRACTION, "{578daa61-200b-4622-8cb1-6d8a6fa93c17}")
};

extern class CMidi2SimpleLoopbackAbstractionModule _AtlModule;
