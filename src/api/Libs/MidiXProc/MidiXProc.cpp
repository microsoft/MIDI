// Copyright (c) Microsoft Corporation. All rights reserved.
#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <wrl\module.h>
#include <wrl\event.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\win32_helpers.h>
#include <wil\registry.h>
#include <wil\result.h>
#include <wil\wistd_memory.h>
#include <wil\tracelogging.h>

#include "WindowsMidiServices.h"

#include <Devpkey.h>
#include "MidiKsDef.h"
#include "MidiDefs.h"
#include <TraceLoggingProvider.h>
#include "MidiXProc.h"

class MidiXProcTelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        MidiXProcTelemetryProvider,
        "Microsoft.Windows.Midi2.MidiXproc",
        // {159b4b38-c2b9-58bc-f3cc-dcd31ca44d21}
        (0x159b4b38,0xc2b9,0x58bc,0xf3,0xcc,0xdc,0xd3,0x1c,0xa4,0x4d,0x21))
};

// infinite timeouts when waiting for read events cause the server to just hang
#define MIDI_XPROC_BUFFER_FULL_WAIT_TIMEOUT 5000

// timeout waiting for the messages to be recieved
#define MIDI_XPROC_BUFFER_EMPTY_WAIT_TIMEOUT 5000

// timeout waiting for the messages to be recieved
#define MIDI_XPROC_CALLBACK_RETRY_TIMEOUT 1000

