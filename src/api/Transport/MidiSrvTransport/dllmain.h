// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

class CMidi2MidiSrvTransportModule : public ATL::CAtlDllModuleT< CMidi2MidiSrvTransportModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2MidiSrvTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2MIDISRVTRANSPORT, "{15C88C78-4CB3-49B5-97D4-01E60C8E6B9D}")
};

extern class CMidi2MidiSrvTransportModule _AtlModule;
