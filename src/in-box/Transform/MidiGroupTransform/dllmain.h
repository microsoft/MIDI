// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2GroupTransformModule : public ATL::CAtlDllModuleT< CMidi2GroupTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2GroupTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2_GROUP_TRANSFORM, LIBID_QUOTED_Midi2GroupTransform)
};

extern class CMidi2GroupTransformModule _AtlModule;
