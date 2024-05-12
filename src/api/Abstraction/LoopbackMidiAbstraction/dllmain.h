// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2LoopbackMidiAbstractionModule : public ATL::CAtlDllModuleT< CMidi2LoopbackMidiAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2LoopbackMidiAbstractionLib)

    // the guid here is the lib guid from the IDL file, not the interface guid
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2LOOPBACKMIDIABSTRACTION, "{945588ac-b364-4795-abc4-58cdf9009ed9}")
};

extern class CMidi2LoopbackMidiAbstractionModule _AtlModule;
