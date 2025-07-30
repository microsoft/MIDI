// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2Ble1MidiTransportModule : public ATL::CAtlDllModuleT<CMidi2Ble1MidiTransportModule>
{
public :
    DECLARE_LIBID(LIBID_Midi2Ble1MidiTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2BLE1MIDITRANSPORT, "{34cf426c-6683-4851-8e0f-7693ca011921}")
};

extern class CMidi2Ble1MidiTransportModule _AtlModule;
