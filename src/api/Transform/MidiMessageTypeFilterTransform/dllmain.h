// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MessageTypeFilterTransformModule : public ATL::CAtlDllModuleT< CMidi2MessageTypeFilterTransformModule >
{
public:
    DECLARE_LIBID(LIBID_Midi2MessageTypeFilterTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2_MESSAGE_TYPE_FILTER_TRANSFORM, LIBID_QUOTED_Midi2MessageTypeFilterTransform)
};

extern class CMidi2MessageTypeFilterTransformModule _AtlModule;
