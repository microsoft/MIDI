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

#include "MidiAbstraction.h"

#include <Devpkey.h>
#include "MidiKsDef.h"
#include "MidiDefs.h"
#include "MidiXProc.h"

_Use_decl_annotations_
HRESULT
GetRequiredBufferSize(ULONG& RequestedSize
)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    RequestedSize = ((RequestedSize + SystemInfo.dwAllocationGranularity - 1) & ~(SystemInfo.dwAllocationGranularity - 1));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MapBuffer(BOOL DoubleMapped,
            ULONG Size,
            PMEMORY_MAPPED_BUFFER BufferAllocationData
)
{
    PBYTE MapAddress = nullptr;

    if (DoubleMapped)
    {
        wil::unique_virtualalloc_ptr<void> placeholderAlloc;
        placeholderAlloc.reset(VirtualAlloc(nullptr, 2 * (SIZE_T) Size, MEM_RESERVE, PAGE_READWRITE));
        RETURN_IF_NULL_ALLOC(placeholderAlloc);
        MapAddress = (PBYTE)placeholderAlloc.get();
        placeholderAlloc.reset();

        BufferAllocationData->Map2.reset(MapViewOfFileEx(BufferAllocationData->FileMapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, Size, MapAddress + Size));
        RETURN_IF_NULL_ALLOC(BufferAllocationData->Map2);
    }

    BufferAllocationData->Map1.reset(MapViewOfFileEx(BufferAllocationData->FileMapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, Size, MapAddress));
    RETURN_IF_NULL_ALLOC(BufferAllocationData->Map1);
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CreateMappedBuffer(BOOL DoubleMapped,
                    ULONG Size,
                    PMEMORY_MAPPED_BUFFER BufferAllocationData
)
{
    if (!BufferAllocationData->FileMapping)
    {
        BufferAllocationData->FileMapping.reset(CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, Size, nullptr));
        RETURN_LAST_ERROR_IF_NULL(BufferAllocationData->FileMapping);
    }
    RETURN_IF_FAILED(MapBuffer(DoubleMapped, Size, BufferAllocationData));
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CreateMappedDataBuffer(ULONG RequestedSize,
                            PMEMORY_MAPPED_BUFFER Buffer,
                            PMEMORY_MAPPED_DATA Data
)
{
    if (Data->BufferSize == 0)
    {
        // If the buffer size is not already known
        // determine a buffer size that is compatible with
        // the requested size, which cannot be 0.
        RETURN_HR_IF(E_INVALIDARG, 0 == RequestedSize);
        RETURN_IF_FAILED(GetRequiredBufferSize(RequestedSize));
        Data->BufferSize = RequestedSize;
    }
    else
    {
        // The buffer has already been allocated is just being mapped,
        // the requested size is unused and should be 0.
        RETURN_HR_IF(E_INVALIDARG, 0 != RequestedSize);
    }

    RETURN_IF_FAILED(CreateMappedBuffer(TRUE, Data->BufferSize, Buffer));

    Data->BufferAddress = (PBYTE)Buffer->Map1.get();
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CreateMappedRegisters(PMEMORY_MAPPED_BUFFER Buffer,
                        PMEMORY_MAPPED_REGISTERS Registers
)
{
    RETURN_IF_FAILED(CreateMappedBuffer(FALSE, sizeof(ULONG) * 2, Buffer));
    Registers->ReadPosition = (PULONG)Buffer->Map1.get();
    Registers->WritePosition = Registers->ReadPosition + 1;
    return S_OK;
}

_Use_decl_annotations_
HRESULT
DisableMmcss(
    unique_mmcss_handle& MmcssHandle
)
{
#if 1
    RETURN_HR_IF(S_OK, NULL == MmcssHandle);
    // Detach the thread from the MMCSS service to free the handle.
    MmcssHandle.reset();
#else
    UNREFERENCED_PARAMETER(MmcssHandle);
#endif

    return S_OK;
}

_Use_decl_annotations_
HRESULT
EnableMmcss(
    unique_mmcss_handle& MmcssHandle,
    DWORD& MmcssTaskId
)
{
#if 1
    MmcssHandle.reset(AvSetMmThreadCharacteristics( L"Pro Audio", &MmcssTaskId ));
    if (!MmcssHandle)
    {
        // If the task id has gone invalid, try with a new task id
        MmcssTaskId = 0;
        MmcssHandle.reset(AvSetMmThreadCharacteristics( L"Pro Audio", &MmcssTaskId ));
    }

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        MmcssHandle.reset();
    });

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), FALSE == AvSetMmThreadPriority(MmcssHandle.get(), AVRT_PRIORITY_HIGH));
    cleanupOnFailure.release();
