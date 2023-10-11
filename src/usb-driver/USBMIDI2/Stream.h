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

    Steam.h - Header for routines for creating and establishing ACX Circuit

Abstract:

   This file contains ACX support routines for Circuit.

Environment:

    Kernel-mode Driver Framework

--*/
/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Stream.h

Abstract:

    Contains structure definitions and function prototypes private to
    the driver.

Environment:

    Kernel mode

--*/

#ifndef _STREAM_H_
#define _STREAM_H_

class StreamEngine;

/* make prototypes usable from C++ */
#ifdef __cplusplus
extern "C" {
#endif

//
// Define stream context.
//
typedef struct _MIDI_STREAM_CONTEXT {
    StreamEngine *      StreamEngine;
    PEPROCESS           Process;
    ACXPIN              Pin;
    bool                IsCounted;
} MIDI_STREAM_CONTEXT, *PMIDI_STREAM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MIDI_STREAM_CONTEXT, GetMidiStreamContext)

// Stream callbacks 

EVT_WDF_OBJECT_CONTEXT_DESTROY      EvtStreamDestroy;
EVT_WDF_DEVICE_CONTEXT_CLEANUP      EvtStreamCleanup;
EVT_ACX_STREAM_PREPARE_HARDWARE     EvtStreamPrepareHardware;
EVT_ACX_STREAM_RELEASE_HARDWARE     EvtStreamReleaseHardware;
EVT_ACX_STREAM_RUN                  EvtStreamRun;
EVT_ACX_STREAM_PAUSE                EvtStreamPause;

EVT_ACX_OBJECT_PROCESS_REQUEST      EvtMidiGetLoopedStreamingBufferCallback;
EVT_ACX_OBJECT_PROCESS_REQUEST      EvtMidiGetLoopedStreamingRegistersCallback;
EVT_ACX_OBJECT_PROCESS_REQUEST      EvtMidiSetLoopedStreamingNotificationEventCallback;

EVT_ACX_OBJECT_PROCESS_REQUEST      EvtMidi2NativeDataFormatCallback;
EVT_ACX_OBJECT_PROCESS_REQUEST      EvtMidi2GroupTerminalBlocksCallback;

/* make internal prototypes usable from C++ */
#ifdef __cplusplus
}
#endif

_Must_inspect_result_
__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
AttachToCallerProcess(
    _In_        WDFREQUEST        Request,
    _Out_       PRKAPC_STATE      ApcState,
    _Out_       BOOLEAN         * Attached,
    _Out_opt_   PEPROCESS       * Process
    );

__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
VOID
AttachToProcess(
    _In_    PEPROCESS       Process,
    _Out_   PRKAPC_STATE    ApcState,
    _Out_   BOOLEAN       * Attached
    );

__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
VOID
RestoreProcess(
    _In_    PRKAPC_STATE    ApcState
    );

#endif // _STREAM_H_
