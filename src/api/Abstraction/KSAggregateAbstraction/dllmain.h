// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2KSAggregateAbstractionModule : public ATL::CAtlDllModuleT< CMidi2KSAggregateAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2KSAggregateAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2KSAGGREGATEABSTRACTION, "{74174a03-30ce-4a50-a440-bc969cb4d475}")
};

extern class CMidi2KSAggregateAbstractionModule _AtlModule;
