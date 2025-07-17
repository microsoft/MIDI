// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

//
// KsMidiPort properties
// These properties are related to the SWD created for a MIDI port
//
#define STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId L"{2BF8A79A-79FF-49BC-9126-372815B153C8},1"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_KsFilterInterfaceId, 0x2bf8a79a, 0x79ff, 0x49bc, 0x91, 0x26, 0x37, 0x28, 0x15, 0xb1, 0x53, 0xc8, 1);     // DEVPROP_TYPE_STRING

#define STRING_DEVPKEY_KsMidiPort_KsPinId L"{2BF8A79A-79FF-49BC-9126-372815B153C8},2"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_KsPinId, 0x2bf8a79a, 0x79ff, 0x49bc, 0x91, 0x26, 0x37, 0x28, 0x15, 0xb1, 0x53, 0xc8, 2);     // DEVPROP_TYPE_UINT32

// used for emulating a bidirectional endpoint when using a KS endpoint with separate in and out pins.
#define STRING_DEVPKEY_KsMidiPort_InPinId L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},1"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_InPinId, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 1);     // DEVPROP_TYPE_UINT32

#define STRING_DEVPKEY_KsMidiPort_OutPinId L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},2"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_OutPinId, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 2);     // DEVPROP_TYPE_UINT32

#define STRING_DEVPKEY_KsTransport L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},3"
DEFINE_DEVPROPKEY(DEVPKEY_KsTransport, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 3);     // DEVPROP_TYPE_BYTE


// The binary is KSMIDI_PIN_MAP which contains 16 input and 16 output group maps
// this is used for when we create a UMP BIDI from a set of MIDI in and Out pins on a device
#define STRING_DEVPKEY_KsAggMidiGroupPinMap L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},16"
DEFINE_DEVPROPKEY(DEVPKEY_KsAggMidiGroupPinMap, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 16); // DEVPROP_TYPE_BINARY


typedef enum _MidiTransport
{
    MidiTransport_Invalid = 0,
    MidiTransport_StandardByteStream = 1,
    MidiTransport_CyclicByteStream = 2,
    MidiTransport_StandardAndCyclicByteStream = 3, // bitmask combination of standard and cyclic bytestream
    MidiTransport_CyclicUMP = 4,    
    MidiTransport_StandardByteStreamAndCyclicUMP = 5, // bitmask combination of standard bytestream and cyclic ump
    MidiTransport_CyclicByteStreamAndCyclicUMP = 6, // bitmask combination of cyclic bytestream and cyclic ump
    MidiTransport_StandardAndCyclicByteStreamAndCyclicUMP = 7 // bitmask combination of all transports
} MidiTransport;

#ifndef KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET
#define STATIC_KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET\
            0xfbffd49e, 0xce26, 0x464a, 0x9d, 0xfc, 0xfe, 0xe4, 0x24, 0x56, 0xc8, 0x1c
        DEFINE_GUIDSTRUCT("FBFFD49E-CE26-464A-9DFC-FEE42456C81C", KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET);
#define KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET DEFINE_GUIDNAMED(KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
#endif

#ifndef KSPROPSETID_MidiLoopedStreaming
#define STATIC_KSPROPSETID_MidiLoopedStreaming\
            0x1f306ba6, 0xfd9b, 0x427a, 0xbc, 0xb3, 0x27, 0xcb, 0xcf, 0xe, 0xf, 0x19
        DEFINE_GUIDSTRUCT("1F306BA6-FD9B-427A-BCB3-27CBCF0E0F19", KSPROPSETID_MidiLoopedStreaming);
#define KSPROPSETID_MidiLoopedStreaming DEFINE_GUIDNAMED(KSPROPSETID_MidiLoopedStreaming)

typedef enum {
    KSPROPERTY_MIDILOOPEDSTREAMING_BUFFER,
    KSPROPERTY_MIDILOOPEDSTREAMING_REGISTERS,
    KSPROPERTY_MIDILOOPEDSTREAMING_NOTIFICATION_EVENT,
} KSPROPERTY_MIDILOOPEDSTREAMING;

typedef struct {
    PVOID   BufferAddress;
    ULONG   ActualBufferSize;
} KSMIDILOOPED_BUFFER, *PKSMIDILOOPED_BUFFER;

typedef struct {
    PVOID WritePosition;
    PVOID ReadPosition;
} KSMIDILOOPED_REGISTERS, *PKSMIDILOOPED_REGISTERS;

typedef struct {
    KSPROPERTY  Property;
    ULONG       RequestedBufferSize;
} KSMIDILOOPED_BUFFER_PROPERTY, *PKSMIDILOOPED_BUFFER_PROPERTY;

// Data format for KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET
typedef struct {
    LONGLONG Position;
    ULONG   ByteCount;
} UMPDATAFORMAT, *PUMPDATAFORMAT;
#endif

#ifndef MIDILOOPED_EVENT2
#define MIDILOOPED_EVENT2
typedef struct {
    HANDLE WriteEvent;
    HANDLE ReadEvent;
} KSMIDILOOPED_EVENT2, *PKSMIDILOOPED_EVENT2;
#endif

#ifndef LOOPEDDATAFORMAT
#define LOOPEDDATAFORMAT UMPDATAFORMAT
#define PLOOPEDDATAFORMAT PUMPDATAFORMAT
#endif

#ifndef KSPROPSETID_MIDI2_ENDPOINT_INFORMATION
#define STATIC_KSPROPSETID_MIDI2_ENDPOINT_INFORMATION\
            0x814552d, 0x2a1e, 0x48fe, 0x9f, 0xbd, 0x70, 0xac, 0x67, 0x91, 0x7, 0x55
        DEFINE_GUIDSTRUCT("0814552D-2A1E-48FE-9FBD-70AC67910755", KSPROPSETID_MIDI2_ENDPOINT_INFORMATION);
#define KSPROPSETID_MIDI2_ENDPOINT_INFORMATION DEFINE_GUIDNAMED(KSPROPSETID_MIDI2_ENDPOINT_INFORMATION)

typedef enum {
    KSPROPERTY_MIDI2_NATIVEDATAFORMAT,
    KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,
    KSPROPERTY_MIDI2_SERIAL_NUMBER,
    KSPROPERTY_MIDI2_DEVICE_MANUFACTURER,
    KSPROPERTY_MIDI2_DEVICE_VID,
    KSPROPERTY_MIDI2_DEVICE_PID,
} KSPROPERTY_MIDI2_ENDPOINT_INFORMATION;
#endif

