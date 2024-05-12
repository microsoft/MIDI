// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CMidi2SampleTransformModule : public ATL::CAtlDllModuleT< CMidi2SampleTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2SampleTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2SAMPLETRANSFORM, "{8021FF86-72EC-41C1-A97B-5608AA89006E}")
};

extern class CMidi2SampleTransformModule _AtlModule;
