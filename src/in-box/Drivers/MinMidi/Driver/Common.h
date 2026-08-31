// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <ntifs.h>
#include <wdm.h>
#include <windef.h>
#include <ks.h>
#include <ksmedia.h>
#include <wil\resource.h>
#include <ntintsafe.h>

#include "NewDelete.h"

#include "MidiKSDef.h"

#define MM_MAPPING_ADDRESS_DIVISIBLE 0x1

#define PAGED_CODE_SEG __declspec(code_seg("PAGE"))
#define INIT_CODE_SEG __declspec(code_seg("INIT"))
#define NONPAGED_CODE_SEG __declspec(code_seg("NOPAGE"))

#define NT_RETURN_IF_NTSTATUS_FAILED(status) do {\
        if(!NT_SUCCESS(status)) { \
            return status; \
        } \
    } while(0)


#define NT_RETURN_NTSTATUS_IF(status, expression) do {\
    if(expression) { \
        return status; \
    } \
} while(0)

// Group Terminal Block structure defines
#pragma pack(push)
#pragma pack(1)

#define MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL 0x00
#define MIDI_GROUP_TERMINAL_BLOCK_INPUT         0x01
#define MIDI_GROUP_TERMINAL_BLOCK_OUTPUT        0x02

/* Based on current definitions in USB Class Definition for MIDI Devices Release 2.0

For Group Terminal Block Type (sect. A.6)
BIDIRECTIONAL                       0x00
INPUT_ONLY                          0x01
OUTPUT_ONLY                         0x02

For Group Terminal Default MIDI Protocol (sect. A.7)
USE_MIDI_CI                         0x00
MIDI_1_0_UP_TO_64_BITS              0x01
MIDI_1_0_UP_TO_64_BITS_AND_JRTS     0x02
MIDI_1_0_UP_TO_128_BITS             0x03
MIDI_1_0_UP_TO_128_BITS_AND_JRTS    0x04
MIDI_2_0                            0x11
MIDI_2_0_AND_JRTS                   0x12

Group Terminal Number
GROUP_1                             0x00
GROUP_2                             0x01
GROUP_3                             0x02
...
GROUP_16                            0x0F
*/

typedef struct
{
    WORD                            Size;
    BYTE                            Number;             // Group Terminal Block ID
    BYTE                            Direction;          // Group Terminal Block Type
    BYTE                            FirstGroupIndex;    // The first group in this block
    BYTE                            GroupCount;         // The number of groups spanned
    BYTE                            Protocol;           // The MIDI Protocol
    WORD                            MaxInputBandwidth;  ///< Maximum Input Bandwidth Capability in 4KB/second
    WORD                            MaxOutputBandwidth; ///< Maximum Output Bandwidth Capability in 4KB/second
} UMP_GROUP_TERMINAL_BLOCK_HEADER;

typedef struct
{
    UMP_GROUP_TERMINAL_BLOCK_HEADER GrpTrmBlock;
    WCHAR                           Name[1];             // NULL Terminated string, blank indicates none available
                                                        // from USB Device
} UMP_GROUP_TERMINAL_BLOCK_DEFINITION, *PUMP_GROUP_TERMINAL_BLOCK_DEFINITION;

#pragma pack(pop)

