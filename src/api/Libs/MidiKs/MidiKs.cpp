// Copyright (c) Microsoft Corporation. All rights reserved.
#include <windows.h>
#include <cguid.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include "MidiAbstraction_i.c"
#include "MidiAbstraction.h"

#include <Devpkey.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include "MidiKs.h"

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
    MidiTransport Transport,
    ULONG& BufferSize
)
{
    m_Transport = Transport;

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

    RETURN_IF_FAILED(OpenStream(BufferSize));
    RETURN_IF_FAILED(PinSetState(KSSTATE_ACQUIRE));
    RETURN_IF_FAILED(PinSetState(KSSTATE_PAUSE));
    RETURN_IF_FAILED(PinSetState(KSSTATE_RUN));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::OpenStream(ULONG& BufferSize
)
{
    RETURN_IF_FAILED(InstantiateMidiPin(m_Filter.get(), m_PinID, m_Transport, &m_Pin));

    BOOL looped = ((m_Transport == MidiTransport_CyclicByteStream) || (m_Transport == MidiTransport_CyclicUMP));

    if (looped)
    {
        m_MidiPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE);
        RETURN_IF_NULL_ALLOC(m_MidiPipe);

        m_CrossProcessMidiPump.reset(new (std::nothrow) CMidiXProc());
        RETURN_IF_NULL_ALLOC(m_CrossProcessMidiPump);

        m_MidiPipe->WriteEvent.create(wil::EventOptions::ManualReset);

        // if we're looped (cyclic buffer), we need to
        // configure the buffer, registers, and event.
        RETURN_IF_FAILED(ConfigureLoopedBuffer(BufferSize));
        RETURN_IF_FAILED(ConfigureLoopedRegisters());
        RETURN_IF_FAILED(ConfigureLoopedEvent());
    }
    else
    {
        // Buffer size isn't applicable for
        // legacy messages sent through ioctl
        BufferSize = 0;
    }

    return S_OK;
}

