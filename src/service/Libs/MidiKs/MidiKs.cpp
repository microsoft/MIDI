// Copyright (c) Microsoft Corporation. All rights reserved.
#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS

#include <Windows.Devices.Enumeration.h>
#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include "MidiAbstraction_i.c"
#include "MidiAbstraction.h"

#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiKs.h"

using namespace concurrency;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

KSMidiDevice::~KSMidiDevice()
{
    Cleanup();
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::Initialize(
    LPCWSTR Device,
    HANDLE Filter,
    UINT PinId,
    BOOL UseCyclic,
    BOOL UseUMP,
    ULONG BufferSize
)
{
    m_IsLooped = UseCyclic;
    m_IsUMP = UseUMP;
    m_LoopedBufferSize = BufferSize;

    m_FilterFilename = wil::make_cotaskmem_string_nothrow(Device);
    RETURN_IF_NULL_ALLOC(m_FilterFilename);

    if (!Filter)
    {
        RETURN_IF_FAILED(FilterInstantiate(m_FilterFilename.get(), &m_Filter));
    }
    else
    {
        RETURN_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), Filter, GetCurrentProcess(), &m_Filter, 0, FALSE, DUPLICATE_SAME_ACCESS));
    }

    m_PinID = PinId;

    RETURN_IF_FAILED(OpenStream());
    RETURN_IF_FAILED(PinSetState(KSSTATE_ACQUIRE));
    RETURN_IF_FAILED(PinSetState(KSSTATE_PAUSE));
    RETURN_IF_FAILED(PinSetState(KSSTATE_RUN));

    return S_OK;
}

HRESULT
KSMidiDevice::OpenStream()
{
    RETURN_IF_FAILED(InstantiateMidiPin(m_Filter.get(), m_PinID, m_IsLooped, m_IsUMP, &m_Pin));

    if (m_IsLooped)
    {
        // if we're looped (cyclic buffer), we need to
        // configure the buffer, registers, and event.
        RETURN_IF_FAILED(ConfigureLoopedBuffer());
        RETURN_IF_FAILED(ConfigureLoopedRegisters());
        RETURN_IF_FAILED(ConfigureLoopedEvent());
    }

    return S_OK;
}

