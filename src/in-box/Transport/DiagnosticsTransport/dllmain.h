// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

class CMidi2DiagnosticsTransportModule : public ATL::CAtlDllModuleT< CMidi2DiagnosticsTransportModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2DiagnosticsTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2DIAGNOSTICSTRANSPORT, "{578daa61-200b-4622-8cb1-6d8a6fa93c17}")
};

extern class CMidi2DiagnosticsTransportModule _AtlModule;
