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

    SteamEngine.cpp - Class code file for StreamEngine class used to manage Pins
                      for the driver.

Abstract:

   This file contains the stream Pins and support routines.

Environment:

    Kernel-mode Driver Framework

--*/
// Copyright (c) Microsoft Corporation. All rights reserved.
#include "Pch.h"

#include "Trace.h"
#include "StreamEngine.tmh"

// UMP 32 is 4 bytes
#define MINIMUM_UMP_DATASIZE 4

// UMP 128 is 16 bytes
#define MAXIMUM_UMP_DATASIZE 16

// maximum buffer size is 0x100 pages.
#define MAXIMUM_LOOPED_BUFFER_SIZE (PAGE_SIZE * 0x100)
static_assert(    MAXIMUM_LOOPED_BUFFER_SIZE < ULONG_MAX/2, "The maximum looped buffer size may not exceed 1/2 MAX_ULONG");

_Use_decl_annotations_
StreamEngine::StreamEngine(
    _In_ ACXPIN Pin
    ) : m_Pin(Pin)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! create %p", this);
}

_Use_decl_annotations_
StreamEngine::~StreamEngine()
{
    PAGED_CODE();
    Cleanup();
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::Cleanup()
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Entry %p", this);

    m_Process = nullptr;

    // shut down the worker thread and wait for it to close,
    // if it exists.
    if (m_WorkerThread)
    {
        m_ThreadExitEvent.set();
        m_ThreadExitedEvent.wait();
        KeWaitForSingleObject(m_WorkerThread, Executive, KernelMode, FALSE, nullptr);
        ObDereferenceObject(m_WorkerThread);
        m_WorkerThread = nullptr;
    }

    // unmap and free the registers that are mapped to user mode,
    // if they exist.
    CleanupSingleBufferMapping(&m_Registers);
    m_ReadRegister = nullptr;
    m_WriteRegister = nullptr;

    // unmap and free the buffer that is double mapped to user mode
    // and kernel mode, if the buffer exists and is mapped.
    CleanupDoubleBufferMapping(&m_KernelBufferMapping);
    CleanupDoubleBufferMapping(&m_ClientBufferMapping);

    // clean up the write event that was provided to us by user mode.
    if (m_WriteEvent)
    {
        ObDereferenceObject(m_WriteEvent);
        m_WriteEvent = nullptr;
    }

    // clean up the read event that was provided to us by user mode.
    if (m_ReadEvent)
    {
        ObDereferenceObject(m_ReadEvent);
        m_ReadEvent = nullptr;
    }

    if (m_Process)
    {
        ASSERT(m_Process == IoGetCurrentProcess());
        m_Process = nullptr;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
void
StreamEngine::WorkerThread(
    PVOID context
    )
{
    auto streamEngine = reinterpret_cast<StreamEngine*>(context);
    streamEngine->HandleIo();
}

NONPAGED_CODE_SEG
void
StreamEngine::HandleIo()
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // This function handles both sending and receiving midi messages
    // when cyclic buffering is being used.
    // This implememtation loops the midi out data back to midi in.
    NTSTATUS status = STATUS_SUCCESS;

    // start with the even reset to indicate that the thread is running
    m_ThreadExitedEvent.clear();

    if (AcxPinGetId(m_Pin) == MidiPinTypeMidiOut)
    {
        // application is sending a midi message out, this is traditionally called
        // midi out.            
        PVOID waitObjects[] = { m_ThreadExitEvent.get(), m_WriteEvent };

        do
        {
            // run until we get a thread exit event.
            // wait for either a write event indicating data is ready to move, or thread exit.
            status = KeWaitForMultipleObjects(2, waitObjects, WaitAny, Executive, KernelMode, FALSE, nullptr, nullptr);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "%!FUNC! thread event with status: %!STATUS!", status);

            if (STATUS_WAIT_1 == status)
            {
                do
                {
                    // process data in the cyclic buffer until either the read buffer is empty
                    // or the write buffer is full.

                    // we have a write event, there should be data available to move.

                    ULONG bytesAvailableToRead = 0;

                    KeResetEvent(m_WriteEvent);

                    // Retrieve the midi out position for the buffer we are reading from. The data between the read position
                    // and write position is valid. (read position is our last read position, write position is their last written).
                    // Retrieve our read position first since we know it won't be changing, get their last write position second,
                    // so we can get as much data as possible.
                    ULONG midiOutReadPosition = (ULONG) InterlockedCompareExchange((LONG *)m_ReadRegister, 0, 0);
                    ULONG midiOutWritePosition = (ULONG) InterlockedCompareExchange((LONG *)m_WriteRegister, 0, 0);

                    // first figure out how much data there is to read, taking
                    // into account the looping buffer.
                    if (midiOutReadPosition <= midiOutWritePosition)
                    {
                        // if the read position is less than the write position, 
                        // then we haven't looped around so the difference between
                        // the read and write position is the amount of data to read.
                        bytesAvailableToRead = midiOutWritePosition - midiOutReadPosition;
                    }
                    else
                    {
                        // the write position is behind the read position, so
                        // we looped around. The difference between the read position and 
                        // the write position is the available space, so the total buffer
                        // size minus the available space gives us the amount of data to read.
                        bytesAvailableToRead = m_BufferSize - (midiOutReadPosition - midiOutWritePosition);
                    }

                    if (bytesAvailableToRead == 0)
                    {
                        // nothing to copy, so we are finished with this pass
                        break;
                    }

                    PVOID startingReadAddress = (PVOID)(((PBYTE)m_KernelBufferMapping.Buffer1.m_BufferClientAddress) + midiOutReadPosition);
                    PUMPDATAFORMAT header = (PUMPDATAFORMAT)(startingReadAddress);
                    UINT32 dataSize = header->ByteCount;

                    if (dataSize < MINIMUM_UMP_DATASIZE || dataSize > MAXIMUM_UMP_DATASIZE)
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC! Invalid dataSize, aborting.");
                        // data is malformed, abort.
                        goto cleanup;
                    }

                    ULONG bytesToCopy = dataSize + sizeof(UMPDATAFORMAT);
                    ULONG finalReadPosition = (midiOutReadPosition + bytesToCopy) % m_BufferSize;

                    if (bytesAvailableToRead < bytesToCopy)
                    {
                        // if the full contents of this buffer isn't yet available,
                        // wait for more data to come in.
                        // Client will set an event when the write position advances.
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Full contents of buffer unavailable");
                        break;
                    }

                    m_TotalBuffersProcessed++;

                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, 
                        "%!FUNC! MidiOut processing buffer, total processed: %lu, more data: %s",
                        m_TotalBuffersProcessed, (finalReadPosition != midiOutWritePosition)?"yes":"no");

                    USBMIDI2DriverIoWrite(
                        AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin)),
                        (PUCHAR)startingReadAddress + sizeof(UMPDATAFORMAT),
                        header->ByteCount,
                        (finalReadPosition != midiOutWritePosition) ? TRUE : FALSE  // indicates there is more data in this pass or not
                    );

                    // advance our read position
                    InterlockedExchange((LONG *)m_ReadRegister, finalReadPosition);
                    KeSetEvent(m_ReadEvent, 0, 0);
                }while(true);
            }
            else
            {
                // exit event or failure, exit the loop to terminate the thread.
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! thread terminated with status: %!STATUS!", status);
                break;
            }
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! thread event end with status: %!STATUS!", status);
        }while(true);
    }
    else if (AcxPinGetId(m_Pin) == MidiPinTypeMidiIn)
    {
        // application is reading a midi message in, this is traditionally called
        // midi in.            
        PVOID waitObjects[] = { m_ThreadExitEvent.get(), m_ReadEvent };

        do
        {
            // run until we get a thread exit event.
            // wait for either a read event indicating data has been read, or thread exit.
            status = KeWaitForMultipleObjects(2, waitObjects, WaitAny, Executive, KernelMode, FALSE, nullptr, nullptr);

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "%!FUNC! thread event with status: %!STATUS!", status);

            if (STATUS_WAIT_1 == status)
            {
                // We have a read evnet
                KeResetEvent(m_ReadEvent);

                // we have a read event, therefore we need to check thresholds
                ULONG bufferInUse = BufferInUse();

                if (bufferInUse >= READ_BUFFER_MAX_THRESHOLD)
                {
                    USBMIDI2DriverIoContinuousReader(AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin)), false, false);
                }
                else if (bufferInUse < READ_BUFFER_MIN_THRESHOLD)
                {
                    USBMIDI2DriverIoContinuousReader(AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin)), true, false);
                }
            }
            else
            {
                // exit event or failure, exit the loop to terminate the thread.
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! thread terminated with status: %!STATUS!", status);
                break;
            }
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "%!FUNC! thread event end with status: %!STATUS!", status);
        } while (true);
    }

