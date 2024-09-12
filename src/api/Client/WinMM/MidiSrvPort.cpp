// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "MidiSrvPort.h"

CMidiPort::CMidiPort()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));
}

CMidiPort::~CMidiPort()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(g_ProcessIsTerminating, "ProcessIsTerminating"));

    if (!g_ProcessIsTerminating)
    {
        Cleanup();
    }
    else
    {
        // unsafe to release COM objects while process is terminating.
        static_cast<void>(m_MidiAbstraction.detach());
        static_cast<void>(m_MidiIn.detach());
        static_cast<void>(m_MidiOut.detach());
    }
}

_Use_decl_annotations_
HRESULT
CMidiPort::RuntimeClassInitialize(GUID SessionId, std::wstring& InterfaceId, MidiFlow Flow, MIDIOPENDESC* OpenDesc, DWORD_PTR Flags)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(SessionId, "SessionId"),
        TraceLoggingWideString(InterfaceId.c_str(), "InterfaceId"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(Flags, "Flags"));

    ABSTRACTIONCREATIONPARAMS abstractionCreationParams { MidiDataFormat_ByteStream };
    DWORD mmcssTaskId {0};
    LARGE_INTEGER qpc{ 0 };
    
    QueryPerformanceFrequency(&qpc);
    m_qpcFrequency = qpc.QuadPart;

    m_Flow = Flow,
    memcpy(&m_OpenDesc, OpenDesc, sizeof(m_OpenDesc));
    m_Flags = Flags;
    m_InterfaceId = InterfaceId;

    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_MidiAbstraction)));

    if (Flow == MidiFlowIn)
    {
        RETURN_IF_FAILED(m_MidiAbstraction->Activate(__uuidof(IMidiIn), (void **) &m_MidiIn));
        RETURN_IF_FAILED(m_MidiIn->Initialize(m_InterfaceId.c_str(), &abstractionCreationParams, &mmcssTaskId, this, 0, SessionId));
    }
    else
    {
        RETURN_IF_FAILED(m_MidiAbstraction->Activate(__uuidof(IMidiOut), (void **) &m_MidiOut));
        RETURN_IF_FAILED(m_MidiOut->Initialize(m_InterfaceId.c_str(), &abstractionCreationParams, &mmcssTaskId, SessionId));
    }

    WinmmClientCallback(m_Flow == MidiFlowIn?MIM_OPEN:MOM_OPEN, 0, 0);
    return S_OK;
}

HRESULT
CMidiPort::Cleanup()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));

    auto remove = wil::scope_exit([&]()
    {
        // Scope exit cleanup of everything else.
        // Now that MidiIn/Out have been cleaned up (or failed),
        // it's safe to clean up everything else, while holding the
        // lock on the off chance that some other call happens to
        // come in from the client while we're cleaning up.
        auto lock = m_Lock.lock();
        std::queue<LPMIDIHDR> emptyQueue;

        m_MidiIn.reset();
        m_MidiOut.reset();
        m_MidiAbstraction.reset();
        std::swap(m_InBuffers, emptyQueue);
        m_InterfaceId.clear();

        // OpenDesc and flags are used for the Winmm client callback,
        // safe to clear now that callback is completed.
        memset(&m_OpenDesc, 0, sizeof(m_OpenDesc));
        m_Flags = 0;
    });

    Stop();

    // MidiIn/Out cleanup must be done without holding
    // the lock, because they will block for worker thread cleanup
    if (m_MidiIn)
    {
        RETURN_IF_FAILED(m_MidiIn->Cleanup());
    }
    else if (m_MidiOut)
    {
        RETURN_IF_FAILED(m_MidiOut->Cleanup());
    }

    // successful cleanup, send MIM/MOM_CLOSE to the client
    WinmmClientCallback(m_Flow == MidiFlowIn?MIM_CLOSE:MOM_CLOSE, 0, 0);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::MidMessage(UINT Msg, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    switch(Msg)
    {
        case MIDM_ADDBUFFER:
            RETURN_IF_FAILED(AddBuffer((LPMIDIHDR) Param1, Param2));
            break;
        case MIDM_START:
            RETURN_IF_FAILED(Start());
            break;
        case MIDM_STOP:
            RETURN_IF_FAILED(Stop());
            break;
        case MIDM_RESET:
            RETURN_IF_FAILED(Reset());
            break;
        case MIDM_CLOSE:
            RETURN_IF_FAILED(Close());
            break;
        default:
            RETURN_IF_FAILED(HRESULT_FROM_MMRESULT(MMSYSERR_NOTSUPPORTED));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::ModMessage(UINT Msg, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    switch(Msg)
    {
        case MODM_LONGDATA:
            RETURN_IF_FAILED(SendLongMessage(reinterpret_cast<MIDIHDR*>(Param1)));
            break;
        case MODM_DATA:
            RETURN_IF_FAILED(SendMidiMessage(static_cast<UINT32>(Param1)));
            break;
        case MODM_RESET:
            RETURN_IF_FAILED(Reset());
            break;
        case MODM_CLOSE:
            RETURN_IF_FAILED(Close());
            break;
        default:
            RETURN_IF_FAILED(HRESULT_FROM_MMRESULT(MMSYSERR_NOTSUPPORTED));
    }

    return S_OK;
}

_Use_decl_annotations_
bool 
CMidiPort::IsFlow(MidiFlow Flow)
{
    auto lock = m_Lock.lock();
    return (m_Flow == Flow);
}


HRESULT
CMidiPort::Reset()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));

    if (m_Flow == MidiFlowIn)
    {
        {
            auto lock = m_BuffersLock.lock();
            while(!m_InBuffers.empty())
            {
                LPMIDIHDR buffer = m_InBuffers.front();
                m_InBuffers.pop();
                buffer->dwFlags &= ~MHDR_INQUEUE;
                buffer->dwFlags |= MHDR_DONE;
            }
        }

        RETURN_IF_FAILED(Stop());
    }

    return S_OK;
}

