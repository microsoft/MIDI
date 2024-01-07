// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2JitterReductionListenerTransformModule : public ATL::CAtlDllModuleT< CMidi2JitterReductionListenerTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2JitterReductionListenerTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2JITTERREDUCTIONLISTENERTRANSFORM, "{cef39caa-8681-47ad-8aac-93f4e706ec94}")
};

extern class CMidi2JitterReductionListenerTransformModule _AtlModule;
