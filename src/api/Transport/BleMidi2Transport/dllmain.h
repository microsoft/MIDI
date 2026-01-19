// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2Ble2MidiTransportModule : public ATL::CAtlDllModuleT<CMidi2Ble2MidiTransportModule>
{
public :
    DECLARE_LIBID(LIBID_Midi2Ble2MidiTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2BLE2MIDITRANSPORT, "{c0bb738e-2753-43b1-aef9-5d10c6629f16}")
};

extern class CMidi2Ble2MidiTransportModule _AtlModule;
