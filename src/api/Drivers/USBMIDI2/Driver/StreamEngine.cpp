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

// The same filter instance may not be used for both midi
// pin instances, so storing the pins on the filter for the
// pins to share will not work.
//
// To work around this, a global is used for sharing the pin
// for looping midi out back to midi in
//
// Because we are using a global pin instance,
// InstancesPossible on the in/out pins must not exceed 1 when
// doing loopback.
StreamEngine* g_MidiInStreamEngine {nullptr};

// UMP 32 is 4 bytes
#define MINIMUM_UMP_DATASIZE 4

// UMP 128 is 16 bytes
#define MAXIMUM_UMP_DATASIZE 16

_Use_decl_annotations_
StreamEngine::StreamEngine(
    _In_ ACXPIN Pin
    ) : m_Pin(Pin)
{
    PAGED_CODE();

    // TODO: should this be config? do we support standard?
    m_IsLooped = TRUE;

    // When used for loopback "standard" streaming,
    // we use a list_entry structure to store the loopback messages
    InitializeListHead(&m_LoopbackMessageQueue);
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

    // For standard streaming loopback, we have a list of the messages
    // being looped back, if it's not empty, free any remaining messages.
    while(!IsListEmpty(&m_LoopbackMessageQueue))
    {
        PLOOPBACK_MESSAGE message = (PLOOPBACK_MESSAGE) RemoveHeadList(&m_LoopbackMessageQueue);
        ExFreePoolWithTag(message, USBMIDI_POOLTAG);
    }

    if (m_Process)
    {
        ASSERT(m_Process == IoGetCurrentProcess());
        m_Process = nullptr;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
void
StreamEngine::WorkerThread(
    PVOID context
    ){
    PAGED_CODE();
    auto streamEngine = reinterpret_cast<StreamEngine*>(context);
    streamEngine->HandleIo();
}

_Use_decl_annotations_
void
StreamEngine::HandleIo()
{
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
            if (STATUS_WAIT_1 == status)
            {
                do
                {
                    // process data in the cyclic buffer until either the read buffer is empty
                    // or the write buffer is full.

                    // In order to loopback data, we have to have a midi in pin.
                    // The job of this lock is to synchronize availability and
                    // state of pMidiStreamEngine in Device Context (g_MidiInStreamEngine), so it must be held for the duration of
                    // use of pMidiStreamEngine in Device Context (g_MidiInStreamEngine). There should be very little contention,
                    // as the only other time this is used is when midi in
                    // transitions between run and paused.

                    // we have a write event, there should be data available to move.
//                    if (nullptr != pDevCtx->pMidiStreamEngine)
                    if (nullptr != g_MidiInStreamEngine)
                    {
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
                            // TBD: need to log an abort

                            // data is malformed, abort.
                            return;
                        }

                        ULONG bytesToCopy = dataSize + sizeof(UMPDATAFORMAT);
                        ULONG finalReadPosition = (midiOutReadPosition + bytesToCopy) % m_BufferSize;

                        if (bytesAvailableToRead < bytesToCopy)
                        {
                            // if the full contents of this buffer isn't yet available,
                            // wait for more data to come in.
                            // Client will set an event when the write position advances.
                            break;
                        }

                        // Send relevant buffer to USB
                        PUMPDATAFORMAT thisData = (PUMPDATAFORMAT)startingReadAddress;
                        if (thisData->ByteCount)
                        {
                            USBMIDI2DriverIoWrite(
                                AcxCircuitGetWdfDevice(AcxPinGetCircuit(m_Pin)),
                                (PUCHAR)startingReadAddress + sizeof(UMPDATAFORMAT),
                                thisData->ByteCount
                            );
                        }

                        // advance our read position
                        InterlockedExchange((LONG *)m_ReadRegister, finalReadPosition);
                    }
                    else
                    {
                        // nothing to do with the data, so just update the read position up to the write position
                        // to indicate that it was "processed", so that the midi out writer won't fill up
                        // the buffer
                        InterlockedExchange((LONG *)(m_ReadRegister), *((ULONG*)(m_WriteRegister)));
                        break;
                    }
                }while(true);
            }
            else
            {
                // exit event or failure, exit the loop to terminate the thread.
                break;
            }
        }while(true);
    }
    // else, this is a midi in pin, nothing to do for this worker thread that is just
    // looping the midi out data back to midi in.

    m_ThreadExitedEvent.set();
    PsTerminateSystemThread(status);
}

_Use_decl_annotations_
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
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Check parameters passed
    if (!pBuffer)
        return false;

    // Current mechanism to determine if currently processing data is that
    // the StreamEngine is not null. TBD this mechanism needs to be fixed.
