// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"

_Use_decl_annotations_
HRESULT 
CMidiEndpointProtocolManager::Initialize(
    std::shared_ptr<CMidiClientManager>& ClientManager,
    std::shared_ptr<CMidiDeviceManager>& DeviceManager
)
{

    UNREFERENCED_PARAMETER(ClientManager);
    UNREFERENCED_PARAMETER(DeviceManager);

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidiEndpointProtocolManager::NegotiateAndRequestMetadata(
    LPCWSTR DeviceInterfaceId,
    BOOL PreferToSendJRTimestampsToEndpoint,
    BOOL PreferToReceiveJRTimestampsFromEndpoint,
    BYTE PreferredMidiProtocol,
    WORD TimeoutMS
) noexcept
{
    UNREFERENCED_PARAMETER(DeviceInterfaceId);
    UNREFERENCED_PARAMETER(PreferToSendJRTimestampsToEndpoint);
    UNREFERENCED_PARAMETER(PreferToReceiveJRTimestampsFromEndpoint);
    UNREFERENCED_PARAMETER(PreferredMidiProtocol);
    UNREFERENCED_PARAMETER(TimeoutMS);

    // We assume the endpoint manager which calls this function knows whether or not protocol
    // negotiation etc. should happen. So we leave it up to the transport to correctly call
    // this or not, and we just assume it made the correct choice.
    // 
    // For now, we'll do this synchronously, but it should be in a separate thread in the future


    // Create and open a connection to the endpoint, complete with metadata listeners




    // Send discovery request

    // Wait until all metadata arrives or we timeout


    return S_OK;

}



HRESULT 
CMidiEndpointProtocolManager::Cleanup()
{

    return S_OK;
}