_Use_decl_annotations_
HRESULT
GetRequiredBufferSize(ULONG& requestedSize
)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    // a 0 buffer size is invalid.
    RETURN_HR_IF(E_INVALIDARG, requestedSize == 0);

    // if adding one allocation granularity to the requested size causes overflow, the requested size
    // is invalid.
    RETURN_HR_IF(E_INVALIDARG, (requestedSize + systemInfo.dwAllocationGranularity) < requestedSize);

    // Requested size is rounded up to the nearest dwAllocationGranularity
    requestedSize = ((requestedSize + systemInfo.dwAllocationGranularity - 1) & ~(systemInfo.dwAllocationGranularity - 1));

    // If the adjusted buffer size is larger than the maximum permitted, fail the request.
    RETURN_HR_IF(E_INVALIDARG, requestedSize > MAXIMUM_LOOPED_BUFFER_SIZE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MapBuffer(BOOL doubleMapped,
            ULONG size,
            PMEMORY_MAPPED_BUFFER bufferAllocationData
)
{
    PBYTE mapAddress = nullptr;

    if (doubleMapped)
    {
        wil::unique_virtualalloc_ptr<void> placeholderAlloc;
        placeholderAlloc.reset(VirtualAlloc(nullptr, 2 * (SIZE_T) size, MEM_RESERVE, PAGE_READWRITE));
        RETURN_IF_NULL_ALLOC(placeholderAlloc);
        mapAddress = (PBYTE)placeholderAlloc.get();
        placeholderAlloc.reset();

        bufferAllocationData->Map2.reset(MapViewOfFileEx(bufferAllocationData->FileMapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, size, mapAddress + size));
        RETURN_IF_NULL_ALLOC(bufferAllocationData->Map2);
    }

    bufferAllocationData->Map1.reset(MapViewOfFileEx(bufferAllocationData->FileMapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, size, mapAddress));
    RETURN_IF_NULL_ALLOC(bufferAllocationData->Map1);
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CreateMappedBuffer(BOOL doubleMapped,
                    ULONG size,
                    PMEMORY_MAPPED_BUFFER bufferAllocationData
)
{
    if (!bufferAllocationData->FileMapping)
    {
        bufferAllocationData->FileMapping.reset(CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr));
        RETURN_LAST_ERROR_IF_NULL(bufferAllocationData->FileMapping);
    }
    RETURN_IF_FAILED(MapBuffer(doubleMapped, size, bufferAllocationData));
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CreateMappedDataBuffer(ULONG requestedSize,
                            PMEMORY_MAPPED_BUFFER buffer,
                            PMEMORY_MAPPED_DATA data
)
{
    if (data->BufferSize == 0)
    {
        // If the buffer size is not already known
        // determine a buffer size that is compatible with
        // the requested size, which cannot be 0.
        RETURN_HR_IF(E_INVALIDARG, 0 == requestedSize);
        RETURN_IF_FAILED(GetRequiredBufferSize(requestedSize));
        data->BufferSize = requestedSize;
    }
    else
    {
        // The buffer has already been allocated is just being mapped,
        // the requested size is unused and should be 0.
        RETURN_HR_IF(E_INVALIDARG, 0 != requestedSize);
    }

    RETURN_IF_FAILED(CreateMappedBuffer(TRUE, data->BufferSize, buffer));

    data->BufferAddress = (PBYTE)buffer->Map1.get();
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CreateMappedRegisters(PMEMORY_MAPPED_BUFFER buffer,
                        PMEMORY_MAPPED_REGISTERS registers
)
{
    RETURN_IF_FAILED(CreateMappedBuffer(FALSE, sizeof(ULONG) * 2, buffer));
    registers->ReadPosition = (PULONG)buffer->Map1.get();
    registers->WritePosition = registers->ReadPosition + 1;
    return S_OK;
}

_Use_decl_annotations_
HRESULT
DisableMmcss(
    unique_mmcss_handle& mmcssHandle
)
{
    if (CMidiXProc::MMCSSUseEnabled())
    {
        RETURN_HR_IF(S_OK, NULL == mmcssHandle);
        // Detach the thread from the MMCSS service to free the handle.
        mmcssHandle.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
EnableMmcss(
    unique_mmcss_handle& mmcssHandle,
    DWORD& mmcssTaskId
)
{
    if (CMidiXProc::MMCSSUseEnabled())
    {
        mmcssHandle.reset(AvSetMmThreadCharacteristics(L"Pro Audio", &mmcssTaskId));
        if (!mmcssHandle)
        {
            // If the task id has gone invalid, try with a new task id
            mmcssTaskId = 0;
            mmcssHandle.reset(AvSetMmThreadCharacteristics(L"Pro Audio", &mmcssTaskId));
        }

        auto cleanupOnFailure = wil::scope_exit([&]()
            {
                mmcssHandle.reset();
            });

        RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), FALSE == AvSetMmThreadPriority(mmcssHandle.get(), AVRT_PRIORITY_HIGH));
        cleanupOnFailure.release();
    }
    else
    {
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }

    return S_OK;
}

CMidiXProc::~CMidiXProc()
{
    Shutdown();
}

// static members, because of the way the C-style functions are used
constinit BOOL CMidiXProc::m_useMMCSS{ false };
constinit BOOL CMidiXProc::m_mmcssSettingRead{ false };

bool CMidiXProc::MMCSSUseEnabled()
{
    if (!m_mmcssSettingRead)
    {
        // see if we're going to use MMCSS here.
        DWORD regValueUseMMCSS{ MIDI_USE_MMCSS_REG_DEFAULT_VALUE };
        if (SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, MIDI_USE_MMCSS_REG_VALUE, &regValueUseMMCSS)))
        {
            m_useMMCSS = (bool)(regValueUseMMCSS > 0);
        }
        else
        {
            m_useMMCSS = false;
        }

        m_mmcssSettingRead = true;
    }

    return m_useMMCSS;
}

_Use_decl_annotations_
HRESULT
CMidiXProc::WaitForSendComplete(ULONG startingReadPosition, ULONG BufferWrittenPosition, UINT32 BufferLength)
{
    // Wait for midi messages to be recieved by other end of pipe
    if (m_MidiOut)
    {
        // Open a new handle to the read event, this will be a different handle than the
        // event used for adding messages to the queue, ensuring that a thread waiting for
        // space to come available won't interfere with a thread waiting for its message
        // to be consumed
        wil::unique_event_nothrow openedReadEvent;
        HANDLE readEventHandle = m_MidiOut->ReadEvent.get();

        if (m_MidiOut->ReadEventName.size() > 0)
        {
            openedReadEvent.reset(OpenEvent(EVENT_ALL_ACCESS, FALSE, m_MidiOut->ReadEventName.c_str()));
            if (openedReadEvent.get())
            {
                readEventHandle = openedReadEvent.get();
            }
        }
        HANDLE handles[] = { m_ThreadTerminateEvent.get(), readEventHandle };

        // calculate the linear position for the end of the written buffer.
        // We are using linear positions rather than looped position to simplify the calculations
        // needed to determine if the data has been read yet.
        ULONG bufferWrittenEndPosition = (BufferWrittenPosition + BufferLength);

        // the starting read position is the read position at the time the buffer was written, which is
        // by definition earlier than the linear position of the buffer that was just written.
        // If the buffer was written into the xproc queue behind the current read position,
        // then the linear position it was written into is actually 1 buffer size ahead relative to the 
        // starting read position, so we need to move the bufferWrittenEndPosition forward
        // by one BufferSize.
        if (startingReadPosition > BufferWrittenPosition)
        {
            bufferWrittenEndPosition += m_MidiOut->Data.BufferSize;
        }

        // used for calculating and counting times the buffer has looped since monitoring
        // for completion of this buffer started
        ULONG previousReadPosition = startingReadPosition;
        ULONG loopcount = 0;

        do
        {
            // wait for either the other end to read the buffer, termination, or timeout with no messages read
            DWORD ret = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, MIDI_XPROC_BUFFER_EMPTY_WAIT_TIMEOUT);
            if (ret == (WAIT_OBJECT_0 + 1))
            {
                // this is a manual reset event, so reset the event before reading the updated position so
                // that we do not miss any reads.
                ResetEvent(readEventHandle);

                // received a read event, retrieve the new read position
                ULONG readPosition = InterlockedCompareExchange((LONG*)m_MidiOut->Registers.ReadPosition, 0, 0);

                // if the new read position is less than the previous, we've read past the end of the buffer,
                // so we've completed a loop, increment the loop count.
                if (readPosition < previousReadPosition)
                {
                    loopcount++;
                }

                // calculate the current linear read position, accounting for loops
                readPosition += (m_MidiOut->Data.BufferSize * loopcount);

                // comparing linear positions, if the read position has advanced past the end of the buffer,
                // then it has been read.
                if (readPosition > bufferWrittenEndPosition)
                {
                    return S_OK;
                }

                previousReadPosition = readPosition;
            }
            else
            {
                // termination or timeout, wait aborted
                return E_ABORT;
            }
        } while (true);
    }

    // no midi out pipe, waiting not applicable
    return S_FALSE;
}

