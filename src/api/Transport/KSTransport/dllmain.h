// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2KSTransportModule : public ATL::CAtlDllModuleT< CMidi2KSTransportModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2KSTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2KSTRANSPORT, "{05244d43-856c-4948-82de-eae678ab9f6f}")
};

extern class CMidi2KSTransportModule _AtlModule;
