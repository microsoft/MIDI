// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2BasicLoopbackMidiTransportModule : public ATL::CAtlDllModuleT< CMidi2BasicLoopbackMidiTransportModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2BasicLoopbackMidiTransportLib)

    // the guid here is the lib guid from the IDL file, not the interface guid
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2BASICLOOPBACKMIDITRANSPORT, "{934b1877-4837-426c-acf8-6e9196604dbe}")
};

extern class CMidi2BasicLoopbackMidiTransportModule _AtlModule;
