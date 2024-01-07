// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2JitterReductionGeneratorTransformModule : public ATL::CAtlDllModuleT< CMidi2JitterReductionGeneratorTransformModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2JitterReductionGeneratorTransformLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2JITTERREDUCTIONGENERATORTRANSFORM, "{31e46efc-bfde-46af-8c1f-5151abdada7e}")
};

extern class CMidi2JitterReductionGeneratorTransformModule _AtlModule;