//    if (pDevCtx->pMidiStreamEngine)
    if (g_MidiInStreamEngine)
    {
        // Retrieve the midi in position for the buffer that we are writing to. The data between the write position
        // and read position is empty. (read position is their last read position, write position is our last written).
        // retrieve our write position first since we know it won't be changing, and their read position second,
        // so we can have as much free space as possible.
//        ULONG midiInWritePosition = (ULONG)InterlockedCompareExchange((LONG*)pDevCtx->pMidiStreamEngine->m_WriteRegister, 0, 0);
//        ULONG midiInReadPosition = (ULONG)InterlockedCompareExchange((LONG*)pDevCtx->pMidiStreamEngine->m_ReadRegister, 0, 0);
        ULONG midiInWritePosition = (ULONG)InterlockedCompareExchange((LONG*)g_MidiInStreamEngine->m_WriteRegister, 0, 0);
        ULONG midiInReadPosition = (ULONG)InterlockedCompareExchange((LONG*)g_MidiInStreamEngine->m_ReadRegister, 0, 0);


        // Check enough space to write into
        ULONG bytesAvailable = 0;

        // Now we need to calculate the available space, taking into account the looping
        // buffer.
        if (midiInReadPosition <= midiInWritePosition)
        {
            // if the read position is less than the write position, then the difference between
            // the read and write position is the buffer in use, same as above. So, the total
            // buffer size minus the used buffer gives us the available space.
//            bytesAvailable = pDevCtx->pMidiStreamEngine->m_BufferSize - (midiInWritePosition - midiInReadPosition);
            bytesAvailable = g_MidiInStreamEngine->m_BufferSize - (midiInWritePosition - midiInReadPosition);
        }
        else
        {
            // we looped around, the write position is behind the read position.
            // The difference between the read position and the write position
            // is the available space, which is exactly what we want.
            bytesAvailable = midiInReadPosition - midiInWritePosition;
        }
        if (bufferSize > bytesAvailable)
        {
            // We have a problem. We have data to move, but there
            // isn't enough buffer available to do it. The client is not reading data,
            // or not reading it fast enough.

            // TBD: need to log a glitch event if this happens, so we can
            // track it.

            // Two options, either retry, or drop the data.
            // We will drop this data but request to handle existing data for next call
            KeSetEvent(m_WriteEvent, 0, 0);
            return false;
        }

        // There's enough space available, calculate our write position
//        PVOID startingWriteAddress = (PVOID)(((PBYTE)pDevCtx->pMidiStreamEngine->m_KernelBufferMapping.Buffer1.m_BufferClientAddress) + midiInWritePosition);
        PVOID startingWriteAddress = (PVOID)(((PBYTE)g_MidiInStreamEngine->m_KernelBufferMapping.Buffer1.m_BufferClientAddress) + midiInWritePosition);

        // Copy the data
        RtlCopyMemory(
            (PVOID)startingWriteAddress,
            (PVOID)pBuffer,
            bufferSize
        );

        // now calculate the new position that the buffer has been written up to.
        // this will be the original write position, plus the bytes copied, again modululs
        // the buffer size to take into account the loop.
//        ULONG finalWritePosition = (midiInWritePosition + bytesToCopy) % pDevCtx->pMidiStreamEngine->m_BufferSize;
        ULONG finalWritePosition = (midiInWritePosition + bufferSize) % g_MidiInStreamEngine->m_BufferSize;

        // finalize by advancing the registers and setting the write event

        // advance the write position for the loopback and signal that there's data available
//        InterlockedExchange((LONG*)pDevCtx->pMidiStreamEngine->m_WriteRegister, finalWritePosition);
//        KeSetEvent(pDevCtx->pMidiStreamEngine->m_WriteEvent, 0, 0);
        InterlockedExchange((LONG*)g_MidiInStreamEngine->m_WriteRegister, finalWritePosition);
        KeSetEvent(g_MidiInStreamEngine->m_WriteEvent, 0, 0);
    }
    else
    {
        return true;    // just drop the data
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return true;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::PrepareHardware()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PAGED_CODE();

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
    return status;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::ReleaseHardware()
{
    PAGED_CODE();

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
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::Pause()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PAGED_CODE();

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
        // Midi out
        if (m_IsLooped)
        {
            // This is only applicable for looped cyclic buffer,
            // standard doesn't use the worker thread.
    
            // shut down and clean up the worker thread.
            m_ThreadExitEvent.set();
            m_ThreadExitedEvent.wait();
            KeWaitForSingleObject(m_WorkerThread, Executive, KernelMode, FALSE, nullptr);
            ObDereferenceObject(m_WorkerThread);
            m_WorkerThread = nullptr;
        }
    }
    else
    {
        // If g_MidiInStreamEngine is available, the midi out worker thread will loopback.
        // If it isn't available, the midi out worker will throw the data away.
        // So when we transition midi in from run to paused, acquire the lock that
        // protects g_MidiInStreamEngine and clear g_MidiInStreamEngine to stop the loopback data
        // flowing.
        // TBD - this mechanism needs to change in case where device can be destroyed
//        pDevCtx->pMidiStreamEngine = nullptr;
        g_MidiInStreamEngine = nullptr;
    }

    //
    // Run to Pause.
    // NOTE: On the way down we always want to succeed.
    //
    m_StreamState = AcxStreamStatePause;
    status = STATUS_SUCCESS;

exit:
    return status;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::Run()
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PAGED_CODE();

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
        // midi out
        if (m_IsLooped)
        {
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
    }
    else
    {
        // If g_MidiInStreamEngine is available, the midi out worker thread will loopback.
        // If it isn't available, the midi out worker will throw the data away.
        // So when we transition midi in from paused to run, acquire the lock that
        // protects g_MidiInStreamEngine and set g_MidiInStreamEngine to start the loopback data
        // flowing.
        // TBD this mechanism must be changed in case where multiple instances
//        pDevCtx->pMidiStreamEngine = this;
        g_MidiInStreamEngine = this;
    }

    //
    // Pause to Run.
    //
    m_StreamState = AcxStreamStateRun;
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

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::CleanupSingleBufferMapping(
    PSINGLE_BUFFER_MAPPING Mapping
    )
{
    PAGED_CODE();

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

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::CleanupDoubleBufferMapping(
    _Inout_ PDOUBLE_BUFFER_MAPPING Mapping
    )
{
    PAGED_CODE();

    // Buffer 1 typically owns the memory, so unmap
    // and remove idl's in the opposite order of creation,
    // buffer 2 and then buffer 1.
    CleanupSingleBufferMapping(&Mapping->Buffer2);
    CleanupSingleBufferMapping(&Mapping->Buffer1);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::GetLoopedStreamingBuffer(
    ULONG BufferSize,
    PKSMIDILOOPED_BUFFER  Buffer
    )
{
    // handle incoming property call to retrieve the looped streaming buffer.
    // this is registered as a get handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by ACX. basic support is also handled
    // by avstream, so no additional handling is required.
    PAGED_CODE();

    // this is only applicable if this is a looped pin instance.
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, FALSE == m_IsLooped);

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

    // if adding a page to the buffer size causes it to overflow, the requested size is
    // an invalid parameter.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, (bufferSize + PAGE_SIZE) < bufferSize);

    // round to the nearest page
    bufferSize = ROUND_TO_PAGES(bufferSize);

    // Create our mapping to user mode w/ the double buffer.
    NT_RETURN_IF_NTSTATUS_FAILED(GetDoubleBufferMapping(bufferSize, UserMode, nullptr, &m_ClientBufferMapping));

    // now build a double buffer mapping in kernel mode
    NT_RETURN_IF_NTSTATUS_FAILED(GetDoubleBufferMapping(bufferSize, KernelMode, &m_ClientBufferMapping.Buffer1, &m_KernelBufferMapping));

    // Success, we now have a single buffer mapped to the client space back to back
    // return the client address pointer to the first one
    m_BufferSize = Buffer->ActualBufferSize = bufferSize;
    Buffer->BufferAddress = m_ClientBufferMapping.Buffer1.m_BufferClientAddress;

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

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

    ASSERT(m_Process == IoGetCurrentProcess());

    // this is only applicable if this is a looped pin instance.
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, FALSE == m_IsLooped);

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

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
StreamEngine::SetLoopedStreamingNotificationEvent(
    PKSMIDILOOPED_EVENT    Buffer
    )
{
    // handle incoming property call to set the looped streaming events.
    // this is registered as a set handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by ACX. basic support is also handled
    // by avstream, so no additional handling is required.

    PAGED_CODE();

    ASSERT(m_Process == IoGetCurrentProcess());

    // this is only applicable if this is a looped pin instance.
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, FALSE == m_IsLooped);

    // reference the notification event provided by the client,
    // For midi out will use this event as the signal that midi out data has been written,
    // For midi in this will be the signal that we will set when midi in data has been
    // written.
    NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandle(Buffer->WriteEvent,
                                              EVENT_MODIFY_STATE,
                                              *ExEventObjectType,
                                              ExGetPreviousMode(),
                                              (PVOID*)&m_WriteEvent,
                                              nullptr));

    return STATUS_SUCCESS;
}

