// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// This represents Windows MIDI Services acting as a client to a 
// remote network host
// 
// Each connection from this PC to a remote host results in a session. 
// Each valid session becomes a UMP Endpoint in Windows. The lifetime 
// of the UMP endpoint is tied to the lifetime of this session.

class MidiNetworkClientSession
{
public:
    HRESULT Initialize();

    HRESULT Shutdown();

private:

};
