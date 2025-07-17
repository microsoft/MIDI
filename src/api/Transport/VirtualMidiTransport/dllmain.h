// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2VirtualMidiTransportModule : public ATL::CAtlDllModuleT< CMidi2VirtualMidiTransportModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2VirtualMidiTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2VIRTUALMIDITRANSPORT, "{42af54fc-9aba-4fe8-8ea2-d32d2a333a0c}")
};

extern class CMidi2VirtualMidiTransportModule _AtlModule;
