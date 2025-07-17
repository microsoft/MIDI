// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

//#include "MidiVirtualDevice.h"

// thread-safe meyers singleton for storing the devices we'll use

// Endpoint Manager creates device-side endpoint based on configuration
// 
// Device App Initializes Device Endpoint
// - When device-side endpoint Bidi is initialized, the endpoint manager creates the client-side endpoint
// - Client endpoint property for which device endpoint it connects to is set in pnp properties
// - Coupling between device-side and client-side endpoints is added to device table
// - Each device is a single device endpoint which does not support multi-client
// 
// Client App Connects to Client Endpoint
// - Client Endpoint is Initialized
// - Client endpoint Bidi is added to device table. This endpoint supports multiclient.
//  
// Client app calls SendMessage
// - Client Bidi receives Callback
// - Client Bidi calls SendMessage on device side
// - Device Bidi calls Callback on Device app
// - Client (Device app is the client here) Pipe xproc stuff
// - Device app receives Callback
// 
// Device app calls SendMessage
// - Device Bidi receives Callback
// - Device Bidi calls SendMessage on client Bidi
// - Client Bidi calls Callback on Client app
// - Client Pipe xproc stuff
// - Client app receives Callback
//
// Client app closes connection
// - Client Bidi is torn down
// - Client is removed from device table
// 
// Device app closes connection
// - Device Bidi is torn down
// - All clients connected to that device are removed from the routing table, and are torn down
//
//

class MidiEndpointTable
{
public:
    HRESULT AddCreatedEndpointDevice(_In_ MidiVirtualDeviceEndpointEntry& entry) noexcept;
 
    HRESULT OnDeviceConnected(_In_ std::wstring const deviceEndpointInterfaceId, _In_ CMidi2VirtualMidiBidi* deviceBidi) noexcept;
    HRESULT OnClientConnected(_In_ std::wstring const clientEndpointInterfaceId, _In_ CMidi2VirtualMidiBidi* clientBidi) noexcept;

    HRESULT OnDeviceDisconnected(_In_ std::wstring const deviceEndpointInterfaceId) noexcept;
    HRESULT OnClientDisconnected(_In_ std::wstring const clientEndpointInterfaceId) noexcept;

    bool IsUniqueIdInUse(_In_ std::wstring const uniqueId) noexcept;

    HRESULT Shutdown();

private:

    wil::critical_section m_entriesLock;

    // key is the association id
    std::map<std::wstring, MidiVirtualDeviceEndpointEntry> m_endpoints;

};