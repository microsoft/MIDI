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

    Shutdown();
}

_Use_decl_annotations_
HRESULT
CMidiPort::RuntimeClassInitialize(GUID sessionId, std::wstring& interfaceId, MidiFlow flow, const MIDIOPENDESC* openDesc, DWORD_PTR flags)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingGuid(sessionId, "sessionId"),
        TraceLoggingWideString(interfaceId.c_str(), "interfaceId"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(flags, "flags"));

    TRANSPORTCREATIONPARAMS transportCreationParams { MidiDataFormats_ByteStream, WINMM_APIID };
    DWORD mmcssTaskId {0};
    LARGE_INTEGER qpc{ 0 };
    
    QueryPerformanceFrequency(&qpc);
    m_qpcFrequency = qpc.QuadPart;

    m_Flow = flow,
    memcpy(&m_OpenDesc, openDesc, sizeof(m_OpenDesc));
    m_Flags = flags;
    m_InterfaceId = interfaceId;

    std::unique_ptr<CMidi2MidiSrv> midiSrv(new (std::nothrow) CMidi2MidiSrv());
    RETURN_IF_NULL_ALLOC(midiSrv);

    RETURN_IF_FAILED(midiSrv->Initialize(m_InterfaceId.c_str(), flow, &transportCreationParams, &mmcssTaskId, this, 0, sessionId));
    m_MidisrvTransport = std::move(midiSrv);

    WinmmClientCallback(m_Flow == MidiFlowIn?MIM_OPEN:MOM_OPEN, 0, 0);
    return S_OK;
}

