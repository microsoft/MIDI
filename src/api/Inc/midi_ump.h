// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <stdint.h>

// NOTE: Timestamp was removed from the UMP Payload because it's a separate parameter to the service calls

namespace WindowsMidiServicesInternal
{
    // standard timestamp
    using MidiTimestamp = uint64_t;

    // just a struct helper
    #define MIDIWORDALIGNEDSTRUCT __declspec(align(sizeof(uint32_t))) struct

    // structs must be aligned on 32 bit boundaries, not the default 64
    #pragma pack(push, 4)
        MIDIWORDALIGNEDSTRUCT PackedUmp32
        {
            //uint64_t timestamp;
            uint32_t word0 {0};
        };
        MIDIWORDALIGNEDSTRUCT PackedUmp64
        {
            //uint64_t timestamp;
            uint32_t word0 {0};
            uint32_t word1 {0};
        };
        MIDIWORDALIGNEDSTRUCT PackedUmp96
        {
            //uint64_t timestamp;
            uint32_t word0 {0};
            uint32_t word1 {0};
            uint32_t word2 {0};
        };
        MIDIWORDALIGNEDSTRUCT PackedUmp128
        {
            //uint64_t timestamp;
            uint32_t word0 {0};
            uint32_t word1 {0};
            uint32_t word2 {0};
            uint32_t word3 {0};
        };

        // could reuse packedump128, but this is more clear
        MIDIWORDALIGNEDSTRUCT PackedMaxInternalUmpStorage
        {
            uint32_t Word0 {0};
            uint32_t Word1 {0};
            uint32_t Word2 {0};
            uint32_t Word3 {0};
        };

    #pragma pack(pop)

    // helper function to check the size of a buffer of memory
    inline bool IsValidSingleUmpBufferSize(_In_ uint32_t const size)
    {
        return (size == sizeof(PackedUmp32) ||
            size == sizeof(PackedUmp64) ||
            size == sizeof(PackedUmp96) ||
            size == sizeof(PackedUmp128)
            );
    }

    inline bool IsValidSingleUmpWordCount(_In_ uint32_t const wordCount)
    {
        return (wordCount >= 1 && wordCount <= 4);
    }

    inline bool FillPackedUmp32FromBytePointer(_In_reads_bytes_(byteCount) uint8_t* data, _In_ uint8_t byteCount, _Inout_ PackedUmp32& ump)
    {
        if (byteCount == sizeof(ump))
        {
            memcpy(&ump, data, byteCount);

            return true;
        }

        return false;
    }

    inline bool FillPackedUmp64FromBytePointer(_In_reads_bytes_(byteCount) uint8_t* data, _In_ uint8_t byteCount, _Inout_ PackedUmp64& ump)
    {
        if (byteCount == sizeof(ump))
        {
            memcpy(&ump, data, byteCount);

            return true;
        }

        return false;
    }

    inline bool FillPackedUmp96FromBytePointer(_In_reads_bytes_(byteCount) uint8_t* data, _In_ uint8_t byteCount, _Inout_ PackedUmp96& ump)
    {
        if (byteCount == sizeof(ump))
        {
            memcpy(&ump, data, byteCount);

            return true;
        }

        return false;
    }

    inline bool FillPackedUmp128FromBytePointer(_In_reads_bytes_(byteCount) uint8_t* data, _In_ uint8_t byteCount, _Inout_ PackedUmp128& ump)
    {
        if (byteCount == sizeof(ump))
        {
            memcpy(&ump, data, byteCount);

            return true;
        }

        return false;
    }
}
