// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CMidi2UMP2BSTransformModule : public ATL::CAtlDllModuleT< CMidi2UMP2BSTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2UMP2BSTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2UMP2BSTRANSFORM, "{5A2E778A-2450-42A0-88E9-E9C04CA2BA62}")
};

extern class CMidi2UMP2BSTransformModule _AtlModule;
