// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "MidiSrvPort.h"

#define CALC_TICKS(pos) ((DWORD) (((pos) * 1000.0) / m_qpcFrequency))

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
    DWORD ticks = CALC_TICKS(position - m_StartTime);

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

        // The data in here is all coming from midisrv, so we can make a few assumptions:
        // 
        // 1. No running status. Messages translated from UMP to bytestream do not use
        //    running status at all. 
        // 
        // 2. Messages that fit into a single UMP will not be interrupted by system
        //    real-time messages. So the MIDI 1.0 scenario where a clock message could
        //    show up in between the data bytes in a channel voice message will not 
        //    happen here. It's easy enough to handle that, but not required.
        // 
        // 3. SysEx messages could be interrupted by system real-time messages, so 
        //    we do need to handle that case. 
        // 
        // 4. SysEx messages could be interrupted by non-sysex, non-real-time messages
        //    and so need to be terminated.
        //
        // 5. SysEx messages could be incomplete. It's possible to send a SysEx start
        //    UMP and never send a SysEx complete UMP. Even in WinMM today, this never
        //    sends the SysEx buffer because there is no timeout or anything with
        //    the buffers. Fixing that is not in scope here, beyond making sure we
        //    do not crash when it happens.
        //
        // 6. SysEx start messages could be incorrect. For example, we could get a 
        //    SysEx start message followed by another SysEx start message. This 
        //    violates the protocol, but it's possible. We'd need to look into what
        //    libmidi2 does in that case, but should also guard against that here.


        // the message we're building
        BYTE inProgressMidiMessage[3]{ 0 };         // status, data 1, data 2
        BYTE countMidiMessageBytesReceived = 0;

        BYTE prioritySingleByteMessage{ 0 }; // real time or other single-byte message that could interrupt the in-progress message

        LPMIDIHDR buffer{ nullptr };

        // keep processing until we run out of callback data
        // we process one byte at a time here
        while (callbackDataRemaining > 0 && started)
        {
            // Handle all system real-time messages first, no matter
            // where they appear in a stream
            if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(*callbackData))
            {
                // send the single-byte message immediately
                // do not add it to the inProgressMidiMessage or any buffer even though
                // this shouldn't happen with midisrv-supplied message data

                prioritySingleByteMessage = *callbackData;
            }
            else if (!m_IsInSysex)
            {
                // we're not already processing sysex, but now we should start
                if (MIDI_BYTE_IS_SYSEX_END_STATUS(*callbackData))
                {
                    // we're not in SysEx mode, but this byte was provided
                    // so we'll send it anyway
                    // Convert from the QPC time to the number of ticks elapsed from the start time.
                    DWORD ticks = CALC_TICKS(position - startTime);
                    DWORD_PTR midiMessage = *callbackData;
                    WinmmClientCallback(MIM_ERROR, midiMessage, ticks);
                }
                else if (MIDI_BYTE_IS_SYSEX_START_STATUS(*callbackData))
                {
                    // we weren't in SysEx mode, and now we're kicking
                    // into a SysEx stream

                    // starting a new buffer of SysEx
                    {
                        auto lock = m_BuffersLock.lock();
                        if (!m_InBuffers.empty())
                        {
                            buffer = m_InBuffers.front();
                        }
                    }

                    // get the buffer, add the F0
                    if (buffer)
                    {
                        // put us into SysEx transfer state.
                        m_IsInSysex = true;

                        BYTE* bufferData = (((BYTE*)buffer->lpData) + buffer->dwBytesRecorded);

                        *bufferData = *callbackData;
                        ++bufferData;
                        ++buffer->dwBytesRecorded;
                    }
                    else
                    {
                        // couldn't get the buffer and couldn't add the sysex start byte
                        // wait for either a buffer or the port to be stopped. 
                        // Note: this blocks other data processing with an infinite wait

                        HANDLE handles[] = { m_BuffersAdded.get(), m_Stopped.get() };
                        WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);

                        // now that we have a buffer, or have stopped, reprocess
                        continue;
                    }
                }
                else if (MIDI_BYTE_IS_STATUS_BYTE(*callbackData))
                {
                    // start a new message
                    inProgressMidiMessage[0] = *callbackData;
                    countMidiMessageBytesReceived = 1;
                }
                else if (MIDI_BYTE_IS_DATA_BYTE(*callbackData))
                {
                    // Handle a data byte, and not sysex.

                    if (countMidiMessageBytesReceived >= 1 && countMidiMessageBytesReceived < 3)
                    {
                        inProgressMidiMessage[countMidiMessageBytesReceived++] = *callbackData;
                    }
                    else
                    {
                        // This is bad data. Could be the rest of an interrupted SysEx message.
                        // We can just discard it, but it could be better for the receiving program
                        // to do that. We could even put in a reg setting to control this behavior.

                        // Convert from the QPC time to the number of ticks elapsed from the start time.
                        //DWORD ticks = CALC_TICKS(position - startTime);
                        //DWORD_PTR midiMessage = *callbackData;
                        //WinmmClientCallback(MIM_ERROR, midiMessage, ticks);
                    }
                }
            }
            else if (m_IsInSysex)
            {
                {
                    auto lock = m_BuffersLock.lock();
                    if (!m_InBuffers.empty())
                    {
                        buffer = m_InBuffers.front();
                    }
                }

                if (buffer)
                {
                    // we have a status byte that is not the end status
                    if (MIDI_BYTE_IS_STATUS_BYTE(*callbackData) && !MIDI_BYTE_IS_SYSEX_END_STATUS(*callbackData))
                    {
                        // other statuses do not get added to the buffer, so we handle it here.
                        // they do, however, terminate SysEx and then get added to the normal message handling
                        m_IsInSysex = false;

                        // send current SysEx buffer because any status byte that
                        // is not a real-time status byte will terminate current SysEx
                        RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGERROR, position));

                        // spin back around so we reprocess this byte now that we've terminated sysex
                        continue;
                    }

                    // we have a data byte, or the end status. Either way, we're going to add it to the buffer.
                    else if (MIDI_BYTE_IS_DATA_BYTE(*callbackData) || MIDI_BYTE_IS_SYSEX_END_STATUS(*callbackData))
                    {
                        // either the end byte or a data byte, for sysex. Either way, add it to the buffer
                        BYTE* bufferData = (((BYTE*)buffer->lpData) + buffer->dwBytesRecorded);

                        *bufferData = *callbackData;
                        ++bufferData;
                        ++buffer->dwBytesRecorded;

                        // the byte we received is a proper sysex end byte, so complete it
                        if (MIDI_BYTE_IS_SYSEX_END_STATUS(*callbackData))
                        {
                            m_IsInSysex = false;
                            RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, position));
                        }
                        else if (buffer->dwBytesRecorded == buffer->dwBufferLength)
                        {
                            // The buffer is full now so send what we have, but we are still in SysEx mode
                            RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, position));
                        }
                    }
                }
                else
                {
                    // we have remaining callback data, but no buffer available, wait for either a buffer
                    // or the port to be stopped. Note: this blocks other data processing

                    HANDLE handles[] = { m_BuffersAdded.get(), m_Stopped.get() };
                    WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);

                    // now that we have a buffer, or have stopped, reprocess this byte or end the loop
                    continue;
                }

            }

            // do we have a single-byte priority message to send?
            if (MIDI_BYTE_IS_STATUS_BYTE(prioritySingleByteMessage))
            {
                // Convert from the QPC time to the number of ticks elapsed from the start time.
                DWORD ticks = CALC_TICKS(position - startTime);
                DWORD_PTR midiMessage{ prioritySingleByteMessage };
                WinmmClientCallback(MIM_DATA, midiMessage, ticks);

                prioritySingleByteMessage = 0;
            }

            // do we have a complete non-SysEx message to send? If so, send it
            if (countMidiMessageBytesReceived > 0)
            {
                if (countMidiMessageBytesReceived == 3 && MIDI_MESSAGE_IS_THREE_BYTES(inProgressMidiMessage[0]) ||
                    countMidiMessageBytesReceived == 2 && MIDI_MESSAGE_IS_TWO_BYTES(inProgressMidiMessage[0]) ||
                    countMidiMessageBytesReceived == 1 && MIDI_MESSAGE_IS_ONE_BYTE(inProgressMidiMessage[0]))
                {
                    // Convert from the QPC time to the number of ticks elapsed from the start time.
                    DWORD ticks = CALC_TICKS(position - startTime);
                    DWORD_PTR midiMessage{ static_cast<DWORD_PTR>(inProgressMidiMessage[0]) };

                    if (MIDI_MESSAGE_IS_TWO_BYTES(inProgressMidiMessage[0]) || 
                        MIDI_MESSAGE_IS_THREE_BYTES(inProgressMidiMessage[0]))
                    {
                        midiMessage |= (static_cast<DWORD_PTR>(inProgressMidiMessage[1]) << 8);
                    }

                    if (MIDI_MESSAGE_IS_THREE_BYTES(inProgressMidiMessage[0]))
                    {
                        midiMessage |= (static_cast<DWORD_PTR>(inProgressMidiMessage[2]) << 16);
                    }

                    WinmmClientCallback(MIM_DATA, midiMessage, ticks);

                    // reset for next message
                    countMidiMessageBytesReceived = 0;

                    inProgressMidiMessage[0] = 0;
                    inProgressMidiMessage[1] = 0;
                    inProgressMidiMessage[2] = 0;
                }
            }

            // move to the next byte of data in the loop
            ++callbackData;
            --callbackDataRemaining;

            // before we continue on to the next buffer, we need to find out if the
            // port is still running.
            if (callbackDataRemaining > 0)
            {
                auto lock = m_Lock.lock();
                started = m_Started;
            }
        }

        //// We fell out of the loop. If callback data remaining is 0 and we have some sysex in the buffer, send it
        //if (buffer && started)
        //{
        //    if (buffer->dwBytesRecorded > 0 && callbackDataRemaining == 0)
        //    {
        //        RETURN_IF_FAILED(CompleteLongBuffer(MIM_LONGDATA, position));
        //    }
        //}
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

        // NOTE: sizeof(midiMessage) there is not correct for how the service reads data. This results
        // in extra 00 byte data being sent to the service, and then in the translator, being read as
        // running status values for the initial status byte.

        UINT messageSize{ sizeof(UINT32) };
        byte status = midiMessage & 0x000000FF;
        //byte* messagePointer = (byte*)(&midiMessage);

        // if it's a valid status byte, then figure
        // out the actual size of the message. If it's
        // not a valid status byte, then we just drop it
        if (MIDI_BYTE_IS_STATUS_BYTE(status))
        {
            if (MIDI_MESSAGE_IS_ONE_BYTE(status))
            {
                messageSize = 1;
            }
            else if (MIDI_MESSAGE_IS_TWO_BYTES(status))
            {
                messageSize = 2;
            }
            else if (MIDI_MESSAGE_IS_THREE_BYTES(status))
            {
                messageSize = 3;
            }

            // send the message to the transport
            // pass a timestamp of 0 to bypass scheduler
            RETURN_IF_FAILED(m_MidisrvTransport->SendMidiMessage(&midiMessage, messageSize, 0));
        }
        else
        {
            return E_INVALIDARG;
        }
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
        // 
        //RETURN_IF_FAILED(m_MidisrvTransport->SendMidiMessage(buffer->lpData, buffer->dwBytesRecorded, 0));
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
    //TraceLoggingWrite(
    //    WdmAud2TelemetryProvider::Provider(),
    //    MIDI_TRACE_EVENT_INFO,
    //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingValue(msg, "msg"),
    //    TraceLoggingValue(param1, "param1"),
    //    TraceLoggingValue(param2, "param2"));

    DriverCallback(m_OpenDesc.dwCallback,
                    HIWORD(m_Flags),
                    (HDRVR) m_OpenDesc.hMidi,
                    msg,
                    m_OpenDesc.dwInstance,
                    param1,
                    param2);
}