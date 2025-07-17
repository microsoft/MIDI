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

#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include <Devpkey.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include "MidiKs.h"

KSMidiDevice::~KSMidiDevice()
{
    Shutdown();
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::Initialize(
    LPCWSTR device,
    HANDLE filter,
    UINT pinId,
    MidiTransport transport,
    ULONG& bufferSize
)
{
    m_Transport = transport;

    m_FilterFilename = wil::make_cotaskmem_string_nothrow(device);
    RETURN_IF_NULL_ALLOC(m_FilterFilename);

    if (!filter)
    {
        // Wrapper opens the handle internally.
        m_FilterHandleWrapper = std::make_unique<KsHandleWrapper>(m_FilterFilename.get());
        RETURN_IF_FAILED(m_FilterHandleWrapper->Open());
    }
    else
    {
        // Duplicate the incoming handle and assign it to the wrapper directly.
        HANDLE dupHandle = nullptr;
        RETURN_IF_WIN32_BOOL_FALSE(
            DuplicateHandle(GetCurrentProcess(), filter, GetCurrentProcess(), &dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS));

        m_FilterHandleWrapper = std::make_unique<KsHandleWrapper>(m_FilterFilename.get());
        m_FilterHandleWrapper->SetHandle(wil::unique_handle(dupHandle));
        RETURN_IF_FAILED(m_FilterHandleWrapper->RegisterForNotifications());
    }

    m_PinID = pinId;

    RETURN_IF_FAILED(OpenStream(bufferSize));
    RETURN_IF_FAILED(PinSetState(KSSTATE_ACQUIRE));
    RETURN_IF_FAILED(PinSetState(KSSTATE_PAUSE));
    RETURN_IF_FAILED(PinSetState(KSSTATE_RUN));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::OpenStream(ULONG& bufferSize
)
{
    // Duplicate the handle to safely pass it to another component or store it.
    wil::unique_handle handleDupe(m_FilterHandleWrapper->GetHandle());
    RETURN_IF_NULL_ALLOC(handleDupe);

    m_PinHandleWrapper = std::make_unique<KsHandleWrapper>(m_FilterFilename.get(), m_PinID, m_Transport, handleDupe.get());
    RETURN_IF_FAILED(m_PinHandleWrapper->Open());

    BOOL looped = ((m_Transport == MidiTransport_CyclicByteStream) || (m_Transport == MidiTransport_CyclicUMP));

    if (looped)
    {
        m_MidiPipe.reset(new (std::nothrow) MEMORY_MAPPED_PIPE);
        RETURN_IF_NULL_ALLOC(m_MidiPipe);

        m_MidiPipe->DataFormat = (m_Transport == MidiTransport_CyclicByteStream)?MidiDataFormats_ByteStream:MidiDataFormats_UMP;
        m_CrossProcessMidiPump.reset(new (std::nothrow) CMidiXProc());
        RETURN_IF_NULL_ALLOC(m_CrossProcessMidiPump);

        m_MidiPipe->WriteEvent.create(wil::EventOptions::ManualReset);
        m_MidiPipe->ReadEvent.create(wil::EventOptions::ManualReset);

        // if we're looped (cyclic buffer), we need to
        // configure the buffer, registers, and event.
        RETURN_IF_FAILED(ConfigureLoopedBuffer(bufferSize));
        RETURN_IF_FAILED(ConfigureLoopedRegisters());
        RETURN_IF_FAILED(ConfigureLoopedEvent());
    }
    else
    {
        // Buffer size isn't applicable for
        // legacy messages sent through ioctl
        bufferSize = 0;
    }

    return S_OK;
}

HRESULT
KSMidiDevice::Shutdown()
{
    // tear down all cross process work before
    // closing out pin and filter handles, which will
    // invalidate the memory addresses.
    if (m_CrossProcessMidiPump)
    {
        m_CrossProcessMidiPump->Shutdown();
        m_CrossProcessMidiPump.reset();
    }

    if (m_MidiPipe)
    {
        m_MidiPipe.reset();
    }

    if (m_PinHandleWrapper && m_PinHandleWrapper->IsOpen())
    {
        RETURN_IF_FAILED(PinSetState(KSSTATE_PAUSE));
        RETURN_IF_FAILED(PinSetState(KSSTATE_ACQUIRE));
        RETURN_IF_FAILED(PinSetState(KSSTATE_STOP));
    }

    m_PinHandleWrapper.reset();
    m_FilterHandleWrapper.reset();
    
    // if a worker has configured mmcss and hasn't yet cleaned
    // it up, this is our last chance
    DisableMmcss(m_MmcssHandle);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::PinSetState(
    KSSTATE pinState
)
{
    KSPROPERTY property {0};
    ULONG propertySize {sizeof(property)};

    property.Set    = KSPROPSETID_Connection; 
    property.Id     = KSPROPERTY_CONNECTION_STATE;       
    property.Flags  = KSPROPERTY_TYPE_SET;

    if (!m_PinHandleWrapper)
    {
        return E_HANDLE;
    }

    // Using lamba function to prevent handle from dissapearing when being used. 
    RETURN_IF_FAILED(m_PinHandleWrapper->Execute([&](HANDLE h) {
        return SyncIoctl(
            h,
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &pinState,
            sizeof(KSSTATE),
            nullptr);
    }));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiDevice::ConfigureLoopedBuffer(ULONG& bufferSize
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
    property.RequestedBufferSize    = bufferSize;

    if (!m_PinHandleWrapper)
    {
        return E_HANDLE;
    }

    RETURN_IF_FAILED(m_PinHandleWrapper->Execute([&](HANDLE h) {
        return SyncIoctl(
            h,
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &buffer,
            sizeof(buffer),
            nullptr);
}));

    m_MidiPipe->Data.BufferAddress = (PBYTE) buffer.BufferAddress;
    bufferSize = m_MidiPipe->Data.BufferSize = buffer.ActualBufferSize;

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

    if (!m_PinHandleWrapper)
    {
        return E_HANDLE;
    }

    RETURN_IF_FAILED(m_PinHandleWrapper->Execute([&](HANDLE h) {
        return SyncIoctl(
            h,
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &registers,
            sizeof(registers),
            nullptr);
}));

    m_MidiPipe->Registers.ReadPosition = (PULONG) registers.ReadPosition;
    m_MidiPipe->Registers.WritePosition = (PULONG) registers.WritePosition;

    return S_OK;
}

HRESULT
KSMidiDevice::ConfigureLoopedEvent()
{
    KSPROPERTY property {0};
    ULONG propertySize {sizeof(property)};
    KSMIDILOOPED_EVENT2 LoopedEvent {0};

    LoopedEvent.WriteEvent = m_MidiPipe->WriteEvent.get();
    LoopedEvent.ReadEvent = m_MidiPipe->ReadEvent.get();

    property.Set    = KSPROPSETID_MidiLoopedStreaming; 
    property.Id     = KSPROPERTY_MIDILOOPEDSTREAMING_NOTIFICATION_EVENT;       
    property.Flags  = KSPROPERTY_TYPE_SET;

    if (!m_PinHandleWrapper)
    {
        return E_HANDLE;
    }

    RETURN_IF_FAILED(m_PinHandleWrapper->Execute([&](HANDLE h) {
        return SyncIoctl(
            h,
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &LoopedEvent,
            sizeof(LoopedEvent),
            nullptr);
}));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::Initialize(
    LPCWSTR device,
    HANDLE filter,
    UINT pinId,
    MidiTransport transport,
    ULONG bufferSize,
    DWORD* mmcssTaskId
)
{
    RETURN_IF_FAILED(KSMidiDevice::Initialize(device, filter, pinId, transport, bufferSize));

    m_MmcssTaskId = *mmcssTaskId;
    if (m_CrossProcessMidiPump)
    {
        std::unique_ptr<MEMORY_MAPPED_PIPE> emptyPipe;
        // we're sending midi messages here, so this is a midi out pipe, midi in pipe is unused
        RETURN_IF_FAILED(m_CrossProcessMidiPump->Initialize(mmcssTaskId, emptyPipe, m_MidiPipe, nullptr, 0, true));
    }
    *mmcssTaskId = m_MmcssTaskId;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::SendMidiMessage(
    void * midiData,
    UINT32 length,
    LONGLONG position
)
{
    // The length must be one of the valid UMP data lengths
    RETURN_HR_IF(E_INVALIDARG, length == 0);

    if (m_CrossProcessMidiPump)
    {
        auto hr = m_CrossProcessMidiPump->SendMidiMessage(midiData, length, position);
        LOG_IF_FAILED(hr);
        return hr;
    }
    else
    {
        // using standard, write via standard streaming ioctls
        auto hr = WritePacketMidiData(midiData, length, position);
        LOG_IF_FAILED(hr);
        return hr;
    }
}

_Use_decl_annotations_
HRESULT
KSMidiOutDevice::WritePacketMidiData(
    void* midiData,
    UINT32 length,
    LONGLONG position
)
{
    KSSTREAM_HEADER kssh {0};
    KSMUSICFORMAT *event = nullptr;
    UINT32 totalLength = 0;

    // total size of the payload is the leading structure + message size
    totalLength = DWORD_ALIGN(sizeof(KSMUSICFORMAT) + length);

    // allocate and zero an event, so we can send everything as a single message
    event = reinterpret_cast<KSMUSICFORMAT*>(new BYTE[totalLength]{0});
    RETURN_HR_IF(E_OUTOFMEMORY, nullptr == event);

    auto cleanupOnExit = wil::scope_exit([&]()
    {
        delete[] event;
    });

    // The byte count here is the size of the actual midi message, without
    // leading KSMUSICFORMAT structure size or padding
    event->ByteCount = length;

    // now we can copy over the message data into the KSMUSICFORMAT
    CopyMemory(event + 1, midiData, length);

    kssh.Size = sizeof(KSSTREAM_HEADER);

    // mirroring win32 midi, presentation time on outgoing messages
    // is always 1.
    kssh.PresentationTime.Numerator = 1;
    kssh.PresentationTime.Denominator = 1;

    // for legacy midi 1, if the position provided was 0, use 0
    kssh.PresentationTime.Time = position;

    // the length here is the allocated size of event, so 
    // header structure + data + padding
    kssh.DataUsed = kssh.FrameExtent = totalLength;

    kssh.Data = reinterpret_cast<BYTE*>(event);

    if (!m_PinHandleWrapper)
    {
        return E_HANDLE;
    }

    HRESULT hr = m_PinHandleWrapper->Execute([&](HANDLE h) {
        return SyncIoctl(
            h,
            IOCTL_KS_WRITE_STREAM,
            nullptr,
            0,
            (UCHAR*)&kssh,
            kssh.Size,
            nullptr);
    });

    RETURN_IF_FAILED(hr);

    return S_OK;
}

HRESULT
KSMidiInDevice::Shutdown()
{
    m_Running = FALSE;

    // safe to clean up the base class now that any looped
    // worker threads are cleaned up.
    HRESULT hr = KSMidiDevice::Shutdown();

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

        if (!m_PinHandleWrapper)
        {
            return E_HANDLE;
        }

        HRESULT hr = m_PinHandleWrapper->Execute([&](HANDLE h) {
            return SyncIoctl(
                h,
                IOCTL_KS_READ_STREAM,
                nullptr,
                0,
                (UCHAR*)&kssh,
                kssh.Size,
                nullptr);
        });

        if (SUCCEEDED(hr))
        {
            UINT32 payloadSize = event.ksMusicFormat.ByteCount;
            PVOID data = reinterpret_cast<BYTE*>(&event.ksMusicFormat + 1);

            // if payload size is 0, no need to call back, when the handle
            // is closed and the SyncIoctl returns, it may succeed, but have no data.
            if (m_MidiInCallback && payloadSize > 0)
            {
                LOG_IF_FAILED(m_MidiInCallback->Callback(data, payloadSize, kssh.PresentationTime.Time, m_MidiInCallbackContext));
            }
        }
        else
        {
            //return hr;
            LOG_IF_FAILED(hr);
        }
    }
    return S_OK;
}

_Use_decl_annotations_
DWORD WINAPI
KSMidiInDevice::MidiInWorker(
    LPVOID param
)
{
    auto coinit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    KSMidiInDevice *This = reinterpret_cast<KSMidiInDevice*>(param);
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
    LPCWSTR device,
    HANDLE filter,
    UINT pinId,
    MidiTransport transport,
    ULONG bufferSize,
    DWORD* mmcssTaskId,
    IMidiCallback *callback,
    LONGLONG context
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == callback);

    RETURN_IF_FAILED(KSMidiDevice::Initialize(device, filter, pinId, transport, bufferSize));

    if (m_CrossProcessMidiPump)
    {
        std::unique_ptr<MEMORY_MAPPED_PIPE> emptyPipe;
        // we're getting midi messages here, so this is a midi in pipe, midi out pipe is unused
        RETURN_IF_FAILED(m_CrossProcessMidiPump->Initialize(mmcssTaskId, m_MidiPipe, emptyPipe, callback, context, true));
    }
    else
    {
        m_MmcssTaskId = *mmcssTaskId;

        m_ThreadTerminateEvent.create();
        m_ThreadStartedEvent.create();

        // grab the callback/lambda that was passed in, for use later for message callbacks.
        m_MidiInCallback = callback;
        m_MidiInCallbackContext = context;

        m_ThreadHandle.reset(CreateThread(nullptr, 0, MidiInWorker, this, 0, nullptr));
        RETURN_LAST_ERROR_IF_NULL(m_ThreadHandle);

        RETURN_HR_IF(E_FAIL, WaitForSingleObject(m_ThreadStartedEvent.get(), 30000) != WAIT_OBJECT_0);

        *mmcssTaskId = m_MmcssTaskId;
    }

    return S_OK;
}

