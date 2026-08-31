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