HRESULT
CMidiPort::Close()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));

    if (m_Flow == MidiFlowIn)
    {
        auto lock = m_BuffersLock.lock();
        if (!m_InBuffers.empty())
        {
            RETURN_IF_FAILED(HRESULT_FROM_MMRESULT(MIDIERR_STILLPLAYING));
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::AddBuffer(LPMIDIHDR Buffer, DWORD_PTR BufferSize)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(Buffer, "Buffer"),
        TraceLoggingValue(BufferSize, "BufferSize"));

    RETURN_HR_IF(E_INVALIDARG, BufferSize < sizeof(MIDIHDR));

    Buffer->dwFlags &= (~MHDR_DONE);
    Buffer->dwFlags |= MHDR_INQUEUE;
    Buffer->dwBytesRecorded = 0;

    auto lock = m_BuffersLock.lock();
    m_InBuffers.push(Buffer);
    m_BuffersAdded.SetEvent();

    return S_OK;
}

HRESULT
CMidiPort::Start()
{
   TraceLoggingWrite(
       WdmAud2TelemetryProvider::Provider(),
       MIDI_TRACE_EVENT_INFO,
       TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
       TraceLoggingLevel(WINEVENT_LEVEL_INFO),
       TraceLoggingPointer(this, "this"));

   LARGE_INTEGER qpc{ 0 };
   QueryPerformanceCounter(&qpc);

   auto lock = m_Lock.lock();
   m_Started = true;
   m_StartTime = qpc.QuadPart;

   return S_OK;
}