_Use_decl_annotations_
HRESULT
CMidiXProc::Initialize(DWORD* mmcssTaskId,
                        std::unique_ptr<MEMORY_MAPPED_PIPE>& midiIn,
                        std::unique_ptr<MEMORY_MAPPED_PIPE>& midiOut,
                        IMidiCallback *midiInCallback,
                        LONGLONG context,
                        BOOL overwriteZeroTimestamp
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == mmcssTaskId);
    RETURN_HR_IF(E_INVALIDARG, !midiIn && !midiOut);
    RETURN_HR_IF(E_INVALIDARG, midiIn && !midiInCallback);

    // validate that the provided data format is valid
    if (midiIn)
    {
        RETURN_HR_IF(E_INVALIDARG, (midiIn->DataFormat != MidiDataFormats_ByteStream) && 
            (midiIn->DataFormat != MidiDataFormats_UMP) && 
            (midiIn->DataFormat != MidiDataFormats_Any));
    }

    if (midiOut)
    {
        RETURN_HR_IF(E_INVALIDARG, (midiOut->DataFormat != MidiDataFormats_ByteStream) && 
            (midiOut->DataFormat != MidiDataFormats_UMP) &&
            (midiOut->DataFormat != MidiDataFormats_Any));
    }

    m_MidiInCallback = midiInCallback;
    m_MidiInCallbackContext = context;
    m_ThreadTerminateEvent.create(wil::EventOptions::ManualReset);
    m_ThreadStartedEvent.create(wil::EventOptions::ManualReset);
    m_MmcssTaskId = *mmcssTaskId;

    // now that we know we will succeed, take ownership of the
    // mapped pipes.
    m_MidiIn = std::move(midiIn);
    m_MidiOut = std::move(midiOut);

    // true if incoming messages from the device should get a timestamp
    // we never timestamp outgoing messages TO the device, because 0 has
    // "send immediately" semantics, similar to other operating systems.
    // This is ultimately set by the client manager using a value from
    // the property store, which was written by the transport at
    // enumeration time
    m_OverwriteZeroTimestamp = overwriteZeroTimestamp;

    // if we have midi in, create our worker.
    if (m_MidiIn)
    {
        m_ThreadHandle.reset(CreateThread(nullptr, 0, MidiInWorker, this, 0, nullptr));
        RETURN_LAST_ERROR_IF_NULL(m_ThreadHandle);

        RETURN_LAST_ERROR_IF(FALSE == m_ThreadStartedEvent.wait(1000));
    }

    // return the task id actually used by the worker thread, if one was
    // created
    *mmcssTaskId = m_MmcssTaskId;

    return S_OK;
}

