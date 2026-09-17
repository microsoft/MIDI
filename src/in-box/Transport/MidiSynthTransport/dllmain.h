// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

class CMidi2MidiSynthTransportModule : public ATL::CAtlDllModuleT< CMidi2MidiSynthTransportModule >
{
public:
    DECLARE_LIBID(LIBID_Midi2MidiSynthTransportLib)

    // the guid here is the lib guid from the IDL file, not the interface guid
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2MIDISYNTHTRANSPORT, "{c6fd1c01-4252-439b-83a3-f9b45c10bc09}")
};

extern class CMidi2MidiSynthTransportModule _AtlModule;
