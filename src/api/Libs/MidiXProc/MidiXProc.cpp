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

#include "WindowsMidiServices.h"

#include <Devpkey.h>
#include "MidiKsDef.h"
#include "MidiDefs.h"
#include "MidiXProc.h"



_Use_decl_annotations_
HRESULT
GetRequiredBufferSize(ULONG& requestedSize
)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    requestedSize = ((requestedSize + systemInfo.dwAllocationGranularity - 1) & ~(systemInfo.dwAllocationGranularity - 1));

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
    m_ThreadTerminateEvent.create();
    m_ThreadStartedEvent.create();
    m_MmcssTaskId = *mmcssTaskId;

    // now that we know we will succeed, take ownership of the
    // mapped pipes.
    m_MidiIn = std::move(midiIn);
    m_MidiOut = std::move(midiOut);

    // true if incoming messages from the device should get a timestamp
    // we never timestamp outgoing messages TO the device, because 0 has
    // "send immediately" semantics, similar to other operating systems.
    // This is ultimately set by the client manager using a value from
    // the property store, which was written by the abstraction at
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
        RETURN_HR_IF(E_INVALIDARG, length > MAXIMUM_LOOPED_UMP_DATASIZE);
    }

    PMEMORY_MAPPED_REGISTERS registers = &(m_MidiOut->Registers);
    PMEMORY_MAPPED_DATA data = &(m_MidiOut->Data);
    bool retry {false};

    do{
        retry = false;

        // reset the read event, so we will know when the client reads data in the event
        // that the buffer is full
        m_MidiOut->ReadEvent.ResetEvent();

        // the write position is the last position we have written,
        // the read position is the last position the driver has read from
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

            // update the write position and notify the other side that data is available.
            InterlockedExchange((LONG*)registers->WritePosition, newWritePosition);
            RETURN_LAST_ERROR_IF(FALSE == SetEvent(m_MidiOut->WriteEvent.get()));

            bufferSent = true;
        }
        else
        {
            // Buffer is full, wait for the client to read some data out of the buffer to
            // make space
            HANDLE handles[] = { m_ThreadTerminateEvent.get(), m_MidiOut->ReadEvent.get() };
            DWORD ret = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
            if (ret == (WAIT_OBJECT_0 + 1))
            {
                // space has been made, try again.
                retry = 1;
                continue;
            }
        }
    }while (retry);

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
            PMEMORY_MAPPED_DATA data = &(m_MidiIn->Data);

            do
            {
                // we're doing a pass on the buffer, so safe to reset the event right now, no chance
                // of missing anything if we reset it before we retrieve the current positions.
                m_MidiIn->WriteEvent.ResetEvent();

                // the read position is the last position we have read,
                // the write position is the last position written to
                ULONG readPosition = InterlockedCompareExchange((LONG*) registers->ReadPosition, 0, 0);
                ULONG writePosition = InterlockedCompareExchange((LONG*) registers->WritePosition, 0, 0);
                ULONG bytesAvailable {0};

                if (readPosition <= writePosition)
                {
                    bytesAvailable = writePosition - readPosition;
                }
                else
                {
                    bytesAvailable = data->BufferSize - (readPosition - writePosition);
                }

                if (0 == bytesAvailable ||
                    bytesAvailable < sizeof(LOOPEDDATAFORMAT))
                {
                    // nothing to do, need at least the LOOPEDDATAFORMAT
                    // to move forward. Driver will set the event when the
                    // write position advances.
                    break;
                }

                PLOOPEDDATAFORMAT header = (PLOOPEDDATAFORMAT) (((BYTE *) data->BufferAddress) + readPosition);
                UINT32 dataSize = header->ByteCount;
                UINT32 totalSize = dataSize + sizeof(LOOPEDDATAFORMAT);
                ULONG newReadPosition = (readPosition + totalSize) % data->BufferSize;

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
                    // if a position provided is nonzero, use it, otherwise use the current QPC
                    if (header->Position == 0 && m_OverwriteZeroTimestamp)
                    {
                        LARGE_INTEGER qpc{ 0 };
                        QueryPerformanceCounter(&qpc);
                        header->Position = qpc.QuadPart;
                    }

                    m_MidiInCallback->Callback(data, dataSize, header->Position, m_MidiInCallbackContext);
                }

                // advance to the next midi packet, we loop processing them one at a time
                // until we have processed all that is available for this pass.
                InterlockedExchange((LONG*) registers->ReadPosition, newReadPosition);
                RETURN_LAST_ERROR_IF(FALSE == SetEvent(m_MidiIn->ReadEvent.get()));
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

