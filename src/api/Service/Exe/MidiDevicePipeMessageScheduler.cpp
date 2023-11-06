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
    CMidiDevicePipe* devicePipe, 
    DWORD* /*MmcssTaskId*/)
{
    OutputDebugString(L"" __FUNCTION__ " Start up");

    // TODO: See if we should use mmcss here. If so, use the task id
    m_devicePipe = devicePipe;

    // need to make sure this is created before starting up the worker thread
    m_newDataAvailable.create(wil::EventOptions::ManualReset);

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
    OutputDebugString(L"" __FUNCTION__ " Shut down");

    // tell the thread to quit
    m_continueProcessing = false;

    // join the worker thread and wait
    if (m_queueWorkerThread.joinable())
        m_queueWorkerThread.join();

    m_devicePipe.reset();

    m_newDataAvailable.reset();

    return S_OK;
}


_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::ProcessIncomingMidiMessage(
    PVOID data, 
    UINT size, 
    LONGLONG timestamp)
{
    // Check to see if timestamp is in the past or within our tick window: If so, SendMessageNow

    //OutputDebugString(L"" __FUNCTION__ " message timestamp:");
    //OutputDebugString(std::to_wstring(timestamp).c_str());

    if (shared::GetCurrentMidiTimestamp() >= timestamp - m_tickWindow - m_deviceLatencyTicks)
    {
        OutputDebugString(L"" __FUNCTION__ " Send message immediately");

        // message is within current tick window, so just send

        return SendMidiMessageNow(data, size, timestamp);
    }
    else
    {
        OutputDebugString(L"" __FUNCTION__ " Send message to scheduler queue");

        if (size <= MAX_UMP_BYTES)
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
       //         m_newDataAvailable.SetEvent();
            }
            else
            {
                // couldn't lock
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


_Use_decl_annotations_
HRESULT MidiDevicePipeMessageScheduler::SendMidiMessageNow(
    ScheduledMidiMessage const message)
{
//    OutputDebugString(L"" __FUNCTION__);

    if (m_devicePipe != nullptr)
    {
        m_devicePipe->SendMidiMessageNow((PVOID)(message.Data), message.ByteCount, (LONGLONG)(message.Timestamp));
        return S_OK;
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
//    OutputDebugString(L"" __FUNCTION__);

    if (m_devicePipe != nullptr)
    {
        m_devicePipe->SendMidiMessageNow(data, size, timestamp);

        return S_OK;
    }
    else
    {
        // we should log this
        return E_FAIL;
    }
}

// complete guess. We can fine tune this
#define LOCK_AND_SEND_FUNCTION_LATENCY_TICKS 100

void MidiDevicePipeMessageScheduler::QueueWorker()
{
   const uint64_t totalExpectedLatency = m_deviceLatencyTicks + LOCK_AND_SEND_FUNCTION_LATENCY_TICKS;

    while (m_continueProcessing)    // todo. Change this to check a condition variable so we don't hang on cleanup
    {
        if (!m_messageQueue.empty())
        {
            // this block gets us inside the tick window
            if (shared::GetCurrentMidiTimestamp() >= m_messageQueue.top().Timestamp - m_tickWindow - totalExpectedLatency)
            {
                OutputDebugString(L"" __FUNCTION__ " inside tick window");
                
                // Busy loop until we're closer to the target timestamp. This wait will typically be a very small 
                // amount of time, like under two milliseconds. Really depends on the thread priority and scheduling

                // this could potentially block other messages. However, the send message function checks to see if
                // the message is supposed to go out within the tick window. If so, it just sends it. This can/will
                // lead to jitter that is up to the size of the tick window, so worth revisiting this part of the 
                // approach.

                // this loop sneaks up on the actual timestamp from the window start, taking into account the send latency
                while (shared::GetCurrentMidiTimestamp() < m_messageQueue.top().Timestamp - totalExpectedLatency)
                {
                    // busy wait. TBD if sleep(0) takes too much time and we need to do something else.
                    Sleep(0);
                }

                // if a new message came in and replaced top() while we were looping, it will be within the window
                // so it's ok to just send that. We'll catch the next one on the next iteration.
               
                OutputDebugString(L"" __FUNCTION__ " About to acquire queue lock");

                // this will unlock when it goes out of scope
                auto lock = m_queueLock.lock();

                if (lock && !m_messageQueue.empty())
                {
                    OutputDebugString(L"" __FUNCTION__ " Queue lock acquired. Sending message");

                    // send the message
                    LOG_IF_FAILED(SendMidiMessageNow(m_messageQueue.top()));

                    // pop the top item off the queue
                    m_messageQueue.pop();

                    //if (m_newDataAvailable.is_signaled())
                    //{
                    //    // reset the indicator that there's more data, so we can be signaled again
                    //    m_newDataAvailable.ResetEvent();
                    //}

                    lock.reset();   // technically not needed
                }

                OutputDebugString(L"" __FUNCTION__ " Message sent");
            }
            else
            {
                // Queue is not empty, but the next message isn't up to send yet.
                // So we sleep for a minimum amount of time rather than a tight loop.

                OutputDebugString(L"" __FUNCTION__ " queue is not empty, but time for next message isn't up yet.");

                //if (!m_messageQueue.empty())
                //{
                //    uint64_t ts = m_messageQueue.top().Timestamp - totalExpectedLatency;

                //    // see if the difference is more than a quarter of a second. If so, sleep for 200ms.
                //    if (ts - shared::GetCurrentMidiTimestamp() > shared::GetMidiTimestampFrequency() / 4)
                //    {
                //        OutputDebugString(L"Sleeping for 200ms");

                //        // we're waiting using the event here in case a message comes in while 
                //        // we're sleeping
                //        m_newDataAvailable.wait(200);
                //    }
                //}
            }
        }
        else
        {
            OutputDebugString(L"" __FUNCTION__ " queue empty. Waiting.");

            // if queue is empty, just wait. The timeout is to make sure we don't hang on shutdown
            // and also helps loosen up the loop so we're not spin waiting and using a full core or something
            //m_newDataAvailable.wait(5000);

            //if (m_newDataAvailable.is_signaled())
            //{
            //    OutputDebugString(L"" __FUNCTION__ " New data event signal received.");
            //    //OutputDebugString(std::to_wstring(shared::GetCurrentMidiTimestamp()).c_str());

            //    m_newDataAvailable.ResetEvent();
            //}


            // TEMP!
            Sleep(10);

        }

        // give up the rest of this timeslice
        Sleep(0);
    }
}



