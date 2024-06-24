// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2KSAbstractionModule : public ATL::CAtlDllModuleT< CMidi2KSAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2KSAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2KSABSTRACTION, "{05244d43-856c-4948-82de-eae678ab9f6f}")
};

extern class CMidi2KSAbstractionModule _AtlModule;