HRESULT
CMidiXProc::Shutdown()
{
    // if a worker has configured mmcss and hasn't yet cleaned
    // it up, this is our last chance
    DisableMmcss(m_MmcssHandle);

    if (m_ThreadHandle)
    {
        // standard AVStream has an ioctl blocked on the read, which will not
        // end until the pin is closed. So, if we are not looped, we need
        // to clean up the base class first, and then the streaming thread.
        // once the pin was closed it'll unblock and exit the worker thread
        // due to the ioctl failure, allowing for cleanup to complete
        m_ThreadTerminateEvent.SetEvent();
        RETURN_LAST_ERROR_IF(WAIT_OBJECT_0 != WaitForSingleObject(m_ThreadHandle.get(), INFINITE));
        m_ThreadHandle.reset();
    }

    m_MidiIn.reset();
    m_MidiOut.reset();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiXProc::SendMidiMessage(
    MessageOptionFlags optionFlags,
    void * midiData,
    UINT32 length,
    LONGLONG position
)
{
    bool bufferSent{false};
    UINT32 requiredBufferSize = sizeof(LOOPEDDATAFORMAT) + length;

    RETURN_HR_IF(E_UNEXPECTED, !m_MidiOut);
    RETURN_HR_IF(E_INVALIDARG, length < MINIMUM_LOOPED_DATASIZE);
    RETURN_HR_IF(E_INVALIDARG, (m_MidiOut->DataFormat != MidiDataFormats_ByteStream) && 
                                (m_MidiOut->DataFormat != MidiDataFormats_UMP));

    if (m_MidiOut->DataFormat == MidiDataFormats_ByteStream)
    {
        RETURN_HR_IF(E_INVALIDARG, length > MAXIMUM_LOOPED_BYTESTREAM_DATASIZE);
    }
    else
    {
        // TODO: This value needs to change. We may want to use something bigger like 16k

        RETURN_HR_IF(E_INVALIDARG, length > MAXIMUM_LOOPED_UMP_DATASIZE);
    }

    PMEMORY_MAPPED_REGISTERS registers = &(m_MidiOut->Registers);
    PMEMORY_MAPPED_DATA data = &(m_MidiOut->Data);
    bool retry {false};
    ULONG bufferWrittenPosition {0};
    ULONG startingReadPosition {0};

    // include the option flags provided at initialization
    optionFlags = (MessageOptionFlags) (optionFlags | m_MidiOut->MessageOptions);

    {
        // only 1 caller may add a message to the xproc queue at a time
        auto lock = m_MessageSendLock.lock();

        do{
            retry = false;

            // reset the read event, so we will know when the client reads data in the event
            // that the buffer is full
            m_MidiOut->ReadEvent.ResetEvent();

            // the write position is the last position we have written,
            // the read position is the last position the driver (or client) has read from
            ULONG writePosition = InterlockedCompareExchange((LONG*)registers->WritePosition, 0, 0);
            ULONG readPosition = InterlockedCompareExchange((LONG*)registers->ReadPosition, 0, 0);
            ULONG newWritePosition = (writePosition + requiredBufferSize) % data->BufferSize;
            ULONG bytesAvailable{ 0 };

            // Calculate the available space in the buffer.
            if (readPosition <= writePosition)
            {
                bytesAvailable = data->BufferSize - (writePosition - readPosition);
            }
            else
            {
                bytesAvailable = (readPosition - writePosition);
            }

            // Note, if we fill the buffer up 100%, then write position == read position,
            // which is the same as when the buffer is empty and everything in the buffer
            // would be lost.
            // Reserve 1 byte so that when the buffer is full the write position will trail
            // the read position.
            // Because of this reserve, and the above calculation, the true bytesAvailable 
            // count can never be 0.
            assert(bytesAvailable != 0);
            bytesAvailable--;

            // if there is sufficient space to write the buffer, send it
            if (bytesAvailable >= requiredBufferSize)
            {
                PLOOPEDDATAFORMAT header = (PLOOPEDDATAFORMAT)(((BYTE*)data->BufferAddress) + writePosition);

                header->ByteCount = length;
                CopyMemory((((BYTE*)header) + sizeof(LOOPEDDATAFORMAT)), midiData, length);

                // if a position provided is nonzero, use it, otherwise use the current QPC
                if (position)
                {
                    header->Position = position;
                }
                else if (m_OverwriteZeroTimestamp)
                {
                    LARGE_INTEGER qpc{ 0 };
                    QueryPerformanceCounter(&qpc);
                    header->Position = qpc.QuadPart;
                }
                else
                {
                    header->Position = 0;
                }

                // update the write position and notify the other side that data is available.
                InterlockedExchange((LONG*)registers->WritePosition, newWritePosition);
                RETURN_LAST_ERROR_IF(FALSE == SetEvent(m_MidiOut->WriteEvent.get()));

                bufferSent = true;

                // Retain the position that the buffer was written into, if the client requires
                // us to wait for it to complete.
                bufferWrittenPosition = writePosition;
                startingReadPosition = readPosition;

                if (m_PipeStalled)
                {
                    m_PipeStalled = false;
                    m_SequentialDroppedBuffers = 0;

                    // Pipe was previously stalled, however space is available now
                    // pipe has recovered and is no longer stalled.
                    TraceLoggingWrite(
                        MidiXProcTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"MidiXProc recovered from stall.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingPointer(midiData, "midiData"),
                        TraceLoggingValue(readPosition, "readPosition"),
                        TraceLoggingValue(writePosition, "writePosition"),
                        TraceLoggingValue(m_PipeStalled, "PipeStalled"),
                        TraceLoggingValue(m_SequentialDroppedBuffers, "SequentialDroppedBuffers"),
                        TraceLoggingValue(m_TotalDroppedBuffers, "TotalDroppedBuffers")
                    );
                }
            }
            else if (!m_PipeStalled)
            {
                // Buffer is full, and either the pipe has not been detected as being stalled, or the client requested 
                // to always wait for a retry. Wait for the client to read some data out of the buffer to
                // make space
                HANDLE handles[] = { m_ThreadTerminateEvent.get(), m_MidiOut->ReadEvent.get() };
                DWORD ret = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, MIDI_XPROC_BUFFER_FULL_WAIT_TIMEOUT);
                if (ret == (WAIT_OBJECT_0 + 1))
                {
                    // space has been made, try again.
                    retry = true;
                    continue;
                }

                // We're either terminating, or space did not come available, so this buffer is going to be dropped
                m_TotalDroppedBuffers++;

                if (ret == WAIT_OBJECT_0)
                {
                    // We're terminating
                    TraceLoggingWrite(
                        MidiXProcTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"MidiXProc terminated, buffer dropped.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingPointer(midiData, "midiData"),
                        TraceLoggingValue(readPosition, "readPosition"),
                        TraceLoggingValue(writePosition, "writePosition"),
                        TraceLoggingValue(m_PipeStalled, "PipeStalled"),
                        TraceLoggingValue(m_SequentialDroppedBuffers, "SequentialDroppedBuffers"),
                        TraceLoggingValue(m_TotalDroppedBuffers, "TotalDroppedBuffers")
                    );
                }
                else if (ret == WAIT_TIMEOUT)
                {
                    m_PipeStalled = true;                    

                    // Pipe is running and we timed out waiting for space.
                    // The pipe is stalled, set a flag so we skip waiting the full timeout for additional messages
                    // that come in while the pipe is stalled, however we will still check to see if the pipe has
                    // recovered when new messages arrive.
                    TraceLoggingWrite(
                        MidiXProcTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"MidiXProc stall detected, buffer dropped.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingPointer(midiData, "midiData"),
                        TraceLoggingValue(readPosition, "readPosition"),
                        TraceLoggingValue(writePosition, "writePosition"),
                        TraceLoggingValue(m_PipeStalled, "PipeStalled"),
                        TraceLoggingValue(m_SequentialDroppedBuffers, "SequentialDroppedBuffers"),
                        TraceLoggingValue(m_TotalDroppedBuffers, "TotalDroppedBuffers")
                    );
                }
            }
            else
            {
                // The pipe is stalled and continues to be stalled, and the client did not request retry, 
                // log that the message has been lost, don't wait for space, and report the failure
                m_TotalDroppedBuffers++;
                m_SequentialDroppedBuffers++;
                TraceLoggingWrite(
                    MidiXProcTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"MidiXProc stalled, buffer dropped.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingPointer(midiData, "midiData"),
                    TraceLoggingValue(readPosition, "readPosition"),
                    TraceLoggingValue(writePosition, "writePosition"),
                    TraceLoggingValue(m_PipeStalled, "PipeStalled"),
                    TraceLoggingValue(m_SequentialDroppedBuffers, "SequentialDroppedBuffers"),
                    TraceLoggingValue(m_TotalDroppedBuffers, "TotalDroppedBuffers")
                );
            }
        }while (retry);
    }

    // if we successfully sent data, and are configured to wait for the transfer to complete
    // wait for the pipe to be emptied. This is done outside of the lock, to enable multiple
    // callers to wait for their send to complete without blocking others from sending.
    if (bufferSent && (MessageOptionFlags_WaitForSendComplete == (optionFlags & MessageOptionFlags_WaitForSendComplete)))
    {
        RETURN_IF_FAILED(WaitForSendComplete(startingReadPosition, bufferWrittenPosition, length));
    }

    // Failed to send the buffer due to insufficient space,
    // fail.
    if (!bufferSent)
    {
        LOG_IF_FAILED(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    return S_OK;
}

HRESULT
CMidiXProc::ProcessMidiIn()
{
    do
    {
        // wait on write event or exit event
        HANDLE handles[] = { m_ThreadTerminateEvent.get(), m_MidiIn->WriteEvent.get() };
        DWORD ret = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
        if (ret == (WAIT_OBJECT_0 + 1))
        {
            PMEMORY_MAPPED_REGISTERS registers = &(m_MidiIn->Registers);
            PMEMORY_MAPPED_DATA mappedData = &(m_MidiIn->Data);

            do
            {
                // if we're processing a backlog of data and recieve a thread termination,
                // stop processing data
                ret = WaitForSingleObject(m_ThreadTerminateEvent.get(), 0);
                if (ret == WAIT_OBJECT_0)
                {
                    break;
                }
                else
                {
                    // we're doing a pass on the buffer, so safe to reset the event right now, no chance
                    // of missing anything if we reset it before we retrieve the current positions.
                    m_MidiIn->WriteEvent.ResetEvent();

                    // the read position is the last position we have read,
                    // the write position is the last position written to
                    ULONG readPosition = InterlockedCompareExchange((LONG*)registers->ReadPosition, 0, 0);
                    ULONG writePosition = InterlockedCompareExchange((LONG*)registers->WritePosition, 0, 0);
                    ULONG bytesAvailable{ 0 };

                    if (readPosition <= writePosition)
                    {
                        bytesAvailable = writePosition - readPosition;
                    }
                    else
                    {
                        bytesAvailable = mappedData->BufferSize - (readPosition - writePosition);
                    }

                    if (0 == bytesAvailable ||
                        bytesAvailable < sizeof(LOOPEDDATAFORMAT))
                    {
                        // nothing to do, need at least the LOOPEDDATAFORMAT
                        // to move forward. Driver will set the event when the
                        // write position advances.
                        break;
                    }

                    PLOOPEDDATAFORMAT header = (PLOOPEDDATAFORMAT)(((BYTE*)mappedData->BufferAddress) + readPosition);
                    UINT32 dataSize = header->ByteCount;
                    UINT32 totalSize = dataSize + sizeof(LOOPEDDATAFORMAT);
                    ULONG newReadPosition = (readPosition + totalSize) % mappedData->BufferSize;

                    if (bytesAvailable < totalSize)
                    {
                        // if the full contents of this buffer isn't yet available,
                        // stop processing and wait for data to come in.
                        // Driver will set an event when the write position advances.
                        break;
                    }

                    PVOID data = (PVOID)(((BYTE*)header) + sizeof(LOOPEDDATAFORMAT));
                    HRESULT hr = S_OK;

                    if (m_MidiInCallback)
                    {
                        // if a position provided is nonzero, use it, otherwise use the current QPC
                        if (header->Position == 0 && m_OverwriteZeroTimestamp)
                        {
                            LARGE_INTEGER qpc{ 0 };
                            QueryPerformanceCounter(&qpc);
                            header->Position = qpc.QuadPart;
                        }

                        hr = m_MidiInCallback->Callback(m_MidiIn->MessageOptions, data, dataSize, header->Position, m_MidiInCallbackContext);
                    }

                    if (FAILED(hr) && (MessageOptionFlags_CallbackRetry == (m_MidiIn->MessageOptions & MessageOptionFlags_CallbackRetry)))
                    {
                        // Sending the message failed and there was a request to retry, 
                        // Wait and retry sending the failed message.
                        ret = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, MIDI_XPROC_CALLBACK_RETRY_TIMEOUT);
                        if (ret == WAIT_OBJECT_0)
                        {
                            // thread terminated, exit.
                            break;
                        }

                        TraceLoggingWrite(
                            MidiXProcTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"MidiXProc callback failed, buffer retrying.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingHResult(hr, "hr")
                        );
                    }
                    else
                    {
                        if (FAILED(hr))
                        {
                            m_TotalDroppedBuffers++;
                            TraceLoggingWrite(
                                MidiXProcTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"MidiXProc callback failed, buffer dropped.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingHResult(hr, "hr"),
                                TraceLoggingValue(m_TotalDroppedBuffers, "TotalDroppedBuffers")
                            );
                        }

                        // advance to the next midi packet, we loop processing them one at a time
                        // until we have processed all that is available for this pass.
                        InterlockedExchange((LONG*)registers->ReadPosition, newReadPosition);
                        RETURN_LAST_ERROR_IF(FALSE == SetEvent(m_MidiIn->ReadEvent.get()));
                    }
                }
            } while (TRUE);
        }
        else
        {
            // exit event or wait failed, exit the thread.
            break;
        }
    } while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
DWORD WINAPI
CMidiXProc::MidiInWorker(
    LPVOID param
)
{
    auto coinit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    CMidiXProc *This = reinterpret_cast<CMidiXProc*>(param);
    if (This)
    {
        // pump should not be started if midi in is not required.
        assert(This->m_MidiIn);
        // Enable MMCSS for the midi in worker thread
        if (SUCCEEDED(EnableMmcss(This->m_MmcssHandle, This->m_MmcssTaskId)))
        {
            // signal that our thread is started, and
            // mmcss is configured, so initialization can
            // retrieve the task id.
            This->m_ThreadStartedEvent.SetEvent();
            This->ProcessMidiIn();
            DisableMmcss(This->m_MmcssHandle);
        }
    }

    return 0;
}

