// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CMidi2KSAbstractionModule : public ATL::CAtlDllModuleT< CMidi2KSAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2KSAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2KSABSTRACTION, "{05244d43-856c-4948-82de-eae678ab9f6f}")
};

extern class CMidi2KSAbstractionModule _AtlModule;
