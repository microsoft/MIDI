// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2VirtualMidiAbstractionModule : public ATL::CAtlDllModuleT< CMidi2VirtualMidiAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2VirtualMidiAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2VIRTUALMIDIABSTRACTION, "{42af54fc-9aba-4fe8-8ea2-d32d2a333a0c}")
};

extern class CMidi2VirtualMidiAbstractionModule _AtlModule;