HRESULT
KSMidiDevice::Cleanup()
{
    if (nullptr != m_Pin.get())
    {
        PinSetState(KSSTATE_PAUSE);
        PinSetState(KSSTATE_ACQUIRE);
        PinSetState(KSSTATE_STOP);
    }

    m_Pin.reset();
    m_Filter.reset();

    // if a worker has configured mmcss and hasn't yet cleaned
    // it up, this is our last chance
    DisableMmcss();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::PinSetState(
    KSSTATE PinState
)
{
    KSPROPERTY property {0};
    ULONG propertySize {sizeof(property)};

    property.Set    = KSPROPSETID_Connection; 
    property.Id     = KSPROPERTY_CONNECTION_STATE;       
    property.Flags  = KSPROPERTY_TYPE_SET;

    RETURN_IF_FAILED(SyncIoctl(
        m_Pin.get(),
        IOCTL_KS_PROPERTY,
        &property,
        propertySize,
        &PinState,
        sizeof(KSSTATE),
        nullptr));

    return S_OK;
}

HRESULT
KSMidiDevice::ConfigureLoopedBuffer()
{
    KSMIDILOOPED_BUFFER_PROPERTY property {0};
    ULONG propertySize {sizeof(property)};

    property.Property.Set           = KSPROPSETID_MidiLoopedStreaming; 
    property.Property.Id            = KSPROPERTY_MIDILOOPEDSTREAMING_BUFFER;       
    property.Property.Flags         = KSPROPERTY_TYPE_GET;

    // Seems to be a reasonable balance for now,
    // TBD make this configurable via api or registry.
    property.RequestedBufferSize    = 4 * PAGE_SIZE;

    RETURN_IF_FAILED(SyncIoctl(
        m_Pin.get(),
        IOCTL_KS_PROPERTY,
        &property,
        propertySize,
        &m_LoopedBuffer,
        sizeof(m_LoopedBuffer),
        nullptr));

    return S_OK;
}

HRESULT
KSMidiDevice::ConfigureLoopedRegisters()
{
    KSPROPERTY property {0};
    ULONG propertySize {sizeof(property)};

    property.Set    = KSPROPSETID_MidiLoopedStreaming; 
    property.Id     = KSPROPERTY_MIDILOOPEDSTREAMING_REGISTERS;       
    property.Flags  = KSPROPERTY_TYPE_GET;

    RETURN_IF_FAILED(SyncIoctl(
        m_Pin.get(),
        IOCTL_KS_PROPERTY,
        &property,
        propertySize,
        &m_LoopedRegisters,
        sizeof(m_LoopedRegisters),
        nullptr));

    return S_OK;
}

HRESULT
KSMidiDevice::ConfigureLoopedEvent()
{
    KSPROPERTY property {0};
    ULONG propertySize {sizeof(property)};
    KSMIDILOOPED_EVENT LoopedEvent {0};

    m_BufferWriteEvent.create();
    LoopedEvent.WriteEvent = m_BufferWriteEvent.get();

    property.Set    = KSPROPSETID_MidiLoopedStreaming; 
    property.Id     = KSPROPERTY_MIDILOOPEDSTREAMING_NOTIFICATION_EVENT;       
    property.Flags  = KSPROPERTY_TYPE_SET;

    RETURN_IF_FAILED(SyncIoctl(
        m_Pin.get(),
        IOCTL_KS_PROPERTY,
        &property,
        propertySize,
        &LoopedEvent,
        sizeof(LoopedEvent),
        nullptr));

    return S_OK;
}

HRESULT
KSMidiDevice::DisableMmcss()
{
    RETURN_HR_IF(S_OK, NULL == m_MmcssHandle);
    // Detach the thread from the MMCSS service to free the handle.
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), FALSE == AvRevertMmThreadCharacteristics(m_MmcssHandle));
    m_MmcssHandle = NULL;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::EnableMmcss(
    DWORD* MmcssTask
)
{
    // If a task id has been provided, initialize with that task id
    if (MmcssTask)
    {
        m_MmcssTaskId = *MmcssTask;
    }

    m_MmcssHandle = AvSetMmThreadCharacteristics( L"Pro Audio", &m_MmcssTaskId );
    if (NULL == m_MmcssHandle)
    {
        // If the task id has gone invalid, try with a new task id
        m_MmcssTaskId = 0;
        m_MmcssHandle = AvSetMmThreadCharacteristics( L"Pro Audio", &m_MmcssTaskId );
    }

    auto cleanupOnFailure = wil::scope_exit([&]()
    {
        if (m_MmcssHandle)
        {
            DisableMmcss();
        }
    });

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), FALSE == AvSetMmThreadPriority(m_MmcssHandle, AVRT_PRIORITY_HIGH));
    cleanupOnFailure.release();

    // return the task id that was actually used,
    // which will happen if the incoming task id is 0
    if (MmcssTask)
    {
        *MmcssTask = m_MmcssTaskId;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::Initialize(
    LPCWSTR Device,
    HANDLE Filter,
    UINT PinId,
    BOOL Cyclic,
    BOOL UseUMP,
    ULONG BufferSize,
    DWORD* MmcssTaskId
)
{
    RETURN_IF_FAILED(EnableMmcss(MmcssTaskId));
    RETURN_IF_FAILED(KSMidiDevice::Initialize(Device, Filter, PinId, Cyclic, UseUMP, BufferSize));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::WriteMidiData(
    void * MidiData,
    UINT32 length,
    LONGLONG position
)
{
    // The length must be one of the valid UMP data lengths
    RETURN_HR_IF(E_INVALIDARG, length == 0);

    if (m_IsLooped)
    {
        // using cyclic, so write via cyclic
        return WriteLoopedMidiData(MidiData, length, position);
    }
    else
    {
        // using standard, write via standard streaming ioctls
        return WritePacketMidiData(MidiData, length, position);
    }
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::WriteLoopedMidiData(
    void * MidiData,
    UINT32 length,
    LONGLONG position
)
{
    BOOL bufferSent {FALSE};
    UINT32 requiredBufferSize = sizeof(UMPDATAFORMAT) + length;

    do{
        // the write position is the last position we have written,
        // the read position is the last position the driver has read from
        ULONG writePosition = InterlockedCompareExchange((LONG*) m_LoopedRegisters.WritePosition, 0, 0);
        ULONG readPosition = InterlockedCompareExchange((LONG*) m_LoopedRegisters.ReadPosition, 0, 0);
        ULONG newWritePosition = (writePosition + requiredBufferSize) % m_LoopedBuffer.ActualBufferSize;
        ULONG bytesAvailable {0};

        // Calculate the available space in the buffer.
        if (readPosition <= writePosition)
        {
            bytesAvailable = m_LoopedBuffer.ActualBufferSize - (writePosition - readPosition);
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
            PUMPDATAFORMAT header = (PUMPDATAFORMAT) (((BYTE *) m_LoopedBuffer.BufferAddress) + writePosition);

            header->ByteCount = length;
            CopyMemory((((BYTE *) header) + sizeof(UMPDATAFORMAT)), MidiData, length);

            // if a position provided is nonzero, use it, otherwise use the current QPC
            if (position)
            {
                header->Position = position;
            }
            else
            {
                LARGE_INTEGER qpc {0};
                QueryPerformanceCounter(&qpc);
                header->Position = qpc.QuadPart;
            }

            // update the write position and notify the other side that data is available.
            InterlockedExchange((LONG*) m_LoopedRegisters.WritePosition, newWritePosition);
            m_BufferWriteEvent.SetEvent();
            bufferSent = TRUE;
        }
        else
        {
            // There is not sufficient space to send, current strategy is to delay and retry.

            // TODO: If the buffer is full because the driver is wedged, this will just
            // sit here forever retrying. Should there be a timeout?
            bufferSent = FALSE;

            // relenquish the remainder of this processing slice and try again on the next
            Sleep(0);
        }
    }while (!bufferSent);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::WritePacketMidiData(
    void* MidiData,
    UINT32 Length,
    LONGLONG Position
)
{
    KSSTREAM_HEADER kssh {0};
    KSMUSICFORMAT *event = nullptr;
    UINT32 totalLength = 0;

    // total size of the payload is the leading structure + message size
    totalLength = DWORD_ALIGN(sizeof(KSMUSICFORMAT) + Length);

    // allocate and zero an event, so we can send everything as a single message
    event = reinterpret_cast<KSMUSICFORMAT*>(new BYTE[totalLength]{0});
    RETURN_HR_IF(E_OUTOFMEMORY, nullptr == event);

    auto cleanupOnExit = wil::scope_exit([&]()
    {
        delete[] event;
    });

    // The byte count here is the size of the actual midi message, without
    // leading KSMUSICFORMAT structure size or padding
    event->ByteCount = Length;

    // now we can copy over the message data into the KSMUSICFORMAT
    CopyMemory(event + 1, MidiData, Length);

    kssh.Size = sizeof(KSSTREAM_HEADER);

    // mirroring win32 midi, presenatation time on outgoing messages
    // is always 1.
    kssh.PresentationTime.Numerator = 1;
    kssh.PresentationTime.Denominator = 1;

    // for legacy midi 1, if the position provided was 0, use 0
    kssh.PresentationTime.Time = Position;

    // the length here is the allocated size of event, so 
    // header structure + data + padding
    kssh.DataUsed = kssh.FrameExtent = totalLength;

    kssh.Data = reinterpret_cast<BYTE*>(event);

    HRESULT hr = SyncIoctl(
            m_Pin.get(),
            IOCTL_KS_WRITE_STREAM,
            nullptr,
            0,
            (UCHAR*)&kssh,
            kssh.Size,
            nullptr);

    RETURN_IF_FAILED(hr);

    return S_OK;
}

HRESULT
KSMidiInDevice::Cleanup()
{
    // Looped threads have a terminate event and are directly accessing
    // buffers allocated by the pin. So, we need to terminate this thread
    // before we can clean up the base class.
    if (m_IsLooped && m_ThreadHandle)
    {
        m_ThreadTerminateEvent.SetEvent();
        WaitForSingleObject(m_ThreadHandle.get(), INFINITE);
        m_ThreadHandle.reset();
    }

    // safe to clean up the base class now that any looped
    // worker threads are cleaned up.
    HRESULT hr = KSMidiDevice::Cleanup();

    if (m_ThreadHandle)
    {
        // standard AVStream has an ioctl blocked on the read, which will not
        // end until the pin is closed. So, if we are not looped, we need
        // to clean up the base class first, and then the streaming thread.
        // once the pin was closed it'll unblock and exit the worker thread
        // due to the ioctl failure, allowing for cleanup to complete
        m_ThreadTerminateEvent.SetEvent();
        WaitForSingleObject(m_ThreadHandle.get(), INFINITE);
        m_ThreadHandle.reset();
    }

    return hr;
}

#define MIDI_DATA_BUFFER_LENGTH 256

typedef struct
{
    KSMUSICFORMAT   ksMusicFormat;
    BYTE            abInputBuffer[MIDI_DATA_BUFFER_LENGTH];
} MIDI_EVENT;

HRESULT
KSMidiInDevice::SendRequestToDriver()
{
    while (true)
    {
        MIDI_EVENT event {0};
        KSSTREAM_HEADER kssh {0};

        // the KSSTREAM_HEADER struct must be reinitialized before
        // each call, or we get an error when we try to call ReadData
        kssh.Size = sizeof(KSSTREAM_HEADER);

        kssh.PresentationTime.Numerator = 1;
        kssh.PresentationTime.Denominator = 1;

        kssh.FrameExtent = sizeof(event);
        kssh.Data = &event;

        HRESULT hr = SyncIoctl(
                m_Pin.get(),
                IOCTL_KS_READ_STREAM,
                nullptr,
                0,
                (UCHAR*)&kssh,
                kssh.Size,
                nullptr);
        if (SUCCEEDED(hr))
        {
            UINT32 payloadSize = event.ksMusicFormat.ByteCount;
            PVOID data = reinterpret_cast<BYTE*>(&event.ksMusicFormat + 1);

            // if payload size is 0, no need to call back, when the handle
            // is closed and the SyncIoctl returns, it may succeed, but have no data.
            if (m_MidiInCallback && payloadSize > 0)
            {
                m_MidiInCallback->Callback(data, payloadSize, kssh.PresentationTime.Time);
            }
        }
        else
        {
            return hr;
        }
    }
}

HRESULT
KSMidiInDevice::ProcessLoopedMidiIn()
{
    do
    {
        // wait on write event or exit event
        HANDLE handles[] = { m_BufferWriteEvent.get(), m_ThreadTerminateEvent.get() };
        DWORD ret = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
        if (ret == WAIT_OBJECT_0)
        {
            do
            {
                // the read position is the last position we have read,
                // the write position is the last position written to
                ULONG readPosition = InterlockedCompareExchange((LONG*) m_LoopedRegisters.ReadPosition, 0, 0);
                ULONG writePosition = InterlockedCompareExchange((LONG*) m_LoopedRegisters.WritePosition, 0, 0);
                ULONG bytesAvailable {0};

                if (readPosition <= writePosition)
                {
                    bytesAvailable = writePosition - readPosition;
                }
                else
                {
                    bytesAvailable = m_LoopedBuffer.ActualBufferSize - (readPosition - writePosition);
                }

                if (0 == bytesAvailable ||
                    bytesAvailable < sizeof(UMPDATAFORMAT))
                {
                    // nothing to do, need at least the UMPDATAFORMAT
                    // to move forward. Driver will set the event when the
                    // write position advances.
                    break;
                }

                PUMPDATAFORMAT header = (PUMPDATAFORMAT) (((BYTE *) m_LoopedBuffer.BufferAddress) + readPosition);
                UINT32 dataSize = header->ByteCount;
                UINT32 totalSize = dataSize + sizeof(UMPDATAFORMAT);
                ULONG newReadPosition = (readPosition + totalSize) % m_LoopedBuffer.ActualBufferSize;

                if (bytesAvailable < totalSize)
                {
                    // if the full contents of this buffer isn't yet available,
                    // stop processing and wait for data to come in.
                    // Driver will set an event when the write position advances.
                    break;
                }

                PVOID data = (PVOID) (((BYTE *) header) + sizeof(UMPDATAFORMAT));

                if (m_MidiInCallback)
                {
                    m_MidiInCallback->Callback(data, dataSize, header->Position);
                }

                // advance to the next midi packet, we loop processing them one at a time
                // until we have processed all that is available for this pass.
                InterlockedExchange((LONG*) m_LoopedRegisters.ReadPosition, newReadPosition);
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
KSMidiInDevice::MidiInWorker(
    LPVOID lpParam
)
{
    auto coinit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    KSMidiInDevice *This = reinterpret_cast<KSMidiInDevice*>(lpParam);
    if (This)
    {
        // Enable MMCSS for the midi in worker thread
        if (SUCCEEDED(This->EnableMmcss(nullptr)))
        {
            // signal that our thread is started, and
            // mmcss is configured, so initialization can
            // retrieve the task id.
            This->m_ThreadStartedEvent.SetEvent();
            if (This->m_IsLooped)
            {
                This->ProcessLoopedMidiIn();
            }
            else
            {
                This->SendRequestToDriver();
            }

            This->DisableMmcss();
        }
    }

    return 0;
}

_Use_decl_annotations_
HRESULT
KSMidiInDevice::Initialize(
    LPCWSTR Device,
    HANDLE Filter,
    UINT PinId,
    BOOL Cyclic,
    BOOL UseUMP,
    ULONG BufferSize,
    DWORD* MmcssTaskId,
    IMidiCallback *Callback
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Callback);

    m_ThreadTerminateEvent.create();
    m_ThreadStartedEvent.create();

    m_MmcssTaskId = *MmcssTaskId;

    RETURN_IF_FAILED(KSMidiDevice::Initialize(Device, Filter, PinId, Cyclic, UseUMP, BufferSize));

    // grab the callback/lambda that was passed in, for use later for message callbacks.
    m_MidiInCallback = Callback;

    m_ThreadHandle.reset(CreateThread(nullptr, 0, MidiInWorker, this, 0, nullptr));
    RETURN_LAST_ERROR_IF_NULL(m_ThreadHandle);

    RETURN_HR_IF(E_FAIL, WaitForSingleObject(m_ThreadStartedEvent.get(), 30000) != WAIT_OBJECT_0);

    *MmcssTaskId = m_MmcssTaskId;

    return S_OK;
}

