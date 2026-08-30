// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// This information is provided by the configuration manager


struct MidiLoopbackDeviceDefinition
{
    std::wstring AssociationId{};

    std::wstring EndpointName{};
    std::wstring EndpointDescription{};

    // bare file name in the shared endpoint assets folder, never a path
    std::wstring ImageFileName{};

    std::wstring EndpointUniqueIdentifier{};

    std::wstring InstanceIdPrefix{};

    std::wstring CreatedShortClientInstanceId{};
    std::wstring CreatedEndpointInterfaceId{};

    bool UMPOnly{ false };

    // what the configuration asked for, so a loopback which was saved muted comes back muted
    bool IsMuted{ false };

};
