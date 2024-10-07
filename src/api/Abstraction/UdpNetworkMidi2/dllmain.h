// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2NetworkMidiTransportModule : public ATL::CAtlDllModuleT< CMidi2NetworkMidiTransportModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2NetworkMidiTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2NETWORKMIDITRANSPORT, "{4568a471-6f32-4015-b4db-9087bfb60a0b}")
};

extern class CMidi2NetworkMidiTransportModule _AtlModule;
