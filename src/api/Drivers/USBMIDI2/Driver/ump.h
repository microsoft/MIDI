/************************************************************************************
Copyright 2023 Association of Musical Electronics Industry
Copyright 2023 Microsoft
Driver source code developed by AmeNote. Some components Copyright 2023 AmeNote Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
************************************************************************************/
/*++

Module Name:

    ump.h

Abstract:

   This file contains definitions of the UMP protocol used by driver. It is adapted
   from the tusb_ump library with permission and based on the notices following.

Environment:

    Kernel-mode Driver Framework

--*/

/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2022 Michael Loh (AmeNote.com)
 * Copyright (c) 2022 Franz Detro (native-instruments.de)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

/** \ingroup group_class
 *  \defgroup ClassDriver_CDC Communication Device Class (CDC)
 *            Currently only Abstract Control Model subclass is supported
 *  @{ */

#pragma once

#include <ntdef.h>

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// UMP Protocol definitions
//--------------------------------------------------------------------+
// Message Types
#define UMP_MT_MASK         0xf0
#define UMP_MT_UTILITY      0x00
#define UMP_MT_SYSTEM       0x10
#define UMP_MT_MIDI1_CV     0x20
#define UMP_MT_DATA_64      0x30
#define UMP_MT_MIDI2_CV     0x40
#define UMP_MT_DATA_128     0x50
#define UMP_MT_RESERVED_6   0x60 // 32bits reserved future
#define UMP_MT_RESERVED_7   0x70 // 32bits reserved future
#define UMP_MT_RESERVED_8   0x80 // 64bits reserved future
#define UMP_MT_RESERVED_9   0x90 // 64bits reserved future
#define UMP_MT_RESERVED_A   0xA0 // 64bits reserved future
#define UMP_MT_RESERVED_B   0xB0 // 96bits reserved future
#define UMP_MT_RESERVED_C   0xC0 // 96bits reserved future
#define UMP_MT_FLEX_128     0xd0
#define UMP_MT_RESERVED_E   0xE0 // 128bits reserved future
#define UMP_MT_STREAM_128   0xf0

// Group Number
#define UMP_GROUP_MASK    0x0f

// System Exclusive 7-Bit Status
#define UMP_SYSEX7_STATUS_MASK  0xf0
#define UMP_SYSEX7_COMPLETE     0x00
#define UMP_SYSEX7_START        0x10
#define UMP_SYSEX7_CONTINUE     0x20
#define UMP_SYSEX7_END          0x30
#define UMP_SYSEX7_SIZE_MASK    0x0f

// System Common Status
#define UMP_SYSTEM_MTC          0xf1  // 2 bytes incl status
#define UMP_SYSTEM_SONG_POS_PTR 0xf2  // 3 bytes incl status
#define UMP_SYSTEM_SONG_SELECT  0xf3  // 2 bytes incl status
#define UMP_SYSTEM_UNDEFINED_F4 0xf4  // undefined
#define UMP_SYSTEM_UNDEFINED_F5 0xf5  // undefined
#define UMP_SYSTEM_TUNE_REQ     0xf6  // status byte only
#define UMP_SYSTEM_TIMING_CLK   0xf8  // status byte only
#define UMP_SYSTEM_UNDEFINED_F9 0xf9  // undefined
#define UMP_SYSTEM_START        0xfa  // status byte only
#define UMP_SYSTEM_CONTINUE     0xfb  // status byte only
#define UMP_SYSTEM_STOP         0xfc  // status byte only
#define UMP_SYSTEM_UNDEFINED_FD 0xfd  // undefined
#define UMP_SYSTEM_ACTIVE_SENSE 0xfe  // status byte only
#define UMP_SYSTEM_RESET        0xff  // status byte only

//--------------------------------------------------------------------+
// Class Specific Descriptor
//--------------------------------------------------------------------+

typedef enum
{
    MIDI_CS_INTERFACE = 0x24,
    MIDI_CS_ENDPOINT = 0x25,
    MIDI_CS_INTERFACE_GR_TRM_BLOCK = 0x26,
} midi_cs_interface_types;

typedef enum
{
  MIDI_CS_DESCRIPTOR_UNDEFINED= 0x00,
  MIDI_CS_INTERFACE_HEADER    = 0x01,
  MIDI_CS_INTERFACE_IN_JACK   = 0x02,
  MIDI_CS_INTERFACE_OUT_JACK  = 0x03,
  MIDI_CS_INTERFACE_ELEMENT   = 0x04,
} midi_cs_interface_subtypes;

#define MIDI_CS_BCD_MIDI1       0x0100
#define MIDI_CS_BCD_MIDI2       0x0200

typedef enum
{
  MIDI_CS_ENDPOINT_GENERAL = 0x01,
  MIDI20_CS_ENDPOINT_GENERAL = 0x02,
} midi_cs_endpoint_subtype_t;

typedef enum
{
  MIDI_JACK_EMBEDDED = 0x01,
  MIDI_JACK_EXTERNAL = 0x02
} midi_jack_type_t;

