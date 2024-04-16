// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


// class for communicating with the endpoint. Not for enumeration

class MidiBleBiDi
{
public:
    HRESULT Initialize(_In_ LPCWSTR endpointDeviceInterfaceId);

    HRESULT SendMidiMessage();
    
    // todo: Callback for receiving messages. We'll just display them for now.

private:
    std::unique_ptr<MidiBleDevice> m_device{ nullptr };


};