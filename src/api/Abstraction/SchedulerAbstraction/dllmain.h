// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2SchedulerAbstractionModule : public ATL::CAtlDllModuleT< CMidi2SchedulerAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2SchedulerAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2SCHEDULERABSTRACTION, "{001776b2-f6ad-4faa-8610-0aab74ad3957}")
};

extern class CMidi2SchedulerAbstractionModule _AtlModule;
