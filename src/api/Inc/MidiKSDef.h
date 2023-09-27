// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

//
// KsMidiPort properties
// These properties are related to the SWD created for a MIDI port
//
#define STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId L"{2BF8A79A-79FF-49BC-9126-372815B153C8},1"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_KsFilterInterfaceId, 0x2bf8a79a, 0x79ff, 0x49bc, 0x91, 0x26, 0x37, 0x28, 0x15, 0xb1, 0x53, 0xc8, 1);     // DEVPROP_TYPE_STRING

#define STRING_DEVPKEY_KsMidiPort_KsPinId L"{2BF8A79A-79FF-49BC-9126-372815B153C8},2"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_KsPinId, 0x2bf8a79a, 0x79ff, 0x49bc, 0x91, 0x26, 0x37, 0x28, 0x15, 0xb1, 0x53, 0xc8, 2);     // DEVPROP_TYPE_UINT32

#define STRING_DEVPKEY_KsMidiPort_SupportsUMPFormat L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},1"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_SupportsUMPFormat, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 1);     // DEVPROP_TYPE_BOOL

#define STRING_DEVPKEY_KsMidiPort_SupportsMidiOneFormat L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},2"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_SupportsMidiOneFormat, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 2);     // DEVPROP_TYPE_BOOL

#define STRING_DEVPKEY_KsMidiPort_SupportsLooped L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},3"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_SupportsLooped, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 3);     // DEVPROP_TYPE_BOOL

// used for emulating a bidirectional endpoint when using a KS endpoint with separate in and out pins.
#define STRING_DEVPKEY_KsMidiPort_InPinId L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},4"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_InPinId, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 4);     // DEVPROP_TYPE_UINT32

#define STRING_DEVPKEY_KsMidiPort_OutPinId L"{05279CB1-002F-4E6B-A3A3-29A87D82B4F7},5"
DEFINE_DEVPROPKEY(DEVPKEY_KsMidiPort_OutPinId, 0x5279cb1, 0x2f, 0x4e6b, 0xa3, 0xa3, 0x29, 0xa8, 0x7d, 0x82, 0xb4, 0xf7, 5);     // DEVPROP_TYPE_UINT32


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

typedef struct {
    LONGLONG Position;
    ULONG    ByteCount;
} UMPDATAFORMAT, *PUMPDATAFORMAT;

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
    HANDLE WriteEvent;
} KSMIDILOOPED_EVENT, *PKSMIDILOOPED_EVENT;

typedef struct {
    KSPROPERTY  Property;
    ULONG       RequestedBufferSize;
} KSMIDILOOPED_BUFFER_PROPERTY, *PKSMIDILOOPED_BUFFER_PROPERTY;

#endif

