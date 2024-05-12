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

    std::wstring EndpointUniqueIdentifier{};

    std::wstring InstanceIdPrefix{};

    std::wstring CreatedShortClientInstanceId{};
    std::wstring CreatedEndpointInterfaceId{};
};
