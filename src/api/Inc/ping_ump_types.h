// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// This is using a proprietary Ping message functionality that has not been standardized,
// and which may never be implemented, or may change, so we need to keep this 
// implementation internal to the service and API and this specific use case.
// If a formal ping type is adopted, we'll use that.

// This transport doesn't accept any other type of message, so risk is low here

// 4 bits Message Type 0xF - 128 bit message
// 2 bits Form = 0
// 10 bits Status = (0x3FE for request, 0x3FD for reply)
// 16 bits padding to fill out first word

// So first word for request is 0xF3FE0000, reply is 0xF3FD0000

// Everything after that is the random number ID for this ping, but for simplicity, we 
// only use the first 64 bits represented by the second and third words

#define INTERNAL_PING_REQUEST_UMP_WORD0 0xF3FE0000
#define INTERNAL_PING_RESPONSE_UMP_WORD0 0xF3FD0000

namespace WindowsMidiServicesInternal
{
    #pragma pack(push, 4)

    struct PackedPingRequestUmp
    {
        std::uint32_t Word0 { INTERNAL_PING_REQUEST_UMP_WORD0 };
        std::uint64_t PingId{ 0 };
        std::uint32_t Padding { 0 };

    };

    struct PackedPingResponseUmp
    {
        std::uint32_t Word0 { INTERNAL_PING_RESPONSE_UMP_WORD0 };
        std::uint64_t PingId;
        std::uint32_t Padding { 0 };
    };

    #pragma pack(pop)  
}

