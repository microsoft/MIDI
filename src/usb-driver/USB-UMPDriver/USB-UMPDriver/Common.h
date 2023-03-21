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

    Common.h

Abstract:

    This file contains the Common definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

#include <ntddk.h>
#include "usbdi.h"
#include "usbdlib.h"

#include <wdf.h>
#include <wdfusb.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "trace.h"

#define USBUMP_POOLTAG 'UPMU'

#define USB_REQ_TIMEOUT_SEC     7

FORCEINLINE
GUID
DeviceToActivityId(
    _In_ WDFDEVICE Device
)
{
    GUID activity = { 0 };
    RtlCopyMemory(&activity, &Device, sizeof(WDFDEVICE));
    return activity;
}


// Defines for working with USB MIDI 1.0 used for UMP <-> USB MIDI 1.0 conversions
#define MIDI_CIN_MASK       (UCHAR)0x0f
typedef enum MIDI_CIN_TYPES_t
{
    MIDI_CIN_MISC = 0,
    MIDI_CIN_CABLE_EVENT = 1,
    MIDI_CIN_SYSCOM_2BYTE = 2, // 2 byte system common message e.g MTC, SongSelect
    MIDI_CIN_SYSCOM_3BYTE = 3, // 3 byte system common message e.g SPP
    MIDI_CIN_SYSEX_START = 4, // SysEx starts or continue
    MIDI_CIN_SYSEX_END_1BYTE = 5, // SysEx ends with 1 data, or 1 byte system common message
    MIDI_CIN_SYSEX_END_2BYTE = 6, // SysEx ends with 2 data
    MIDI_CIN_SYSEX_END_3BYTE = 7, // SysEx ends with 3 data
    MIDI_CIN_NOTE_ON = 8,
    MIDI_CIN_NOTE_OFF = 9,
    MIDI_CIN_POLY_KEYPRESS = 10,
    MIDI_CIN_CONTROL_CHANGE = 11,
    MIDI_CIN_PROGRAM_CHANGE = 12,
    MIDI_CIN_CHANNEL_PRESSURE = 13,
    MIDI_CIN_PITCH_BEND_CHANGE = 14,
    MIDI_CIN_1BYTE_DATA = 15
} MIDI_CIN_Type;

// MIDI 1.0 status byte
enum
{
    //------------- System Exclusive -------------//
    MIDI_STATUS_SYSEX_START = 0xF0,
    MIDI_STATUS_SYSEX_END = 0xF7,

    //------------- System Common -------------//
    MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME = 0xF1,
    MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER = 0xF2,
    MIDI_STATUS_SYSCOM_SONG_SELECT = 0xF3,
    // F4, F5 is undefined
    MIDI_STATUS_SYSCOM_TUNE_REQUEST = 0xF6,

    //------------- System RealTime  -------------//
    MIDI_STATUS_SYSREAL_TIMING_CLOCK = 0xF8,
    // 0xF9 is undefined
    MIDI_STATUS_SYSREAL_START = 0xFA,
    MIDI_STATUS_SYSREAL_CONTINUE = 0xFB,
    MIDI_STATUS_SYSREAL_STOP = 0xFC,
    // 0xFD is undefined
    MIDI_STATUS_SYSREAL_ACTIVE_SENSING = 0xFE,
    MIDI_STATUS_SYSREAL_SYSTEM_RESET = 0xFF,
};