HRESULT
KSMidiDevice::Cleanup()
{
    // tear down all cross process work before
    // closing out pin and filter handles, which will
    // invalidate the memory addresses.
    if (m_CrossProcessMidiPump)
    {
        m_CrossProcessMidiPump->Cleanup();
        m_CrossProcessMidiPump.reset();
    }

    if (m_MidiPipe)
    {
        m_MidiPipe.reset();
    }

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
    DisableMmcss(m_MmcssHandle);

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
KSMidiDevice::ConfigureLoopedBuffer(ULONG& BufferSize
)
{
    KSMIDILOOPED_BUFFER_PROPERTY property {0};
    KSMIDILOOPED_BUFFER buffer{0};
    ULONG propertySize {sizeof(property)};

    property.Property.Set           = KSPROPSETID_MidiLoopedStreaming; 
    property.Property.Id            = KSPROPERTY_MIDILOOPEDSTREAMING_BUFFER;       
    property.Property.Flags         = KSPROPERTY_TYPE_GET;

    // Seems to be a reasonable balance for now,
    // TBD make this configurable via api or registry.
    property.RequestedBufferSize    = BufferSize;

    RETURN_IF_FAILED(SyncIoctl(
        m_Pin.get(),
        IOCTL_KS_PROPERTY,
        &property,
        propertySize,
        &buffer,
        sizeof(buffer),
        nullptr));

    m_MidiPipe->Data.BufferAddress = (PBYTE) buffer.BufferAddress;
    BufferSize = m_MidiPipe->Data.BufferSize = buffer.ActualBufferSize;

    return S_OK;
}

HRESULT
KSMidiDevice::ConfigureLoopedRegisters()
{
    KSPROPERTY property {0};
    KSMIDILOOPED_REGISTERS registers {0};
    ULONG propertySize {sizeof(property)};

    property.Set    = KSPROPSETID_MidiLoopedStreaming; 
    property.Id     = KSPROPERTY_MIDILOOPEDSTREAMING_REGISTERS;       
    property.Flags  = KSPROPERTY_TYPE_GET;

    RETURN_IF_FAILED(SyncIoctl(
        m_Pin.get(),
        IOCTL_KS_PROPERTY,
        &property,
        propertySize,
        &registers,
        sizeof(registers),
        nullptr));

    m_MidiPipe->Registers.ReadPosition = (PULONG) registers.ReadPosition;
    m_MidiPipe->Registers.WritePosition = (PULONG) registers.WritePosition;

    return S_OK;
}

HRESULT
KSMidiDevice::ConfigureLoopedEvent()
{
    KSPROPERTY property {0};
    ULONG propertySize {sizeof(property)};
    KSMIDILOOPED_EVENT LoopedEvent {0};

    LoopedEvent.WriteEvent = m_MidiPipe->WriteEvent.get();

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

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::Initialize(
    LPCWSTR Device,
    HANDLE Filter,
    UINT PinId,
    MidiTransport Transport,
    ULONG BufferSize,
    DWORD* MmcssTaskId
)
{
    RETURN_IF_FAILED(KSMidiDevice::Initialize(Device, Filter, PinId, Transport, BufferSize));

    m_MmcssTaskId = *MmcssTaskId;
    if (m_CrossProcessMidiPump)
    {
        std::unique_ptr<MEMORY_MAPPED_PIPE> emptyPipe;
        // we're sending midi messages here, so this is a midi out pipe, midi in pipe is unused
        RETURN_IF_FAILED(m_CrossProcessMidiPump->Initialize(MmcssTaskId, emptyPipe, m_MidiPipe, nullptr, 0));
    }
    *MmcssTaskId = m_MmcssTaskId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::SendMidiMessage(
    void * MidiData,
    UINT32 Length,
    LONGLONG Position
)
{
    // The length must be one of the valid UMP data lengths
    RETURN_HR_IF(E_INVALIDARG, Length == 0);

    if (m_CrossProcessMidiPump)
    {
        return m_CrossProcessMidiPump->SendMidiMessage(MidiData, Length, Position);
    }
    else
    {
        // using standard, write via standard streaming ioctls
        return WritePacketMidiData(MidiData, Length, Position);
    }
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
    m_Running = FALSE;

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
    while (m_Running)
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
                m_MidiInCallback->Callback(data, payloadSize, kssh.PresentationTime.Time, m_MidiInCallbackContext);
            }
        }
        else
        {
            return hr;
        }
    }
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
        if (SUCCEEDED(EnableMmcss(This->m_ThreadOwnedMmcssHandle, This->m_MmcssTaskId)))
        {
            // signal that our thread is started, and
            // mmcss is configured, so initialization can
            // retrieve the task id.
            This->m_ThreadStartedEvent.SetEvent();
            This->SendRequestToDriver();
            DisableMmcss(This->m_ThreadOwnedMmcssHandle);
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
    MidiTransport Transport,
    ULONG BufferSize,
    DWORD* MmcssTaskId,
    IMidiCallback *Callback,
    LONGLONG Context
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == Callback);

    RETURN_IF_FAILED(KSMidiDevice::Initialize(Device, Filter, PinId, Transport, BufferSize));

    if (m_CrossProcessMidiPump)
    {
        std::unique_ptr<MEMORY_MAPPED_PIPE> emptyPipe;
        // we're getting midi messages here, so this is a midi in pipe, midi out pipe is unused
        RETURN_IF_FAILED(m_CrossProcessMidiPump->Initialize(MmcssTaskId, m_MidiPipe, emptyPipe, Callback, Context));
    }
    else
    {
        m_MmcssTaskId = *MmcssTaskId;

        m_ThreadTerminateEvent.create();
        m_ThreadStartedEvent.create();

        // grab the callback/lambda that was passed in, for use later for message callbacks.
        m_MidiInCallback = Callback;
        m_MidiInCallbackContext = Context;

        m_ThreadHandle.reset(CreateThread(nullptr, 0, MidiInWorker, this, 0, nullptr));
        RETURN_LAST_ERROR_IF_NULL(m_ThreadHandle);

        RETURN_HR_IF(E_FAIL, WaitForSingleObject(m_ThreadStartedEvent.get(), 30000) != WAIT_OBJECT_0);

        *MmcssTaskId = m_MmcssTaskId;
    }

    return S_OK;
}

