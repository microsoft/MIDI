// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class CMidi2BluetoothMidiAbstractionModule : public ATL::CAtlDllModuleT< CMidi2BluetoothMidiAbstractionModule >
{
public :
    DECLARE_LIBID(LIBID_Midi2BluetoothMidiAbstractionLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2BLUETOOTHMIDIABSTRACTION, "{C6DFEA54-CB7E-43A0-9CC8-D224D839DB1B}")
};

extern class CMidi2BluetoothMidiAbstractionModule _AtlModule;
