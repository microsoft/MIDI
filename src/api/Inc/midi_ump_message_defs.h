// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


// this is just to make intent clear
#define MIDI_RESERVED_WORD (uint32_t)0
#define MIDI_RESERVED_FIELD 0

#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY                           (uint16_t)0x000
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION                   (uint16_t)0x001
#define MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION                 (uint16_t)0x002
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION                   (uint16_t)0x003
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION    (uint16_t)0x004
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST                 (uint16_t)0x005
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION            (uint16_t)0x006

#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_DISCOVERY                     (uint16_t)0x010
#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION             (uint16_t)0x011
#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION             (uint16_t)0x012

#define MIDI_STREAM_MESSAGE_STANDARD_FORM0      0x0

#define MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE 0x0
#define MIDI_STREAM_MESSAGE_MULTI_FORM_START    0x1
#define MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE 0x2
#define MIDI_STREAM_MESSAGE_MULTI_FORM_END      0x3

#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH 98
#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET 14

#define MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_NAME_MAX_LENGTH 91
#define MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_NAME_CHARACTERS_PER_PACKET 13

#define MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_MAX_LENGTH 42
#define MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_CHARACTERS_PER_PACKET 14



