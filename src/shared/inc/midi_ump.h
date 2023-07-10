#pragma once

#include <stdint.h>

namespace Windows::Devices::Midi2::Internal::Shared
{
    #define MIDIWORDALIGNEDSTRUCT __declspec(align(sizeof(uint32_t))) struct

    #pragma pack(push, 4)
        MIDIWORDALIGNEDSTRUCT PackedUmp32
        {
            uint64_t timestamp;
            uint32_t word0;
        };
        MIDIWORDALIGNEDSTRUCT PackedUmp64
        {
            uint64_t timestamp;
            uint32_t word0;
            uint32_t word1;
        };
        MIDIWORDALIGNEDSTRUCT PackedUmp96
        {
            uint64_t timestamp;
            uint32_t word0;
            uint32_t word1;
            uint32_t word2;
        };
        MIDIWORDALIGNEDSTRUCT PackedUmp128
        {
            uint64_t timestamp;
            uint32_t word0;
            uint32_t word1;
            uint32_t word2;
            uint32_t word3;
        };
    #pragma pack(pop)
}