HRESULT
CMidiPort::Shutdown()
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

        m_MidisrvTransport.reset();

        std::swap(m_InBuffers, emptyQueue);
        m_InterfaceId.clear();

        // openDesc and flags are used for the Winmm client callback,
        // safe to clear now that callback is completed.
        memset(&m_OpenDesc, 0, sizeof(m_OpenDesc));
        m_Flags = 0;
    });

    Stop();

    if (m_MidisrvTransport)
    {
        m_MidisrvTransport->Shutdown();
    }

    // successful cleanup, send MIM/MOM_CLOSE to the client
    WinmmClientCallback(m_Flow == MidiFlowIn?MIM_CLOSE:MOM_CLOSE, 0, 0);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::MidMessage(UINT msg, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    switch(msg)
    {
        case MIDM_ADDBUFFER:
            RETURN_IF_FAILED(AddBuffer((LPMIDIHDR) param1, param2));
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
CMidiPort::ModMessage(UINT msg, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    switch(msg)
    {
        case MODM_LONGDATA:
            RETURN_IF_FAILED(SendLongMessage(reinterpret_cast<MIDIHDR*>(param1)));
            break;
        case MODM_DATA:
            RETURN_IF_FAILED(SendMidiMessage(static_cast<UINT32>(param1)));
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
CMidiPort::IsFlow(MidiFlow flow)
{
    auto lock = m_Lock.lock();
    return (m_Flow == flow);
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
CMidiPort::AddBuffer(LPMIDIHDR buffer, DWORD_PTR bufferSize)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(buffer, "buffer"),
        TraceLoggingValue(bufferSize, "bufferSize"));

    RETURN_HR_IF(E_INVALIDARG, bufferSize < sizeof(MIDIHDR));

    buffer->dwFlags &= (~MHDR_DONE);
    buffer->dwFlags |= MHDR_INQUEUE;
    buffer->dwBytesRecorded = 0;

    auto lock = m_BuffersLock.lock();
    m_InBuffers.push(buffer);
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
CMidiPort::CompleteLongBuffer(UINT message, LONGLONG position)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(message, "message"),
        TraceLoggingValue(position, "position"));

    LPMIDIHDR buffer {nullptr};

    // take the buffer that's being completed off the top of the queue
    {
        auto lock = m_BuffersLock.lock();
        buffer = m_InBuffers.front();
        m_InBuffers.pop();
    }

    // calculate the timestamp for the message
    DWORD ticks = (DWORD) (((position - m_StartTime) / m_qpcFrequency) * 1000.0);

    // mark it completed
    buffer->dwFlags &= (~MHDR_INQUEUE);
    buffer->dwFlags |= MHDR_DONE;

    if((message == MIM_LONGERROR) && (buffer->dwBytesRecorded == 0))
    {
        // if no bytes recorded, its not really an error
        message = MIM_LONGDATA;
    }

    WinmmClientCallback(message, reinterpret_cast<DWORD_PTR>(buffer), ticks);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::Callback(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG position, LONGLONG context)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"Start", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(data, "data"),
        TraceLoggingValue(size, "size"),
        TraceLoggingValue(position, "position"),
        TraceLoggingValue(context, "context"));

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
            TraceLoggingPointer(data, "data"),
            TraceLoggingValue(size, "size"),
            TraceLoggingValue(position, "position"),
            TraceLoggingValue(context, "context"));

        auto lock = m_Lock.lock();
        m_ExitCallback.SetEvent();
        m_InCallback = false;
    });

    // if we're running, and this message was generated after we started,
    // deliver it to the client.
    if (started && position >= startTime)
    {
        // The amount of data left for this callback, and the current data position
        DWORD callbackDataRemaining {size};
        BYTE *callbackData = (BYTE *)data;

        bool processingSysExStartByte = false;

        // keep processing until we run out of callback data
        while (callbackDataRemaining > 0 && started)
        {
            // If this is a sysex message, then go into InSysex
            // state
            if (MIDI_SYSEX == *callbackData)
            {
                m_IsInSysex = true;
                m_RunningStatus = 0;
                processingSysExStartByte = true;
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
                    while (callbackDataRemaining > 0 &&
                        (buffer->dwBytesRecorded < buffer->dwBufferLength))
                    {
                        if (!processingSysExStartByte && 0 != (*callbackData & MIDI_STATUSBYTEFILTER))
                        {
                            // this is a status byte message, we're going to need some additional checks.

                            // we're in a sysex block and somehow received a normal midi message,
                            // that's an error, complete the sysex and exit out of processing sysex.
                            if (*callbackData < MIDI_SYSEX)
                            {
                                RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGERROR, position));
                                m_IsInSysex = false;
                                break;
                            }
                            else if (MIDI_SYSTEM_REALTIME_FILTER != (*callbackData & MIDI_SYSTEM_REALTIME_FILTER))
                            {
                                // a sys-ex block is supposed to end with a MIDI_EOX, however
                                // any valid MIDI status byte CAN end a sys-ex block EXCEPT
                                // for system real-time messages (0xF8 to 0xFF)

                                // we have a valid end of sys-ex byte, so turn off
                                // m_bIsInSysEx
                                RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, position));
                                m_IsInSysex = false;
                                break;
                            }
                        }

                        // it's not an interrupting status byte, so copy it to the buffer and advance our positions.
                        //*callbackData = *bufferData;
                        *bufferData = *callbackData;

                        ++callbackData;
                        ++bufferData;

                        ++buffer->dwBytesRecorded;
                        --callbackDataRemaining;

                        // if we were processing the F0 before, we are no longer
                        processingSysExStartByte = false;
                    }

                    // if we are still in sysex and completed this buffer, we can deliver it, the
                    // next iteration will require a new buffer.
                    if (buffer->dwBufferLength == buffer->dwBytesRecorded)
                    {
                        RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, position));
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
                // Guaranteed to have at least 1 byte worth of data available
                // at this point.
                BYTE status = callbackData[0];
                BYTE data1 = 0;
                BYTE data2 = 0;
                bool dataWritten = false;

                // system messages which are not real-time terminate
                // running status
                if ((status >= MIDI_SYSEX) && (status <= MIDI_EOX))
                {
                    m_RunningStatus = 0;
                }

                // tune request, timing, etc. are all 1 byte messages.
                if (status >= MIDI_TUNEREQUEST &&
                    callbackDataRemaining >= 1)
                {
                    callbackData+=1;
                    callbackDataRemaining-=1;
                    dataWritten = true;
                }
                // song select is the only 2 byte message
                else if (status == MIDI_SONGSELECT &&
                    callbackDataRemaining >= 2)
                {
                    data1 = callbackData[1];
                    callbackData+=2;
                    callbackDataRemaining-=2;
                    dataWritten = true;
                }
                // all other status byte messages are 3 bytes,
                // MIDI_MTC, MIDI_SONGPP, and anything less than 0xF0
                else if (0 != (status & MIDI_STATUSBYTEFILTER) && 
                    callbackDataRemaining >= 3)
                {
                    if(status < MIDI_SYSEX)  // don't save system common or system realtime as running status
                    {
                        m_RunningStatus = status;
                    }

                    data1 = callbackData[1];
                    data2 = callbackData[2];
                    callbackData+=3;
                    callbackDataRemaining-=3;
                    dataWritten = true;
                }

                // if it's not a message with a status byte, it should be
                // running status, which requires 2 bytes, and our running
                // status should be valid.
                else if((0 == (status & MIDI_STATUSBYTEFILTER)) && 
                    (0 != (m_RunningStatus & MIDI_STATUSBYTEFILTER)) &&
                    callbackDataRemaining >= 2)
                {
                    status = m_RunningStatus;
                    data1 = callbackData[0];
                    data2 = callbackData[1];

                    // running status only consumes 2 bytes
                    callbackData+=2;
                    callbackDataRemaining-=2;
                    dataWritten = true;
                }
                else
                {
                    // this is an unknown condition, the amount of available
                    // data doesn't align to the type of message being processed,
                    // or we don't have a status byte and aren't in a running status.

                    assert(0);

                    callbackData+=1;
                    callbackDataRemaining-=1;
                    // data not written, this byte ignored.

                    TraceLoggingWrite(
                        WdmAud2TelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingWideString(L"Invalid state, dropping data", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingPointer(this, "this"));
                }

                if (dataWritten)
                {
                    // Convert from the QPC time to the number of ticks elapsed from the start time.
                    DWORD ticks = (DWORD) (((position - startTime) / m_qpcFrequency) * 1000.0);
                    UINT32 midiMessage = status | (data1 << 8) | (data2 << 16);
                    WinmmClientCallback(MIM_DATA, midiMessage, ticks);
                }
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
CMidiPort::SendMidiMessage(UINT32 midiMessage)
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
        RETURN_HR_IF(E_INVALIDARG, nullptr == m_MidisrvTransport);

        // send the message to the transport
        // pass a timestamp of 0 to bypass scheduler
        RETURN_IF_FAILED(m_MidisrvTransport->SendMidiMessage(&midiMessage, sizeof(midiMessage), 0));
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPort::SendLongMessage(LPMIDIHDR buffer)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"Start", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingPointer(this, "this"),
        TraceLoggingPointer(buffer, "buffer"));

    auto exitCallback = wil::scope_exit([&]()
    {
        TraceLoggingWrite(
            WdmAud2TelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingWideString(L"End", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingPointer(this, "this"),
            TraceLoggingPointer(buffer, "buffer"));
    });

    {
        auto lock = m_Lock.lock();

        // This should be on an initialized midi out port
        RETURN_HR_IF(E_INVALIDARG, nullptr == m_MidisrvTransport);

        // The buffer provided must be valid
        RETURN_HR_IF(E_INVALIDARG, nullptr == buffer);
        RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_INVALFLAG), !(buffer->dwFlags & MHDR_PREPARED));

        // send the message to the transport
        // pass a timestamp of 0 to bypass scheduler
        // TODO: based on the buffer length, this message may require chunking into smaller
        // pieces to ensure it fits into the cross process queue.
        RETURN_IF_FAILED(m_MidisrvTransport->SendMidiMessage(buffer->lpData, buffer->dwBufferLength, 0));
    }

    // mark the buffer as completed
    buffer->dwFlags |= MHDR_DONE;

    // client callback indicating this buffer is completed.
    WinmmClientCallback(MOM_DONE, (DWORD_PTR) buffer, 0);
    return S_OK;
}

_Use_decl_annotations_
void
CMidiPort::WinmmClientCallback(UINT msg, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    DriverCallback(m_OpenDesc.dwCallback,
                    HIWORD(m_Flags),
                    (HDRVR) m_OpenDesc.hMidi,
                    msg,
                    m_OpenDesc.dwInstance,
                    param1,
                    param2);
}