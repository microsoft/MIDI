#pragma once

#include <stdint.h>

// NOTE: Timestamp was removed from the UMP Payload because it's a separate parameter to the service calls

namespace Windows::Devices::Midi2::Internal
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


}