typedef enum
{
  MIDI_GR_TRM_BLOCK_HEADER = 0x01,
  MIDI_GR_TRM_BLOCK = 0x02
} midi_group_terminal_block_type_t;

typedef enum
{
  MIDI_CIN_MISC              = 0,
  MIDI_CIN_CABLE_EVENT       = 1,
  MIDI_CIN_SYSCOM_2BYTE      = 2, // 2 byte system common message e.g MTC, SongSelect
  MIDI_CIN_SYSCOM_3BYTE      = 3, // 3 byte system common message e.g SPP
  MIDI_CIN_SYSEX_START       = 4, // SysEx starts or continue
  MIDI_CIN_SYSEX_END_1BYTE   = 5, // SysEx ends with 1 data, or 1 byte system common message
  MIDI_CIN_SYSEX_END_2BYTE   = 6, // SysEx ends with 2 data
  MIDI_CIN_SYSEX_END_3BYTE   = 7, // SysEx ends with 3 data
  MIDI_CIN_NOTE_ON           = 8,
  MIDI_CIN_NOTE_OFF          = 9,
  MIDI_CIN_POLY_KEYPRESS     = 10,
  MIDI_CIN_CONTROL_CHANGE    = 11,
  MIDI_CIN_PROGRAM_CHANGE    = 12,
  MIDI_CIN_CHANNEL_PRESSURE  = 13,
  MIDI_CIN_PITCH_BEND_CHANGE = 14,
  MIDI_CIN_1BYTE_DATA = 15
} midi_code_index_number_t;

// MIDI 1.0 status byte
enum
{
  //------------- System Exclusive -------------//
  MIDI_STATUS_SYSEX_START                    = 0xF0,
  MIDI_STATUS_SYSEX_END                      = 0xF7,

  //------------- System Common -------------//
  MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME = 0xF1,
  MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER   = 0xF2,
  MIDI_STATUS_SYSCOM_SONG_SELECT             = 0xF3,
  // F4, F5 is undefined
  MIDI_STATUS_SYSCOM_TUNE_REQUEST            = 0xF6,

  //------------- System RealTime  -------------//
  MIDI_STATUS_SYSREAL_TIMING_CLOCK           = 0xF8,
  // 0xF9 is undefined
  MIDI_STATUS_SYSREAL_START                  = 0xFA,
  MIDI_STATUS_SYSREAL_CONTINUE               = 0xFB,
  MIDI_STATUS_SYSREAL_STOP                   = 0xFC,
  // 0xFD is undefined
  MIDI_STATUS_SYSREAL_ACTIVE_SENSING         = 0xFE,
  MIDI_STATUS_SYSREAL_SYSTEM_RESET           = 0xFF,
};

/// MIDI Interface Header Descriptor
#pragma pack(push)
#pragma pack(1)
typedef struct
{
    UINT8     bLength; ///< Size of this descriptor in bytes.
    UINT8     bDescriptorType; ///< Descriptor Type, must be Class-Specific
    UINT8     bDescriptorSubType; ///< Descriptor SubType
} midi_desc_header_common_t;

typedef struct
{
  UINT8     bLength            ; ///< Size of this descriptor in bytes.
  UINT8     bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  UINT8     bDescriptorSubType ; ///< Descriptor SubType
  UINT16 bcdMSC            ; ///< MidiStreaming SubClass release number in Binary-Coded Decimal
  UINT16 wTotalLength      ;
} midi_desc_header_t;

/// MIDI In Jack Descriptor
typedef struct
{
  UINT8 bLength            ; ///< Size of this descriptor in bytes.
  UINT8 bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  UINT8 bDescriptorSubType ; ///< Descriptor SubType
  UINT8 bJackType          ; ///< Embedded or External
  UINT8 bJackID            ; ///< Unique ID for MIDI IN Jack
  UINT8 iJack              ; ///< string descriptor
} midi_desc_in_jack_t;


/// MIDI Out Jack Descriptor with single pin
typedef struct
{
  UINT8 bLength            ; ///< Size of this descriptor in bytes.
  UINT8 bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  UINT8 bDescriptorSubType ; ///< Descriptor SubType
  UINT8 bJackType          ; ///< Embedded or External
  UINT8 bJackID            ; ///< Unique ID for MIDI IN Jack
  UINT8 bNrInputPins;

  UINT8 baSourceID;
  UINT8 baSourcePin;

  UINT8 iJack              ; ///< string descriptor
} midi_desc_out_jack_t ;

/// MIDI Out Jack Descriptor with multiple pins
#define midi_desc_out_jack_n_t(input_num) \
  struct TU_ATTR_PACKED { \
    UINT8 bLength            ; \
    UINT8 bDescriptorType    ; \
    UINT8 bDescriptorSubType ; \
    UINT8 bJackType          ; \
    UINT8 bJackID            ; \
    UINT8 bNrInputPins       ; \
    struct TU_ATTR_PACKED {      \
        UINT8 baSourceID;      \
        UINT8 baSourcePin;     \
    } pins[input_num];           \
   UINT8 iJack              ;  \
  }

