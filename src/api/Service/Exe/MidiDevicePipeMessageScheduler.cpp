// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"

_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::Initialize(
    wil::com_ptr_nothrow<CMidiDevicePipe>& devicePipe, 
    DWORD* /*MmcssTaskId*/)
{
    OutputDebugString(L"" __FUNCTION__ " Start up");

    // TODO: See if we should use mmcss here. If so, use the task id
    m_devicePipe = devicePipe;

    // need to make sure this is created before starting up the worker thread
    m_messageProcessorWakeup.create(wil::EventOptions::ManualReset);

    // create and start the queue worker thread
    std::thread workerThread(
        &MidiDevicePipeMessageScheduler::QueueWorker,
        this);

    // TODO: may need to set thread priority
    //SetThreadPriority(workerThread.native_handle(), ... );

    m_queueWorkerThread = std::move(workerThread);
    m_queueWorkerThread.detach();

    return S_OK;
}


_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::Cleanup()
{
    try
    {
        OutputDebugString(L"" __FUNCTION__ " Shut down");

        // tell the thread to quit. Call SetEvent in case it is in a wait
        m_continueProcessing = false;
        m_messageProcessorWakeup.SetEvent();

        // release our access to the underlying device pipe
//        m_devicePipe.reset();

        // join the worker thread and wait for it to end
        if (m_queueWorkerThread.joinable())
            m_queueWorkerThread.join();

        // terminate the event
 //       m_messageProcessorWakeup.reset();

        return S_OK;
    }
    catch (...)
    {
    }

    // TODO: Log
    return E_FAIL;
}

#define MIDI_MESSAGE_SCHEDULE_ENQUEUE_RETRY_COUNT 10

_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::ProcessIncomingMidiMessage(
    PVOID data, 
    UINT size, 
    LONGLONG timestamp)
{
    if (!m_continueProcessing) return S_OK;

    try
    {
        // Check to see if timestamp is in the past or within our tick window: If so, SendMessageNow
        if (shared::GetCurrentMidiTimestamp() >= timestamp - m_tickWindow - m_deviceLatencyTicks)
        {
            //OutputDebugString(L"" __FUNCTION__ " Send message immediately");

            // message is within current tick window, so just send

            return SendMidiMessageNow(data, size, timestamp);
        }
        else
        {
            if (size <= MAX_UMP_BYTES)
            {
                for (int i = 0; i < MIDI_MESSAGE_SCHEDULE_ENQUEUE_RETRY_COUNT && m_continueProcessing; i++)
                {
                    auto lock = m_queueLock.lock();

                    if (lock)
                    {
                        // schedule the message for sending in the future

                        ScheduledMidiMessage msg;
                        memcpy(msg.Data, data, size);
                        msg.ByteCount = size;
                        msg.Timestamp = timestamp;

                        m_messageQueue.push(msg);

                        lock.reset();

                        // notify the worker thread
                        m_messageProcessorWakeup.SetEvent();

                        // break out of the loop and return
                        return S_OK;
                    }
                    else
                    {
                        // TODO: couldn't lock the queue. we should log this 

                    }
                }
            }
            else
            {
                // invalid data size
                return E_FAIL;
            }
        }

        return S_OK;
    }
    catch (...)
    {
        // TODO: Log
        return E_FAIL;
    }
}


_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::SendMidiMessageNow(
    ScheduledMidiMessage const message)
{
    if (!m_continueProcessing) return S_OK;

    try
    {
        if (m_devicePipe != nullptr)
        {
            m_devicePipe->SendMidiMessageNow((PVOID)(message.Data), message.ByteCount, (LONGLONG)(message.Timestamp));
            return S_OK;
        }
    }
    catch (...)
    {

    }

    // TODO Should log this
    return E_FAIL;
}


_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::SendMidiMessageNow(
    PVOID data, 
    UINT size, 
    LONGLONG timestamp)
{
    try
    {
        if (m_devicePipe != nullptr)
        {
            m_devicePipe->SendMidiMessageNow(data, size, timestamp);

            return S_OK;
        }
    }
    catch (...)
    {
    }

    // todo: log
    return E_FAIL;

}

// complete guess. We can fine tune this
#define LOCK_AND_SEND_FUNCTION_LATENCY_TICKS 100

//
// This code is pretty ugly. Happy to entertain cleanup suggestions
//
void MidiDevicePipeMessageScheduler::QueueWorker()
{
    try
    {
        const uint64_t totalExpectedLatency = m_deviceLatencyTicks + LOCK_AND_SEND_FUNCTION_LATENCY_TICKS;

        while (m_continueProcessing)
        {
            if (!m_messageQueue.empty())
            {
                // this block gets us inside the tick window
                if (shared::GetCurrentMidiTimestamp() >= m_messageQueue.top().Timestamp - m_tickWindow - totalExpectedLatency)
                {
                    // Busy loop until we're closer to the target timestamp. This wait will typically be a very small 
                    // amount of time, like under two milliseconds. Really depends on the thread priority and scheduling

                    // this could potentially block other messages. However, the send message function checks to see if
                    // the message is supposed to go out within the tick window. If so, it just sends it. This can/will
                    // lead to jitter that is up to the size of the tick window, so worth revisiting this part of the 
                    // approach.

                    // this loop sneaks up on the actual timestamp from the window start, taking into account the send latency
                    while (m_continueProcessing && shared::GetCurrentMidiTimestamp() < m_messageQueue.top().Timestamp - totalExpectedLatency)
                    {
                        // busy wait. TBD if sleep(0) takes too much time and we need to do something else.
                        Sleep(0);
                    }

                    if (m_continueProcessing)
                    {
                        // if a new message came in and replaced top() while we were looping, it will be within the window
                        // so it's ok to just send that. We'll catch the next one on the next iteration.

                        // this will unlock when it goes out of scope
                        auto lock = m_queueLock.lock();

                        if (lock && !m_messageQueue.empty())
                        {
                            // send the message
                            LOG_IF_FAILED(SendMidiMessageNow(m_messageQueue.top()));

                            // pop the top item off the queue
                            m_messageQueue.pop();

                            lock.reset();   // technically not needed
                        }
                    }
                }
                else
                {
                    // Queue is not empty, but the next message isn't up to send yet.
                    // So we sleep for a minimum amount of time rather than a tight loop.
                    if (!m_messageQueue.empty())
                    {
                        uint64_t ts = m_messageQueue.top().Timestamp - totalExpectedLatency;

                        // see if the difference is more than a quarter of a second. If so, sleep for 200ms.
                        if (ts - shared::GetCurrentMidiTimestamp() > shared::GetMidiTimestampFrequency() / 4)
                        {
                            // we're waiting using the event here in case a message comes in while 
                            // we're sleeping

                            if (m_continueProcessing) m_messageProcessorWakeup.wait(200);
                        }
                    }
                }
            }
            else
            {
                // Queue is empty, so we just wait. The timeout is to make sure we don't hang on shutdown
                // and also helps loosen up the loop so we're not spin waiting and using a full core or something
                if (m_continueProcessing)
                {
                    m_messageProcessorWakeup.wait(1000);

                    if (m_messageProcessorWakeup.is_signaled())
                    {
                        m_messageProcessorWakeup.ResetEvent();
                    }
                }
            }

            // we're looping, not sleeping now, so we can reset this
            if (m_messageProcessorWakeup.is_signaled())
            {
                m_messageProcessorWakeup.ResetEvent();
            }

            if (m_continueProcessing)
            {
                // give up the rest of this timeslice
                Sleep(0);
            }
        }
    }
    catch (...)
    {
        // TODO: Log
    }
}



