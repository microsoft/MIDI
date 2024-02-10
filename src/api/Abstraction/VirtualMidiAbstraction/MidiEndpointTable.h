// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//#include "MidiVirtualDevice.h"

// thread-safe meyers singleton for storing the devices we'll use

// Endpoint Manager creates device-side endpoint based on configuration
// 
// Device App Initializes Device Endpoint
// - When device-side endpoint BiDi is initialized, the endpoint manager creates the client-side endpoint
// - Client endpoint property for which device endpoint it connects to is set in pnp properties
// - Coupling between device-side and client-side endpoints is added to device table
// - Each device is a single device endpoint which does not support multi-client
// 
// Client App Connects to Client Endpoint
// - Client Endpoint is Initialized
// - Client endpoint BiDi is added to device table. This endpoint supports multiclient.
//  
// Client app calls SendMessage
// - Client BiDi receives Callback
// - Client BiDi calls SendMessage on device side
// - Device BiDi calls Callback on Device app
// - Client (Device app is the client here) Pipe xproc stuff
// - Device app receives Callback
// 
// Device app calls SendMessage
// - Device BiDi receives Callback
// - Device BiDi calls SendMessage on client BiDi
// - Client BiDi calls Callback on Client app
// - Client Pipe xproc stuff
// - Client app receives Callback
//
// Client app closes connection
// - Client BiDi is torn down
// - Client is removed from device table
// 
// Device app closes connection
// - Device BiDi is torn down
// - All clients connected to that device are removed from the routing table, and are torn down
//
//



struct MidiVirtualDeviceEndpointEntry
{
    std::wstring VirtualEndpointAssociationId{ L"" };             // how the config entries associate endpoints. Typically a GUID
    std::wstring BaseEndpointName{ L"" };
    std::wstring Description{ L"" };
    std::wstring ShortUniqueId{ L"" };

    std::wstring CreatedDeviceEndpointId{ L"" };              // the device interface id
    std::wstring CreatedShortDeviceInstanceId{ L"" };

    std::wstring CreatedClientEndpointId{ L"" };
    std::wstring CreatedShortClientInstanceId{ L"" };


    wil::com_ptr_nothrow<CMidi2VirtualMidiBiDi> MidiDeviceBiDi{ nullptr };


 //   std::vector<wil::com_ptr_nothrow<CMidi2VirtualMidiBiDi>> MidiClientConnections{ };

    ~MidiVirtualDeviceEndpointEntry()
    {
        //MidiClientConnections.clear();

        if (MidiDeviceBiDi)
        {
            MidiDeviceBiDi.reset();
        }

    }
};


class MidiEndpointTable
{
public:
    HRESULT AddCreatedEndpointDevice(_In_ MidiVirtualDeviceEndpointEntry& entry) noexcept;
 
    HRESULT OnDeviceConnected(_In_ std::wstring deviceEndpointInterfaceId, _In_ CMidi2VirtualMidiBiDi* deviceBiDi) noexcept;
    HRESULT OnClientConnected(_In_ std::wstring clientEndpointInterfaceId, _In_ CMidi2VirtualMidiBiDi* clientBiDi) noexcept;

    HRESULT OnDeviceDisconnected(_In_ std::wstring deviceEndpointInterfaceId) noexcept;
    HRESULT OnClientDisconnected(_In_ std::wstring clientEndpointInterfaceId, _In_ CMidi2VirtualMidiBiDi* clientBiDi) noexcept;

    HRESULT Cleanup();

private:
    //void RemoveEndpointPair(_In_ GUID VirtualEndpointAssociationId) noexcept;

    // key is the association id
    std::map<std::wstring, MidiVirtualDeviceEndpointEntry> m_endpoints;

};