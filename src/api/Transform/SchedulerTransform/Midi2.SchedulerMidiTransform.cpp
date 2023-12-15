// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.Schedulertransform.h"

_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform::Initialize(
    LPCWSTR deviceId,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD * mmcssTaskId,
    IMidiCallback * callback,
    LONGLONG context
)
{
    UNREFERENCED_PARAMETER(deviceId);
    UNREFERENCED_PARAMETER(creationParams);
    UNREFERENCED_PARAMETER(mmcssTaskId);
    

    OutputDebugString(L"" __FUNCTION__ " Start up");

    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );


    m_callback = callback;

    m_context = context;

    // need to make sure this is created before starting up the worker thread
 //   m_messageProcessorWakeup.create(wil::EventOptions::ManualReset);

    // create the queue worker thread
    std::thread workerThread(
        &CMidi2SchedulerMidiTransform::QueueWorker,
        this);

    // TODO: may need to set thread priority
    //SetThreadPriority(workerThread.native_handle(), ... );

    m_queueWorkerThread = std::move(workerThread);

    // start up the worker thread
    m_queueWorkerThread.detach();


    return S_OK;
}

HRESULT
CMidi2SchedulerMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    try
    {
        OutputDebugString(L"" __FUNCTION__ " Shut down time");


        // tell the thread to quit. Call SetEvent in case it is in a wait
        m_continueProcessing = false;

        // give the thread a change to terminate
        m_messageProcessorWakeup.SetEvent();
        Sleep(0);

        //// clear the queue
        //bool locked = false;
        //while (!locked)
        //{
        //    // clear the queue
        //    if (m_queueLock.try_lock())
        //    {
        //        while (m_messageQueue.size() > 0)
        //        {
        //            m_messageQueue.pop();
        //        }

        //        locked = true;
        //    }

        //    Sleep(0);
        //}


        // join the worker thread and wait for it to end
        if (m_queueWorkerThread.joinable())
            m_queueWorkerThread.join();

        // don't let the component shut down until we're sure everything is wrapped up.
        // this has a bit of a smell to it, but was having problems with the queue
        // getting torn down before the worker thread was done

        //locked = false;

        //while (!locked)
        //{
        //    // clear the queue
        //    if (m_queueLock.try_lock())
        //    {
        //        locked = true;
        //    }

        //    Sleep(0);
        //}

        return S_OK;
    }
    catch (...)
    {
        OutputDebugString(L"" __FUNCTION__ " Exception cleaning up");

        // TODO: Log
        return S_OK;    // we don't care when cleaning up
    }
}

// This is called for messages coming out of the scheduler queue (via
// the overload), or when the scheduler is bypassed due to a timestamp
// of 0 or a timestamp within the "now" send window
_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform::SendMidiMessageNow(
    PVOID data,
    UINT size,
    LONGLONG timestamp)
{
    try
    {
        if (m_callback != nullptr)
        {
            m_callback->Callback(data, size, timestamp, m_context);
            return S_OK;
        }
        else
        {
            // TODO: Log this
            return E_FAIL;
        }
    }
    catch (...)
    {
        // TODO: Log
        return E_FAIL;
    }

}


// This is called for messages coming out of the scheduler queue
_Use_decl_annotations_
HRESULT 
CMidi2SchedulerMidiTransform::SendMidiMessageNow(
    ScheduledUmpMessage const message)
{
    if (!m_continueProcessing) return S_OK;

    return SendMidiMessageNow(
        (PVOID)(message.Data), 
        message.ByteCount, 
        (LONGLONG)(message.Timestamp));
}




