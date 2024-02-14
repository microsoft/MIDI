// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once


// change / extend as necessary. You may need things like IP address
// or whether it's a server or client port, or other information 
// captured in here to able to be used when creating a network 
// MIDI device in device manager. Some of this may come from the
// configuration files, and others may come from the network during
// the initial session creation.
struct MidiNetworkDeviceDefinition
{
    std::wstring AssociationId{};

    std::wstring EndpointName{};
    std::wstring EndpointDescription{};

    std::wstring EndpointUniqueIdentifier{};

    std::wstring InstanceIdPrefix{};

    std::wstring CreatedShortClientInstanceId{};
    std::wstring CreatedEndpointInterfaceId{};
};