/// MIDI Element Descriptor
typedef struct
{
  UINT8 bLength            ; ///< Size of this descriptor in bytes.
  UINT8 bDescriptorType    ; ///< Descriptor Type, must be Class-Specific
  UINT8 bDescriptorSubType ; ///< Descriptor SubType
  UINT8 bElementID;

  UINT8 bNrInputPins;
  UINT8 baSourceID;
  UINT8 baSourcePin;

  UINT8 bNrOutputPins;
  UINT8 bInTerminalLink;
  UINT8 bOutTerminalLink;
  UINT8 bElCapsSize;

  UINT16 bmElementCaps;
  UINT8  iElement;
} midi_desc_element_t;

/// MIDI Element Descriptor with multiple pins
#define midi_desc_element_n_t(input_num) \
  struct TU_ATTR_PACKED {       \
    UINT8 bLength;            \
    UINT8 bDescriptorType;    \
    UINT8 bDescriptorSubType; \
    UINT8 bElementID;         \
    UINT8 bNrInputPins;       \
    struct TU_ATTR_PACKED {     \
        UINT8 baSourceID;     \
        UINT8 baSourcePin;    \
    } pins[input_num];          \
    UINT8 bNrOutputPins;      \
    UINT8 bInTerminalLink;    \
    UINT8 bOutTerminalLink;   \
    UINT8 bElCapsSize;        \
    UINT16 bmElementCaps;     \
    UINT8  iElement;          \
 }

/// MIDI 2 Streaming Data Endpoint Descriptor with one group terminal block
typedef struct
{
  UINT8 bLength            ; ///< Size of this descriptor in bytes: 4+n
  UINT8 bDescriptorType    ; ///< Descriptor Type: CS_ENDPOINT
  UINT8 bDescriptorSubType ; ///< Descriptor SubType: MIDI20_CS_ENDPOINT_GENERAL
  UINT8 bNumGrpTrmBlock    ; ///< Number of Group Terminal Blocks: 1
  UINT8 bAssoGrpTrmBlkID   ; ///< ID of the Group Terminal Block that is associated with this endpoint
} midi2_desc_streaming_data_endpoint_t;

/// MIDI 2 Streaming Data Endpoint Descriptor with multiple group terminal blocks
#define midi2_desc_streaming_data_endpoint_n_t(group_terminal_block_num) \
  struct {       \
    UINT8 bLength;            \
    UINT8 bDescriptorType;    \
    UINT8 bDescriptorSubType; \
    UINT8 bNumGrpTrmBlock;    \
    UINT8 bNrInputPins;       \
    UINT8 baAssoGrpTrmBlkID[group_terminal_block_num]; \
  }

/// MIDI 2 Group Terminal Block Header Descriptor
typedef struct
{
  UINT8  bLength            ; ///< Size of this descriptor in bytes: 5
  UINT8  bDescriptorType    ; ///< Descriptor Type: MIDI_CS_INTERFACE_GR_TRM_BLOCK
  UINT8  bDescriptorSubType ; ///< Descriptor SubType: MIDI_GR_TRM_BLOCK_HEADER
  UINT16 wTotalLength       ; ///< Total number of bytes returned for the class-specific Group Terminal Block descriptors. Includes the combined length of this header descriptor and all Group Terminal Block descriptors.
} midi2_desc_group_terminal_block_header_t;

/// MIDI 2 Group Terminal Block Descriptor
typedef struct
{
  UINT8  bLength            ; ///< Size of this descriptor in bytes: 13
  UINT8  bDescriptorType    ; ///< Descriptor Type: MIDI_CS_INTERFACE_GR_TRM_BLOCK
  UINT8  bDescriptorSubType ; ///< Descriptor SubType: MIDI_GR_TRM_BLOCK
  UINT8  bGrpTrmBlkID       ; ///< ID of this Group Terminal Block
  UINT8  bGrpTrmBlkType     ; ///< Group Terminal Block Type
  UINT8  nGroupTrm          ; ///< The first member Group Terminal in this Block
  UINT8  nNumGroupTrm       ; ///< Number of member Group Terminals spanned
  UINT8  iBlockItem         ; ///< ID of STRING descriptor for UI representation of Block item
  UINT8  bMIDIProtocol      ; ///< Default MIDI protocol
  UINT16 wMaxInputBandwidth ; ///< Maximum Input Bandwidth Capability in 4KB/second
  UINT16 wMaxOutputBandwidth; ///< Maximum Output Bandwidth Capability in 4KB/second
} midi2_desc_group_terminal_block_t;

/// MIDI 2 Group Terminal Blocks Descriptor with multiple terminal blocks
typedef struct {
    midi2_desc_group_terminal_block_header_t header;
    midi2_desc_group_terminal_block_t aBlock[1];
} midi2_cs_interface_desc_group_terminal_blocks;

#pragma pack(pop)

/** @} */

#ifdef __cplusplus
 }
#endif

/** @} */
