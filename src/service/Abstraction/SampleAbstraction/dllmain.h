// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CMidi2SampleAbstractionModule : public ATL::CAtlDllModuleT< CMidi2SampleAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2SampleAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2SAMPLEABSTRACTION, "{F75CD4F1-D2EF-48B2-B3D9-CAA59A8B9CB9}")
};

extern class CMidi2SampleAbstractionModule _AtlModule;