#else
    UNREFERENCED_PARAMETER(MmcssHandle);
    UNREFERENCED_PARAMETER(MmcssTaskId);

    SetPriorityClass(GetCurrentThread(), HIGH_PRIORITY_CLASS);
    SetPriorityClass(GetCurrentThread(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    return S_OK;
}

CMidiXProc::~CMidiXProc()
{
    Cleanup();
}

_Use_decl_annotations_
HRESULT
CMidiXProc::Initialize(DWORD* MmcssTaskId,
                        std::unique_ptr<MEMORY_MAPPED_PIPE>& MidiIn,
                        std::unique_ptr<MEMORY_MAPPED_PIPE>& MidiOut,
                        IMidiCallback *MidiInCallback,
                        LONGLONG Context
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmcssTaskId);
    RETURN_HR_IF(E_INVALIDARG, !MidiIn && !MidiOut);
    RETURN_HR_IF(E_INVALIDARG, MidiIn && !MidiInCallback);

    m_MidiInCallback = MidiInCallback;
    m_MidiInCallbackContext = Context;
    m_ThreadTerminateEvent.create();
    m_ThreadStartedEvent.create();
    m_MmcssTaskId = *MmcssTaskId;

    // now that we know we will succeed, take ownership of the
    // mapped pipes.
    m_MidiIn = std::move(MidiIn);
    m_MidiOut = std::move(MidiOut);

    // if we have midi in, create our worker.
    if (m_MidiIn)
    {
        m_ThreadHandle.reset(CreateThread(nullptr, 0, MidiInWorker, this, 0, nullptr));
        RETURN_LAST_ERROR_IF_NULL(m_ThreadHandle);

        RETURN_LAST_ERROR_IF(FALSE == m_ThreadStartedEvent.wait(1000));
    }

    // return the task id actually used by the worker thread, if one was
    // created
    *MmcssTaskId = m_MmcssTaskId;

    return S_OK;
}