_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform::SendMidiMessage(
    PVOID data,
    UINT size,
    LONGLONG timestamp)
{
    OutputDebugString(L"" __FUNCTION__);


    if (!m_continueProcessing) return S_OK;

    try
    {
        // check to see if we're bypassing scheduling
        if (timestamp == 0)
        {
            // bypass scheduling
            return SendMidiMessageNow(data, size, timestamp);
        }
        else if (shared::GetCurrentMidiTimestamp() >= timestamp - m_tickWindow - m_deviceLatencyTicks)
        {
            // timestamp is in the past or within our tick window: so send now
            return SendMidiMessageNow(data, size, timestamp);
        }
        else
        {
            // otherwise, we schedule the message

            if (size >= MINIMUM_UMP_DATASIZE && size <= MAXIMUM_UMP_DATASIZE)
            {
                for (int i = 0; i < MIDI_MESSAGE_SCHEDULE_ENQUEUE_RETRY_COUNT && m_continueProcessing; i++)
                {
                    auto lock = m_queueLock.try_lock();

                    if (lock)
                    {
                        // recycle the current received index whenever the queue is empty. Prevents long-term wrapping
                        if (m_messageQueue.size() == 0)
                        {
                            m_currentReceivedIndex = 0;
                        }

                        // schedule the message for sending in the future

                        if (m_messageQueue.size() < MIDI_OUTGOING_MESSAGE_QUEUE_MAX_MESSAGE_COUNT)
                        {
                            m_messageQueue.emplace(timestamp, ++m_currentReceivedIndex, size, (byte*)data);

                            // need to reset this before setting the event
                            lock.reset();

                            // notify the worker thread
                            if (m_continueProcessing) m_messageProcessorWakeup.SetEvent();

                            // break out of the loop and return
                            return S_OK;
                        }
                        else
                        {
                            // priority queue doesn't give us any way to pop from the back
                            // so we just have to fail. 
                            // TODO: Eventually, we'll want to return a code back to the client API that it can interpret and use as retval

                            lock.reset();

                            return E_FAIL;
                        }

                    }
                    else
                    {
                        // TODO: couldn't lock the queue. we should log this 

                    }
                }

                // if we got here, we couldn't lock the queue
                // should return a better code
                return E_FAIL;
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
HRESULT
CMidi2SchedulerMidiTransform::GetTopMessageTimestamp(internal::MidiTimestamp &timestamp)
{
    if (!m_continueProcessing) return E_FAIL;

    HRESULT ret = E_FAIL;
    timestamp = 0;

    try
    {
        // auto resets on exit
        auto lock = m_queueLock.try_lock();

        if (lock)
        {
            if (m_continueProcessing && !m_messageQueue.empty())
            {
                timestamp = m_messageQueue.top().Timestamp;
                ret = S_OK;

            //    OutputDebugString(L"\n--Retrieved current timestamp\n");
            }
            else
            {
                ret = E_FAIL;
            }
        }
        else
        {
            ret = E_FAIL;
        }
    }
    catch (...)
    {
        ret = E_FAIL;
    }

    return ret;
}

//
// This code is pretty ugly. Happy to entertain cleanup / optimization suggestions
//
void CMidi2SchedulerMidiTransform::QueueWorker()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    try
    {
        const uint64_t totalExpectedLatency = m_deviceLatencyTicks + LOCK_AND_SEND_FUNCTION_LATENCY_TICKS;

        while (m_continueProcessing)
        {
            internal::MidiTimestamp topTimestamp = 0;

            if (m_continueProcessing) Sleep(0);

            if (m_continueProcessing && !m_messageQueue.empty())
            {
                if (SUCCEEDED(GetTopMessageTimestamp(topTimestamp)))
                {
                    // this block gets us inside the tick window
                    if (shared::GetCurrentMidiTimestamp() >= topTimestamp - m_tickWindow - totalExpectedLatency)
                    {
                        // Busy loop until we're closer to the target timestamp. This wait will typically be a very small 
                        // amount of time, like under two milliseconds. Really depends on the thread priority and scheduling

                        // this could potentially block other messages. However, the send message function checks to see if
                        // the message is supposed to go out within the tick window. If so, it just sends it. This can/will
                        // lead to jitter that is up to the size of the tick window, so worth revisiting this part of the 
                        // approach.

                        // this loop sneaks up on the actual timestamp from the window start, taking into account the send latency
                        while (m_continueProcessing && shared::GetCurrentMidiTimestamp() < topTimestamp - totalExpectedLatency)
                        {
                            // busy wait. TBD if sleep(0) takes too much time and we need to do something else.
                            Sleep(0);
                        }

                        if (m_continueProcessing)
                        {
                            // if a new message came in and replaced top() while we were looping, it will be within the window
                            // so it's ok to just send that. We'll catch the next one on the next iteration. We don't really care
                            // if this is the same message we started with.

                            auto lock = m_queueLock.try_lock();

                            if (lock)
                            {
                                if (!m_messageQueue.empty())
                                {
                                    // send the message
                                    LOG_IF_FAILED(SendMidiMessageNow(m_messageQueue.top()));

                                    // pop the top item off the queue
                                    m_messageQueue.pop();
                                }

                                //lock.reset();
                            }
                        }
                    }
                    else
                    {
                        // Queue is not empty, but the next message isn't up to send yet.
                        // So we sleep for a minimum amount of time rather than a tight loop.
                        if (m_continueProcessing && !m_messageQueue.empty())
                        {
                            uint64_t ts = m_messageQueue.top().Timestamp - totalExpectedLatency;

                            // see if the difference is more than a quarter of a second. If so, sleep for 200ms.
                            if (ts - shared::GetCurrentMidiTimestamp() > shared::GetMidiTimestampFrequency() / 4)
                            {
                                // we're waiting using the event here in case a message comes in while 
                                // we're sleeping

                                if (m_continueProcessing)
                                {
                                    m_messageProcessorWakeup.wait(200);
                                }
                            }
                        }
                        else if (m_continueProcessing)
                        {
                            // Queue is empty, so we just wait. The timeout is to make sure we don't hang on shutdown
                            // and also helps loosen up the loop so we're not spin waiting and using a full core or something
                            m_messageProcessorWakeup.wait(1000);

                        }
                    }

                    if (m_continueProcessing)
                    {
                        // we're looping, not sleeping now, so we can reset this
                        if (m_messageProcessorWakeup.is_signaled())
                        {
                            m_messageProcessorWakeup.ResetEvent();
                        }
                    }
                }
                else
                {
                    // couldn't get top timestamp for some reason.
                }
            }
            else
            {
                // queue likely empty
            }
        }
    }
    catch (...)
    {
        // TODO: Log
    }

    OutputDebugString(L"" __FUNCTION__ " Exit");
}


