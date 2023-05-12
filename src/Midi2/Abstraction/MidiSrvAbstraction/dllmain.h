// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CMidi2MidiSrvAbstractionModule : public ATL::CAtlDllModuleT< CMidi2MidiSrvAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2MidiSrvAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2MIDISRVABSTRACTION, "{15C88C78-4CB3-49B5-97D4-01E60C8E6B9D}")
};

extern class CMidi2MidiSrvAbstractionModule _AtlModule;