HRESULT
CMidiPort::Stop()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));

    bool inCallback {false};

    {
        // set the state to stopped
        auto lock = m_Lock.lock();
        m_Started = false;
        m_StartTime = 0;
        m_Stopped.SetEvent();
        inCallback = m_InCallback;
    }

    if (inCallback)
    {
        // if we were in the callback when the state changed,
        // wait for the callback to terminate. It won't enter again
        // because the state is stopped.
        m_ExitCallback.wait();
    }

    {
        // we're out of the callback, accessing m_IsInSysex is now
        // safe, complete any outstanding long sysex messages with error.
        auto lock = m_Lock.lock();
        LARGE_INTEGER qpc{ 0 };
        QueryPerformanceCounter(&qpc);
        if (m_IsInSysex)
        {
            RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGERROR, qpc.QuadPart));
            m_IsInSysex = false;
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::CompleteLongBuffer(UINT Message, LONGLONG Position)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(Message, "Message"),
        TraceLoggingValue(Position, "Position"));

    LPMIDIHDR buffer {nullptr};

    // take the buffer that's being completed off the top of the queue
    {
        auto lock = m_BuffersLock.lock();
        buffer = m_InBuffers.front();
        m_InBuffers.pop();
    }

    // calculate the timestamp for the message
    DWORD ticks = (DWORD) (((Position - m_StartTime) / m_qpcFrequency) * 1000.0);

    // mark it completed
    buffer->dwFlags &= (~MHDR_INQUEUE);
    buffer->dwFlags |= MHDR_DONE;

    if((Message == MIM_LONGERROR) && (buffer->dwBytesRecorded == 0))
    {
        // if no bytes recorded, its not really an error
        Message = MIM_LONGDATA;
    }

    WinmmClientCallback(Message, reinterpret_cast<DWORD_PTR>(buffer), ticks);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::Callback(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position, LONGLONG Context)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"Start", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(Data, "Data"),
        TraceLoggingValue(Size, "Size"),
        TraceLoggingValue(Position, "Position"),
        TraceLoggingValue(Context, "Context"));

    bool started {FALSE};
    LONGLONG startTime {0};

    {
        auto lock = m_Lock.lock();
        started = m_Started;
        startTime = m_StartTime;
        m_InCallback = true;
        m_ExitCallback.ResetEvent();
    }

    auto exitCallback = wil::scope_exit([&]()
    {
        TraceLoggingWrite(
            WdmAud2TelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"End", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingPointer(Data, "Data"),
            TraceLoggingValue(Size, "Size"),
            TraceLoggingValue(Position, "Position"),
            TraceLoggingValue(Context, "Context"));

        auto lock = m_Lock.lock();
        m_ExitCallback.SetEvent();
        m_InCallback = false;
    });

    // if we're running, and this message was generated after we started,
    // deliver it to the client.
    if (started && Position >= startTime)
    {
        // The amount of data left for this callback, and the current data position
        DWORD callbackDataRemaining {Size};
        BYTE *callbackData = (BYTE *)Data;

        // keep processing until we run out of callback data
        while (callbackDataRemaining > 0 && started)
        {
            // If this is a sysex message, then go into InSysex
            // state
            if (MIDI_SYSEX == *callbackData)
            {
                m_IsInSysex = true;
            }

            if (m_IsInSysex)
            {
                // if we're processing a sysex message, then we need a buffer to deliver
                // the data in, retrieve it, getting either a previously incomplete buffer
                // or a new one.
                LPMIDIHDR buffer {nullptr};

                {
                    auto lock = m_BuffersLock.lock();
                    if (!m_InBuffers.empty())
                    {
                        buffer = m_InBuffers.front();
                    }
                }

                // we were able to successfully retrieve a buffer, now fill it.
                if (buffer)
                {
                    // our data position within the buffer is the start of the buffer, indexed in
                    // by how much has already been written.
                    BYTE *bufferData = (((BYTE *)buffer->lpData) + buffer->dwBytesRecorded);

                    // copy the data, inspecting them as we go looking for messages which would
                    // cause us to complete the sysex message in progress.
                    // We're done when we either run out of data to copy, run out of space to put it
                    // in this buffer, or hit something that causes this sysex to end or be terminated.
                    while(callbackDataRemaining > 0 &&
                        (buffer->dwBytesRecorded < buffer->dwBufferLength))
                    {
                        if(0 != (*callbackData & MIDI_STATUSBYTEFILTER))
                        {
                            // this is a status byte message, we're going to need some additional checks.

                            // we're in a sysex block and somehow received a normal midi message,
                            // that's an error, complete the sysex and exit out of processing sysex.
                            if(MIDI_EOX != *callbackData && 
                               (*callbackData & MIDI_STATUSBYTEFILTER) < MIDI_SYSEX)
                            {
                                RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGERROR, Position));
                                m_IsInSysex = false;
                                break;
                            }
                            else if(MIDI_SYSTEM_REALTIME_FILTER != (*callbackData & MIDI_SYSTEM_REALTIME_FILTER))
                            {
                                // a sys-ex block is supposed to end with a MIDI_EOX, however
                                // any valid MIDI status byte CAN end a sys-ex block EXCEPT
                                // for system real-time messages (0xF8 to 0xFF)

                                // we have a valid end of sys-ex byte, so turn off
                                // m_bIsInSysEx
                                RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, Position));
                                m_IsInSysex = false;
                                break;
                            }
                        }

                        // it's not a status byte, so copy it to the buffer and advance our positions.
                        *callbackData = *bufferData;

                        ++callbackData;
                        ++bufferData;

                        ++buffer->dwBytesRecorded;
                        --callbackDataRemaining;
                    }

                    // if we are still in sysex and completed this buffer, we can deliver it, the
                    // next iteration will require a new buffer.
                    if (buffer->dwBufferLength == buffer->dwBytesRecorded)
                    {

                        RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, Position));
                    }
                }
                // we have remaining callback data, but no buffer available, wait for either a buffer
                // or the port to be stopped.
                else
                {
                    // Wait for either a buffer to be added, or the pin to be stopped,
                    // it doesn't matter which wakes us.
                    HANDLE handles[] = { m_BuffersAdded.get(), m_Stopped.get() };
                    WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
                }
            }
            else
            {
                // not in a sysex, process this as a normal message

                BYTE status;
                BYTE data1;
                BYTE data2;

                // if this is true, running status is active
                if(0 == (*callbackData & MIDI_STATUSBYTEFILTER))
                {
                    // running status required, but not previously set,
                    // this isn't a valid message
                    RETURN_HR_IF(E_FAIL, 0 == m_RunningStatus);
                    status = m_RunningStatus;
                    data1 = callbackData[0];
                    data2 = callbackData[1];
                }
                else
                {
                    status = callbackData[0];
                    if(status < MIDI_SYSEX)  // don't save system common or system realtime as running status
                    {
                        m_RunningStatus = status;
                    }
                    data1 = callbackData[1];
                    data2 = callbackData[2];
                }

                // Convert from the QPC time to the number of ticks elapsed from the start time.
                DWORD ticks = (DWORD) (((Position - startTime) / m_qpcFrequency) * 1000.0);
                UINT32 midiMessage = status | (data1 << 8) | (data2 << 16);
                WinmmClientCallback(MIM_DATA, midiMessage, ticks);
                callbackData+=3;
                callbackDataRemaining-=3;
            }

            // before we continue on to the next buffer, we need to find out if the
            // port is still running.
            if (callbackDataRemaining > 0)
            {
                auto lock = m_Lock.lock();
                started = m_Started;
            }
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::SendMidiMessage(UINT32 MidiMessage)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"Start", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(this, "this"));

    auto exitCallback = wil::scope_exit([&]()
    {
        TraceLoggingWrite(
            WdmAud2TelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"End", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingPointer(this, "this"));
    });

    {
        auto lock = m_Lock.lock();

        // This should be on an initialized midi out port
        RETURN_HR_IF(E_INVALIDARG, nullptr == m_MidiOut);

        // send the message to the abstraction
        // pass a timestamp of 0 to bypass scheduler
        RETURN_IF_FAILED(m_MidiOut->SendMidiMessage(&MidiMessage, sizeof(MidiMessage), 0));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::SendLongMessage(LPMIDIHDR Buffer)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"Start", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(Buffer, "Buffer"));

    auto exitCallback = wil::scope_exit([&]()
    {
        TraceLoggingWrite(
            WdmAud2TelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"End", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingPointer(Buffer, "Buffer"));
    });

    {
        auto lock = m_Lock.lock();

        // This should be on an initialized midi out port
        RETURN_HR_IF(E_INVALIDARG, nullptr == m_MidiOut);

        // The buffer provided must be valid
        RETURN_HR_IF(E_INVALIDARG, nullptr == Buffer);
        RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_INVALFLAG), !(Buffer->dwFlags & MHDR_PREPARED));

        // send the message to the abstraction
        // pass a timestamp of 0 to bypass scheduler
        // TODO: based on the buffer length, this message may require chunking into smaller
        // pieces to ensure it fits into the cross process queue.
        RETURN_IF_FAILED(m_MidiOut->SendMidiMessage(Buffer->lpData, Buffer->dwBufferLength, 0));
    }

    // mark the buffer as completed
    Buffer->dwFlags |= MHDR_DONE;

    // client callback indicating this buffer is completed.
    WinmmClientCallback(MOM_DONE, (DWORD_PTR) Buffer, 0);
    return S_OK;
}

_Use_decl_annotations_
void
CMidiPort::WinmmClientCallback(UINT Msg, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(Msg, "msg"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    DriverCallback(m_OpenDesc.dwCallback,
                    HIWORD(m_Flags),
                    (HDRVR) m_OpenDesc.hMidi,
                    Msg,
                    m_OpenDesc.dwInstance,
                    Param1,
                    Param2);
}