cleanup:
    m_ThreadExitedEvent.set();
    PsTerminateSystemThread(status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
bool
StreamEngine::FillReadStream(
    PUINT8  pBuffer,
    size_t  bufferSize
)
/*++
Routine Description:

    Helper routine to fill stream buffer with data, formatting to UMPDATAFORMAT

Arguments:

    pBuffer - buffer to copy
    bufferSize - the number of bytes in the buffer filled.
    Context - the driver context

Return Value:

    true if successful

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry with m_IsRunning set to %d", (int)m_IsRunning);

    // Check parameters passed
    if (!pBuffer)
        return false;

    // Current mechanism to determine if currently processing data is that
    // the StreamEngine is not null. TBD this mechanism needs to be fixed.
    //auto lock = m_MidiInLock.acquire();
    if (m_IsRunning)
    {
        // Retrieve the midi in position for the buffer that we are writing to. The data between the write position
        // and read position is empty. (read position is their last read position, write position is our last written).
        // retrieve our write position first since we know it won't be changing, and their read position second,
        // so we can have as much free space as possible.
        ULONG midiInWritePosition = (ULONG)InterlockedCompareExchange((LONG*)m_WriteRegister, 0, 0);
        ULONG midiInReadPosition = (ULONG)InterlockedCompareExchange((LONG*)m_ReadRegister, 0, 0);

        // Check enough space to write into
        ULONG bytesAvailable = 0;

        // Now we need to calculate the available space, taking into account the looping
        // buffer.
        if (midiInReadPosition <= midiInWritePosition)
        {
            // if the read position is less than the write position, then the difference between
            // the read and write position is the buffer in use, same as above. So, the total
            // buffer size minus the used buffer gives us the available space.
            bytesAvailable = m_BufferSize - (midiInWritePosition - midiInReadPosition);
        }
        else
        {
            // we looped around, the write position is behind the read position.
            // The difference between the read position and the write position
            // is the available space, which is exactly what we want.
            bytesAvailable = midiInReadPosition - midiInWritePosition;
        }

        // Note, if we fill the buffer up 100%, then write position == read position,
        // which is the same as when the buffer is empty and everything in the buffer
        // would be lost.
        // Reserve 1 byte so that when the buffer is full the write position will trail
        // the read position.
        // Because of this reserve, and the above calculation, the true bytesAvailable 
        // count can never be 0.
        ASSERT(bytesAvailable != 0);
        bytesAvailable--;

        if (bufferSize > bytesAvailable)
        {
            // Because this thread is called at >= DISPATCH_LEVEL, we can not make use of the read event
            // to wait for space to come available.

            // We will drop this data but request to handle existing data for next call
            m_TotalDroppedBuffers++;
            m_ContiguousDroppedBuffers++;
            KeSetEvent(m_WriteEvent, 0, 0);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Insufficient buffer, data dropped, total dropped: %lu, contiguous dropped: %lu", m_TotalDroppedBuffers, m_ContiguousDroppedBuffers);
            return true;
        }

        m_TotalBuffersProcessed++;
        m_ContiguousDroppedBuffers = 0;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! MidiIn processing buffer, total processed: %lu", m_TotalBuffersProcessed);

        // There's enough space available, calculate our write position
        PVOID startingWriteAddress = (PVOID)(((PBYTE)m_KernelBufferMapping.Buffer1.m_BufferClientAddress) + midiInWritePosition);

        // Copy the data
        RtlCopyMemory(
            (PVOID)startingWriteAddress,
            (PVOID)pBuffer,
            bufferSize
        );

        // now calculate the new position that the buffer has been written up to.
        // this will be the original write position, plus the bytes copied, again modululs
        // the buffer size to take into account the loop.
        ULONG finalWritePosition = (midiInWritePosition + bufferSize) % m_BufferSize;

        // finalize by advancing the registers and setting the write event

        // advance the write position and signal that there's data available
        InterlockedExchange((LONG*)m_WriteRegister, finalWritePosition);
        KeSetEvent(m_WriteEvent, 0, 0);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit with data dropped.");
        return true;    // just drop the data
    }        

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return true;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
ULONG
StreamEngine::BufferInUse()
{
    ULONG bufferInUse = 0;

    // Retrieve the midi in position for the buffer that we are writing to. The data between the write position
    // and read position is empty. (read position is their last read position, write position is our last written).
    // retrieve our write position first since we know it won't be changing, and their read position second,
    // so we can have as much free space as possible.
    ULONG midiInWritePosition = (ULONG)InterlockedCompareExchange((LONG*)m_WriteRegister, 0, 0);
    ULONG midiInReadPosition = (ULONG)InterlockedCompareExchange((LONG*)m_ReadRegister, 0, 0);

    // Now we need to calculate the available space, taking into account the looping
    // buffer.
    if (midiInReadPosition <= midiInWritePosition)
    {
        // if the read position is less than the write position, then the difference between
        // the read and write position is the buffer in use, same as above.
        bufferInUse = (midiInWritePosition - midiInReadPosition);
    }
    else
    {
        // we looped around, the write position is behind the read position.
        // The difference between the read position and the write position
        // is the available space
        bufferInUse = m_BufferSize - (midiInReadPosition - midiInWritePosition);
    }

    return bufferInUse;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::PrepareHardware()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // If already in this state, do nothing.
    //
    if (m_StreamState == AcxStreamStatePause)
    {
        // Nothing to do.
        status = STATUS_SUCCESS;
        goto exit;
    }

    if (m_StreamState != AcxStreamStateStop)
    {
        // Error out.
        status = STATUS_INVALID_STATE_TRANSITION;
        goto exit;
    }

    //
    // Stop to Pause.
    // Nothing to do at this time for this sample driver.
    //
    m_StreamState = AcxStreamStatePause;
    status = STATUS_SUCCESS;

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::ReleaseHardware()
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // If already in this state, do nothing.
    //
    if (m_StreamState == AcxStreamStateStop)
    {
        // Nothing to do.
        goto exit;
    }

    //
    // Just assert we are in the correct state. 
    //
    ASSERT(m_StreamState == AcxStreamStatePause);

    //
    // Pause to Stop.
    // Nothing to do at this time for this sample driver.
    // NOTE: On the way down driver must succeed the request.
    //
    m_StreamState = AcxStreamStateStop;

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::Pause()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    if (m_StreamState == AcxStreamStatePause)
    {
        // Nothing to do.
        status = STATUS_SUCCESS;
        goto exit;
    }

    if (m_StreamState != AcxStreamStateRun)
    {
        // Error out.
        status = STATUS_INVALID_STATE_TRANSITION;
        goto exit;
    }

    // transition from run to paused
    if (AcxPinGetId(m_Pin) == MidiPinTypeMidiOut)
    {
        // shut down and clean up the worker thread.
        m_ThreadExitEvent.set();
        m_ThreadExitedEvent.wait();
        KeWaitForSingleObject(m_WorkerThread, Executive, KernelMode, FALSE, nullptr);
        ObDereferenceObject(m_WorkerThread);
        m_WorkerThread = nullptr;
    }
    else if (AcxPinGetId(m_Pin) == MidiPinTypeMidiIn)
    {
        // Make sure we are not trying to change state while processing
        auto lock = m_MidiInLock.acquire();

        m_IsRunning = false;
        m_ThreadExitEvent.set();
        m_TotalDroppedBuffers = 0;
        m_ContiguousDroppedBuffers = 0;
        m_TotalBuffersProcessed = 0;

        // Stop Continous Reader
        USBMIDI2DriverIoContinuousReader(AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin)), false, false);

        // shut down and clean up the worker thread.
        m_ThreadExitEvent.set();
        m_ThreadExitedEvent.wait();
        KeWaitForSingleObject(m_WorkerThread, Executive, KernelMode, FALSE, nullptr);
        ObDereferenceObject(m_WorkerThread);
        m_WorkerThread = nullptr;
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "%!FUNC! Invalid device request, Pin type not valid.");
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Run to Pause.
    // NOTE: On the way down we always want to succeed.
    //
    m_StreamState = AcxStreamStatePause;
    status = STATUS_SUCCESS;

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::Run()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    if (m_StreamState == AcxStreamStateRun)
    {
        // Nothing to do.
        status = STATUS_SUCCESS;
        goto exit;
    }

    if (m_StreamState != AcxStreamStatePause)
    {
        status = STATUS_INVALID_STATE_TRANSITION;
        goto exit;
    }

    // transition from paused to run
    if (AcxPinGetId(m_Pin) == MidiPinTypeMidiOut)
    {
        m_ThreadExitEvent.clear();

        // Create and start the midi out worker thread for the cyclic buffer.
        HANDLE handle = nullptr;
        NT_RETURN_IF_NTSTATUS_FAILED(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, 0, 0, 0, StreamEngine::WorkerThread, this));

        // we use the handle from ObReferenceObjectByHandleWithTag, so we can close this handle
        // when this goes out of scope.
        auto closeHandle = wil::scope_exit([&handle]() {
                ZwClose(handle);
            });

        // reference the thread using our pooltag, for easier diagnostics later on in the event of
        // a leak.
        NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandleWithTag(handle, THREAD_ALL_ACCESS, nullptr, KernelMode, USBMIDI_POOLTAG, (PVOID*)&m_WorkerThread, nullptr));

        // boost the worker thread priority, to ensure that the write events are handled as
        // timely as possible.
        KeSetPriorityThread(m_WorkerThread, HIGH_PRIORITY);
    }
    else if (AcxPinGetId(m_Pin) == MidiPinTypeMidiIn)
    {
        m_ThreadExitEvent.clear();

        // Create and start the midi in worker thread for the management of continuous reader.
        HANDLE handle = nullptr;
        NT_RETURN_IF_NTSTATUS_FAILED(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, 0, 0, 0, StreamEngine::WorkerThread, this));

        // we use the handle from ObReferenceObjectByHandleWithTag, so we can close this handle
        // when this goes out of scope.
        auto closeHandle = wil::scope_exit([&handle]() {
            ZwClose(handle);
            });

        // reference the thread using our pooltag, for easier diagnostics later on in the event of
        // a leak.
        NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandleWithTag(handle, THREAD_ALL_ACCESS, nullptr, KernelMode, USBMIDI_POOLTAG, (PVOID*)&m_WorkerThread, nullptr));

        // boost the worker thread priority, to ensure that the write events are handled as
        // timely as possible.
        KeSetPriorityThread(m_WorkerThread, HIGH_PRIORITY);

        WDFDEVICE devCtx = AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin));
        PDEVICE_CONTEXT pDevCtx = GetDeviceContext(devCtx);

        // NOTE: Must not start continuous reader until after setting pMidiStreamEngine as ISR can be called before
        // the acquired lock is cleared.
        // Start the continuous reader
        if (pDevCtx)
        {
            // Make sure device knows about this stream engine for FillReadStream
            pDevCtx->pStreamEngine = this;

            // Set that we are running
            m_IsRunning = true;

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "%!FUNC! STARTING Continuous Reader.");
            USBMIDI2DriverIoContinuousReader(AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin)), true, false);
        }
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "%!FUNC! Could not start interrupt pipe as no MidiInPipe");
        }

    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "%!FUNC! Invalid device request, Pin type not valid.");
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Pause to Run.
    //
    m_StreamState = AcxStreamStateRun;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    status = STATUS_SUCCESS;

exit:
    return status;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::GetSingleBufferMapping(
    UINT32 BufferSize,
    KPROCESSOR_MODE Mode,
    BOOL LockPages,
    PSINGLE_BUFFER_MAPPING BaseMapping,
    PSINGLE_BUFFER_MAPPING Mapping
    )
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // The allocation must not be 0
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, BufferSize == 0);

    // The allocation must be a multiple of the page size,
    // as entire pages are mapped.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, (BufferSize % PAGE_SIZE) != 0);

    auto cleanupOnFailure = wil::scope_exit([&]() {
            CleanupSingleBufferMapping(Mapping);
        });

    Mapping->m_Mode = Mode;

    if (BaseMapping)
    {
        // We save the buffer address with a flag indicating we don't own it, so it's available
        // for future reference if needed.
        Mapping->m_BufferAddress = Mapping->m_BufferAddress;
        Mapping->m_OwnsAllocation = FALSE;

        Mapping->m_BufferMdl = IoAllocateMdl(MmGetMdlVirtualAddress(BaseMapping->m_BufferMdl), BufferSize, FALSE, FALSE, nullptr);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->m_BufferMdl);

        // build the new mdl using the base mdl as a template
        IoBuildPartialMdl( BaseMapping->m_BufferMdl, Mapping->m_BufferMdl, MmGetMdlVirtualAddress(BaseMapping->m_BufferMdl), BufferSize );
    }
    else
    {
        // otherwise, we're creating a whole new double mapping,
        // allocate the buffer we're going to double-map to the user space.
        Mapping->m_BufferAddress = ExAllocatePool2(POOL_FLAG_NON_PAGED, BufferSize, USBMIDI_POOLTAG);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->m_BufferAddress);
        Mapping->m_OwnsAllocation = TRUE;        

        Mapping->m_BufferMdl = IoAllocateMdl(Mapping->m_BufferAddress, BufferSize, FALSE, FALSE, nullptr);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->m_BufferMdl);

        // This is the original MDL for this, and will be used as the base mdl for any future mdl's.
        // We are responsible for building the MDL.
        MmBuildMdlForNonPagedPool(Mapping->m_BufferMdl);
    }

    if (LockPages)
    {
        __try
        {
            // map the read register to user/kernel space.
            Mapping->m_BufferClientAddress = MmMapLockedPagesSpecifyCache(Mapping->m_BufferMdl, Mode, MmNonCached, nullptr, FALSE, NormalPagePriority | MdlMappingNoExecute);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            Mapping->m_BufferClientAddress = nullptr;
        }
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->m_BufferClientAddress);
    }

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::CleanupSingleBufferMapping(
    PSINGLE_BUFFER_MAPPING Mapping
    )
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // having a client address means it has been mapped,
    // so unmap it.
    if (Mapping->m_BufferClientAddress)
    {
        if (Mapping->m_Mode == UserMode)
        {
            // when mapping user mode, we map with MmMapLockedPagesSpecifyCache,
            // which gets freed with MmUnmapLockedPages.
            MmUnmapLockedPages(Mapping->m_BufferClientAddress, Mapping->m_BufferMdl);
        }
        else
        {
            // when mapping kernel mode, we map with MmMapLockedPagesWithReservedMapping,
            // which gets freed with MmUnmapReservedMapping.
            MmUnmapReservedMapping(Mapping->m_BufferClientAddress, USBMIDI_POOLTAG, Mapping->m_BufferMdl);
        }
        Mapping->m_BufferClientAddress = nullptr;
    }

    // clean up the mdl if it is present
    if (Mapping->m_BufferMdl)
    {
        IoFreeMdl(Mapping->m_BufferMdl);
        Mapping->m_BufferMdl = nullptr;
    }

    // if this mapping owns the allocation, free the memory
    if (Mapping->m_MappingAddress)
    {
        MmFreeMappingAddress(Mapping->m_MappingAddress, USBMIDI_POOLTAG);
        Mapping->m_MappingAddress = nullptr;
    }

    // if this mapping owns the allocation, free the memory
    if (Mapping->m_BufferAddress && Mapping->m_OwnsAllocation)
    {
        ExFreePoolWithTag(Mapping->m_BufferAddress, USBMIDI_POOLTAG);
    }

    Mapping->m_BufferAddress = nullptr;
    Mapping->m_OwnsAllocation = FALSE;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::GetDoubleBufferMapping(
    UINT32 BufferSize,
    KPROCESSOR_MODE Mode,
    PSINGLE_BUFFER_MAPPING BaseMapping,
    PDOUBLE_BUFFER_MAPPING Mapping
    )
{

    // this code creates two single buffer mappings, back to back,
    // in either user mode or kernel mode. Being a cyclic buffer,
    // this has the benefit of allowing us to read or write off of the
    // end of the first buffer with the effect of looping back to the beginning
    // of the buffer. We can read/write past by up to 1 bufferSize.

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // In the event of failure, be sure to clean up anything
    // allocated during this call.
    auto cleanupOnFailure = wil::scope_exit([&]() {
            // clean up in the opposite order of creation,
            // since Buffer2 may have MDL's that reference
            // Buffer 1 memory.
            CleanupSingleBufferMapping(&Mapping->Buffer2);
            CleanupSingleBufferMapping(&Mapping->Buffer1);
        });

    // if we have a "BaseMapping", we need to build Buffer1 using BaseMapping, otherwise BaseMapping will be null
    // and we'll build a new mapping. Don't lock it.
    NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(BufferSize, Mode, FALSE, BaseMapping, &Mapping->Buffer1));

    // if we have a BaseMapping, we also want to build Buffer2 using that BaseMapping.
    // If we don't have a BaseMapping, then we want to use Buffer1 as the base mapping.
    // Don't lock it.
    NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(BufferSize, Mode, FALSE, BaseMapping?BaseMapping:&Mapping->Buffer1, &Mapping->Buffer2));

    // MmMapLockedPagesSpecifyCache is used to map to user mode, and it takes a requested address to use.
    // MmMapLockedPagesSpecifyCache does not respect the requested address for kernel mode, so kernel mode
    // mapping is done differently.
    if (UserMode == Mode)
    {
        SINGLE_BUFFER_MAPPING doubleBuffer;

        // Create a full mapping, with an allocation, of a buffer 2x the required buffer size.
        // We aren't going to keep this, this is just to get the memory manager to set aside some space
        // and provide us the virtual address for it. Don't lock it yet.
        NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(BufferSize * 2, Mode, FALSE, nullptr, &doubleBuffer));

        // A double sized buffer is created to reserve the block of memory
        // needed to map the real buffer back to back. Once we're finished
        // allocating the buffer and mapping it twice, all of the double
        // buffer allocations can be freed.
        auto cleanupOnExit = wil::scope_exit([&]() {
                CleanupSingleBufferMapping(&doubleBuffer);
           });

        // the following scope will have a raised IRQL, to help ensure the mapping succeeds.
        KIRQL OldIrql;
        KeRaiseIrql( APC_LEVEL, &OldIrql );

        // restore the irql when we exit this block, 
        // whether or not the mapping has succeeded.
        auto restoreIrql = wil::scope_exit([&]() {
                KeLowerIrql( OldIrql );
            });

        // retry the mapping up to 8 times, until we have a successful client address for both mappings.
        // When we unmap it's possible that something will come in and use the space that was just unmapped, causing
        // the double mapping to fail.
        for (int i = 0; i < 8 && ((nullptr == Mapping->Buffer1.m_BufferClientAddress) || (nullptr == Mapping->Buffer2.m_BufferClientAddress)); i++)
        {
            // First lock the double buffer
            if (nullptr == doubleBuffer.m_BufferClientAddress)
            {
                __try
                {
                    doubleBuffer.m_BufferClientAddress = MmMapLockedPagesSpecifyCache( doubleBuffer.m_BufferMdl, Mode, MmNonCached, nullptr, FALSE, NormalPagePriority | MdlMappingNoExecute);
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    doubleBuffer.m_BufferClientAddress = nullptr;
                }
                NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == doubleBuffer.m_BufferClientAddress);
            }

            // save off the address that was used for the new locking
            PVOID doubleBufferMappedAddress = doubleBuffer.m_BufferClientAddress;

            // Unmap the double so we can try to map the other two buffers into its place,
            // hopefully the memory block isn't taken before we complete the mapping-> If it is, we'll retry.
            MmUnmapLockedPages( doubleBuffer.m_BufferClientAddress, doubleBuffer.m_BufferMdl );
            doubleBuffer.m_BufferClientAddress = nullptr;

            __try
            {
                // map the data buffer at the doubleBufferMappedAddress that we just had mapped, and then unmapped.
                Mapping->Buffer1.m_BufferClientAddress = MmMapLockedPagesSpecifyCache(Mapping->Buffer1.m_BufferMdl, Mode, MmNonCached, doubleBufferMappedAddress, FALSE, NormalPagePriority | MdlMappingNoExecute);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Mapping->Buffer1.m_BufferClientAddress = nullptr;
            }

            // if we mapped the buffer the first time, map the same buffer a second time
            if (nullptr != Mapping->Buffer1.m_BufferClientAddress)
            {
                __try
                {
                    // map the same data buffer a second time, at an address immediately following the first mapping->
                    Mapping->Buffer2.m_BufferClientAddress = MmMapLockedPagesSpecifyCache(Mapping->Buffer2.m_BufferMdl, Mode, MmNonCached, ((PVOID)(((PBYTE)(Mapping->Buffer1.m_BufferClientAddress))+BufferSize)), FALSE, NormalPagePriority | MdlMappingNoExecute);
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    Mapping->Buffer2.m_BufferClientAddress = nullptr;
                }

                // if we failed mapping the second time, we may be trying again,
                // unmap the first mapping in preparation for the next try
                if (nullptr == Mapping->Buffer2.m_BufferClientAddress)
                {
                    MmUnmapLockedPages( Mapping->Buffer1.m_BufferClientAddress, Mapping->Buffer1.m_BufferMdl );
                    Mapping->Buffer1.m_BufferClientAddress = nullptr;
                }

                // not back to back, try again
                if (Mapping->Buffer2.m_BufferClientAddress != (((PBYTE)(Mapping->Buffer1.m_BufferClientAddress))+BufferSize))
                {
                    MmUnmapLockedPages( Mapping->Buffer1.m_BufferClientAddress, Mapping->Buffer1.m_BufferMdl );
                    Mapping->Buffer1.m_BufferClientAddress = nullptr;
                    MmUnmapLockedPages( Mapping->Buffer2.m_BufferClientAddress, Mapping->Buffer2.m_BufferMdl );
                    Mapping->Buffer2.m_BufferClientAddress = nullptr;
                }

            }
        }

        // if mapping failed, we failed.
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->Buffer1.m_BufferClientAddress);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->Buffer2.m_BufferClientAddress);
    } 
    else
    {
        // kernel mode needs to map slightly differently,
        // first, we allocate the mapping address space needed for both buffers,
        // then we map them to the reserved mapping.
        Mapping->Buffer1.m_MappingAddress = MmAllocateMappingAddressEx(BufferSize * 2, USBMIDI_POOLTAG, MM_MAPPING_ADDRESS_DIVISIBLE);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->Buffer1.m_MappingAddress);

        Mapping->Buffer1.m_BufferClientAddress = MmMapLockedPagesWithReservedMapping(Mapping->Buffer1.m_MappingAddress,USBMIDI_POOLTAG, Mapping->Buffer1.m_BufferMdl, MmNonCached);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->Buffer1.m_BufferClientAddress);

        Mapping->Buffer2.m_BufferClientAddress = MmMapLockedPagesWithReservedMapping(((PVOID)(((PBYTE)(Mapping->Buffer1.m_MappingAddress))+BufferSize)), USBMIDI_POOLTAG, Mapping->Buffer2.m_BufferMdl, MmNonCached);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == Mapping->Buffer2.m_BufferClientAddress);

        // if the mapping isn't back to back, we have a problem.
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, Mapping->Buffer2.m_BufferClientAddress != (((PBYTE)(Mapping->Buffer1.m_BufferClientAddress))+BufferSize));
    }

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::CleanupDoubleBufferMapping(
    _Inout_ PDOUBLE_BUFFER_MAPPING Mapping
    )
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Buffer 1 typically owns the memory, so unmap
    // and remove idl's in the opposite order of creation,
    // buffer 2 and then buffer 1.
    CleanupSingleBufferMapping(&Mapping->Buffer2);
    CleanupSingleBufferMapping(&Mapping->Buffer1);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::GetLoopedStreamingBuffer(
    ULONG BufferSize,
    PKSMIDILOOPED_BUFFER  Buffer
    )
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry requested %lu for %s", BufferSize, AcxPinGetId(m_Pin) == MidiPinTypeMidiIn?"MidiIn":"MidiOut");

    // handle incoming property call to retrieve the looped streaming buffer.
    // this is registered as a get handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by ACX. basic support is also handled
    // by avstream, so no additional handling is required.
    PAGED_CODE();

    // In the event of failure, be sure to clean up anything
    // allocated during this call.
    auto cleanupOnFailure = wil::scope_exit([&]() {
            m_Process = nullptr;
            CleanupDoubleBufferMapping(&m_KernelBufferMapping);
            CleanupDoubleBufferMapping(&m_ClientBufferMapping);
        });

    // cache the current process
    // caller should always be the same process.
    ASSERT(m_Process == nullptr);
    m_Process = IoGetCurrentProcess();

    // Start by using the requested buffer size. 
    // The actual buffer size must be some multiple of PAGE_SIZE
    // so that we can double map it on the page boundary, so we're
    // going to round this number up to the nearest page.
    ULONG bufferSize = BufferSize;
    ULONG testResult {0};

    // The allocation must not be 0
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, bufferSize == 0);

    // if adding a page to the buffer size causes it to overflow, the requested size is
    // an invalid parameter.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !NT_SUCCESS(RtlULongAdd(bufferSize, PAGE_SIZE, &testResult)));

    // round to the nearest page
    bufferSize = ROUND_TO_PAGES(bufferSize);

    // If the adjusted buffer size is larger than the maximum permitted, fail the request.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, bufferSize > MAXIMUM_LOOPED_BUFFER_SIZE);

    // If the adjusted buffer size cannot be doubled, fail the request.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !NT_SUCCESS(RtlULongMult(bufferSize, 2, &testResult)));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Allocating %lu for %s", bufferSize, AcxPinGetId(m_Pin) == MidiPinTypeMidiIn?"MidiIn":"MidiOut");

    // Create our mapping to user mode w/ the double buffer.
    NT_RETURN_IF_NTSTATUS_FAILED(GetDoubleBufferMapping(bufferSize, UserMode, nullptr, &m_ClientBufferMapping));

    // now build a double buffer mapping in kernel mode
    NT_RETURN_IF_NTSTATUS_FAILED(GetDoubleBufferMapping(bufferSize, KernelMode, &m_ClientBufferMapping.Buffer1, &m_KernelBufferMapping));

    // Success, we now have a single buffer mapped to the client space back to back
    // return the client address pointer to the first one
    m_BufferSize = Buffer->ActualBufferSize = bufferSize;
    Buffer->BufferAddress = m_ClientBufferMapping.Buffer1.m_BufferClientAddress;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Allocated %lu for %s", bufferSize, AcxPinGetId(m_Pin) == MidiPinTypeMidiIn?"MidiIn":"MidiOut");

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::GetLoopedStreamingRegisters(
    PKSMIDILOOPED_REGISTERS   Buffer
    )
{
    // handle incoming property call to retrieve the looped streaming registers.
    // this is registered as a get handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by ACX. basic support is also handled
    // by avstream, so no additional handling is required.

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    ASSERT(m_Process == IoGetCurrentProcess());

    // registers already created, can't create again.
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, nullptr != m_ReadRegister);

    // in the event of failure, clean up any partial mappings.
    auto cleanupOnFailure = wil::scope_exit([&]() {
            CleanupSingleBufferMapping(&m_Registers);
            m_ReadRegister = nullptr;
            m_WriteRegister = nullptr;
        });

    // When mapping into user mode, the mapping happens on page-size alignments,
    // which means you need to allocate a page to ensure you own the entire memory
    // that's mapped into user mode (to prevent other unrelated kernel allocations
    // from being mapped into user)
    NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(PAGE_SIZE, UserMode, TRUE, nullptr, &m_Registers));

    m_ReadRegister = (PULONG) m_Registers.m_BufferAddress;
    m_WriteRegister = m_ReadRegister + 1;

    Buffer->ReadPosition = (PULONG) m_Registers.m_BufferClientAddress;
    Buffer->WritePosition = ((PULONG) m_Registers.m_BufferClientAddress) + 1;

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::SetLoopedStreamingNotificationEvent(
    PKSMIDILOOPED_EVENT2   Buffer
    )
{
    // handle incoming property call to set the looped streaming events.
    // this is registered as a set handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by ACX. basic support is also handled
    // by avstream, so no additional handling is required.

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    ASSERT(m_Process == IoGetCurrentProcess());

    // reference the notification event provided by the client,
    // For midi out will use this event as the signal that midi out data has been written,
    // For midi in this will be the signal that we will set when midi in data has been
    // written.
    NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandle(Buffer->WriteEvent,
                                              EVENT_MODIFY_STATE,
                                              *ExEventObjectType,
                                              UserMode,
                                              (PVOID*)&m_WriteEvent,
                                              nullptr));

    // reference the notification event provided by the client,
    // For midi out will use this event as the signal that midi out data has been read from the buffer,
    // For midi in this will be the signal that space is available for additional data to be
    // written.
    NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandle(Buffer->ReadEvent,
                                              EVENT_MODIFY_STATE,
                                              *ExEventObjectType,
                                              UserMode,
                                              (PVOID*)&m_ReadEvent,
                                              nullptr));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

