// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2JitterReductionAbstractionModule : public ATL::CAtlDllModuleT< CMidi2JitterReductionAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2JitterReductionAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2JITTERREDUCTIONABSTRACTION, "{77421f51-7cc2-40ff-bb91-ace053179858}")
};

extern class CMidi2JitterReductionAbstractionModule _AtlModule;
