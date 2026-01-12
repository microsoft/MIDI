// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2ChannelTransformModule : public ATL::CAtlDllModuleT< CMidi2ChannelTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2ChannelTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2_CHANNEL_TRANSFORM, LIBID_QUOTED_Midi2ChannelTransform)
};

extern class CMidi2ChannelTransformModule _AtlModule;
