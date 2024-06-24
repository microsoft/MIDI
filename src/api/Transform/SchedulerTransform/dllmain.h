// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2SchedulerTransformModule : public ATL::CAtlDllModuleT< CMidi2SchedulerTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2SchedulerTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2SCHEDULERTRANSFORM, "{2ac669ca-18ca-4e7d-951e-b6cc511c96fc}")
};

extern class CMidi2SchedulerTransformModule _AtlModule;
