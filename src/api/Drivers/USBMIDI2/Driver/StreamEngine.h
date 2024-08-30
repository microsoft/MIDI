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

    SteamEngine.h - Header file for StreamEngine class used to manage Pins
                    for the driver.

Abstract:

   This file contains the stream Pins and support routines.

Environment:

    Kernel-mode Driver Framework

--*/
// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class StreamEngine;

typedef struct _SINGLE_BUFFER_MAPPING
{
    // address of the memory alloation
    PVOID   m_BufferAddress {nullptr};
    // if true, when this structure is freed the above
    // allocation also needs to be freed.
    BOOL    m_OwnsAllocation {FALSE};

    // only used for kernel mode.
    PVOID   m_MappingAddress {nullptr};
    KPROCESSOR_MODE m_Mode {UserMode};

    // mapped address and mdl
    PVOID   m_BufferClientAddress {nullptr};
    PMDL    m_BufferMdl {nullptr};
} SINGLE_BUFFER_MAPPING, *PSINGLE_BUFFER_MAPPING;

typedef struct _DOUBLE_BUFFER_MAPPING
{
    SINGLE_BUFFER_MAPPING Buffer1;
    SINGLE_BUFFER_MAPPING Buffer2;
} DOUBLE_BUFFER_MAPPING, *PDOUBLE_BUFFER_MAPPING;

class StreamEngine
{
public:

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    StreamEngine(
        _In_ ACXPIN Pin
        );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    virtual
    ~StreamEngine();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    PrepareHardware();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    ReleaseHardware();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    Run();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    Pause();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    GetLoopedStreamingBuffer(
        _In_ ULONG BufferSize,
        _Inout_ PKSMIDILOOPED_BUFFER  Buffer
        );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    GetLoopedStreamingRegisters(
        _Inout_ PKSMIDILOOPED_REGISTERS   Buffer
        );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    SetLoopedStreamingNotificationEvent(
        _In_ PKSMIDILOOPED_EVENT2   Buffer
        );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    NONPAGED_CODE_SEG
    bool
    FillReadStream(
        _In_reads_(bufferSize)      PUINT8              pBuffer,
        _In_                        size_t              bufferSize
    );

private:

    static KSTART_ROUTINE WorkerThread;

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    void
    HandleIo();

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    Cleanup();

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    GetSingleBufferMapping(
        _In_ UINT32 bufferSize,
        _In_ KPROCESSOR_MODE mode,
        _In_ BOOL LockPages,
        _In_opt_ PSINGLE_BUFFER_MAPPING baseMapping,
        _Inout_ PSINGLE_BUFFER_MAPPING mapping
        );

    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    CleanupSingleBufferMapping    (
        _Inout_ PSINGLE_BUFFER_MAPPING mapping
        );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    GetDoubleBufferMapping(
        _In_ UINT32 bufferSize,
        _In_ KPROCESSOR_MODE mode,
        _In_opt_ PSINGLE_BUFFER_MAPPING baseMapping,
        _Inout_ PDOUBLE_BUFFER_MAPPING mapping
        );

    _Must_inspect_result_
    __drv_maxIRQL(PASSIVE_LEVEL)
    PAGED_CODE_SEG
    NTSTATUS
    CleanupDoubleBufferMapping(
        _Inout_ PDOUBLE_BUFFER_MAPPING mapping
        );

private:  
    // Data memebers
    BOOL            m_IsRunning{ FALSE };

    ACXPIN      m_Pin {nullptr};
    ACX_STREAM_STATE m_StreamState {AcxStreamStateStop};

    // The process which acquired the buffer, the user
    // mode buffer is mapped into this processes address space.
    PEPROCESS   m_Process {nullptr};

    // Size of the cyclic buffer allocation
    ULONG       m_BufferSize {0};
    // These contain the double mapping from the buffer to the virtual
    // address spaces for both user mode and kernel mode.
    DOUBLE_BUFFER_MAPPING m_KernelBufferMapping;
    DOUBLE_BUFFER_MAPPING m_ClientBufferMapping;

    // read and write registers which are mapped to the virtual
    // address space for user mode.
    SINGLE_BUFFER_MAPPING m_Registers;
    PULONG      m_ReadRegister {nullptr};
    PULONG      m_WriteRegister {nullptr};

    // event shared with user mode that is signaled whenever
    // data is written into the cyclic buffer by either the driver
    // sending to user mode, or user mode sending to the driver
    PKEVENT     m_WriteEvent {nullptr};

    // event shared with user mode that is signaled whenever
    // data is read from the cyclic buffer by either the driver
    // receiving from user mode, or user mode receiving from the driver
    PKEVENT     m_ReadEvent {nullptr};

    // worker thread that handles IO across the shared memory
    PKTHREAD    m_WorkerThread {nullptr};
    wil::kernel_event_auto_reset   m_ThreadExitEvent;
    wil::kernel_event_manual_reset m_ThreadExitedEvent {true};

    // for standard streaming of messages
    wil::fast_mutex_with_critical_region m_MidiInLock;
};
