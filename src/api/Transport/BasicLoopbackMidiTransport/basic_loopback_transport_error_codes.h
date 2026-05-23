// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include <cstdint>

enum BasicLoopbackTransportServiceErrorCode : uint32_t
{
    UnknownOrUnspecified =     0x00000000,
    UnrecognizedCommand =      0x00000001,

    InvalidJson =              0x00000011,

    EndpointCreationFailed =   0x00000021,

    DuplicateUniqueId =        0x00000031,
    MissingUniqueId =          0x00000032, 
    MissingEndpointName =      0x00000033,
    MissingAssociationId =     0x00000034,
    EndpointNotFound =         0x00000035,

};