HRESULT
CMidiXProc::Cleanup()
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
    void * MidiData,
    UINT32 Length,
    LONGLONG Position
)
{
    BOOL bufferSent {FALSE};
    UINT32 requiredBufferSize = sizeof(LOOPEDDATAFORMAT) + Length;
    UINT maxRetries{ 10000 };

    RETURN_HR_IF(E_UNEXPECTED, !m_MidiOut);
    RETURN_HR_IF(E_INVALIDARG, Length > MAXIMUM_LOOPED_DATASIZE);
    RETURN_HR_IF(E_INVALIDARG, Length < MINIMUM_LOOPED_DATASIZE);

    PMEMORY_MAPPED_REGISTERS Registers = &(m_MidiOut->Registers);
    PMEMORY_MAPPED_DATA Data = &(m_MidiOut->Data);

    do{
        // the write position is the last position we have written,
        // the read position is the last position the driver has read from
        ULONG writePosition = InterlockedCompareExchange((LONG*) Registers->WritePosition, 0, 0);
        ULONG readPosition = InterlockedCompareExchange((LONG*) Registers->ReadPosition, 0, 0);
        ULONG newWritePosition = (writePosition + requiredBufferSize) % Data->BufferSize;
        ULONG bytesAvailable {0};

        // Calculate the available space in the buffer.
        if (readPosition <= writePosition)
        {
            bytesAvailable = Data->BufferSize - (writePosition - readPosition);
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
            PLOOPEDDATAFORMAT header = (PLOOPEDDATAFORMAT) (((BYTE *) Data->BufferAddress) + writePosition);

            header->ByteCount = Length;
            CopyMemory((((BYTE *) header) + sizeof(LOOPEDDATAFORMAT)), MidiData, Length);

            // if a position provided is nonzero, use it, otherwise use the current QPC
            if (Position)
            {
                header->Position = Position;
            }
            else
            {
                LARGE_INTEGER qpc {0};
                QueryPerformanceCounter(&qpc);
                header->Position = qpc.QuadPart;
            }

            // update the write position and notify the other side that data is available.
            InterlockedExchange((LONG*) Registers->WritePosition, newWritePosition);
            RETURN_LAST_ERROR_IF(FALSE == SetEvent(m_MidiOut->WriteEvent.get()));
            bufferSent = TRUE;
        }
        else
        {
            // relenquish the remainder of this processing slice and try again on the next
            Sleep(0);
        }
    }while (!bufferSent && --maxRetries > 0);

    // Failed to send the buffer due to insufficient space,
    // fail.
    if (!bufferSent)
    {
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
            PMEMORY_MAPPED_REGISTERS Registers = &(m_MidiIn->Registers);
            PMEMORY_MAPPED_DATA Data = &(m_MidiIn->Data);

            do
            {
                // we're doing a pass on the buffer, so safe to reset the event right now, no chance
                // of missing anything if we reset it before we retrieve the current positions.
                m_MidiIn->WriteEvent.ResetEvent();

                // the read position is the last position we have read,
                // the write position is the last position written to
                ULONG readPosition = InterlockedCompareExchange((LONG*) Registers->ReadPosition, 0, 0);
                ULONG writePosition = InterlockedCompareExchange((LONG*) Registers->WritePosition, 0, 0);
                ULONG bytesAvailable {0};

                if (readPosition <= writePosition)
                {
                    bytesAvailable = writePosition - readPosition;
                }
                else
                {
                    bytesAvailable = Data->BufferSize - (readPosition - writePosition);
                }

                if (0 == bytesAvailable ||
                    bytesAvailable < sizeof(LOOPEDDATAFORMAT))
                {
                    // nothing to do, need at least the LOOPEDDATAFORMAT
                    // to move forward. Driver will set the event when the
                    // write position advances.
                    break;
                }

                PLOOPEDDATAFORMAT header = (PLOOPEDDATAFORMAT) (((BYTE *) Data->BufferAddress) + readPosition);
                UINT32 dataSize = header->ByteCount;
                UINT32 totalSize = dataSize + sizeof(LOOPEDDATAFORMAT);
                ULONG newReadPosition = (readPosition + totalSize) % Data->BufferSize;

                if (bytesAvailable < totalSize)
                {
                    // if the full contents of this buffer isn't yet available,
                    // stop processing and wait for data to come in.
                    // Driver will set an event when the write position advances.
                    break;
                }

                PVOID data = (PVOID) (((BYTE *) header) + sizeof(LOOPEDDATAFORMAT));

                if (m_MidiInCallback)
                {
                    m_MidiInCallback->Callback(data, dataSize, header->Position, m_MidiInCallbackContext);
                }

                // advance to the next midi packet, we loop processing them one at a time
                // until we have processed all that is available for this pass.
                InterlockedExchange((LONG*) Registers->ReadPosition, newReadPosition);
            } while(TRUE);
        }
        else
        {
            // exit event or wait failed, exit the thread.
            break;
        }
    }while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
DWORD WINAPI
CMidiXProc::MidiInWorker(
    LPVOID lpParam
)
{
    auto coinit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    CMidiXProc *This = reinterpret_cast<CMidiXProc*>(lpParam);
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

