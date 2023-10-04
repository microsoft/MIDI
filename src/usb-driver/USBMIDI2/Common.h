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

// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#define MM_MAPPING_ADDRESS_DIVISIBLE 0x1
#define USB_REQ_TIMEOUT_SEC     7

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
} UMPDATAFORMAT, * PUMPDATAFORMAT;

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

#define PAGED_CODE_SEG __declspec(code_seg("PAGE"))
#define INIT_CODE_SEG __declspec(code_seg("INIT"))

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

