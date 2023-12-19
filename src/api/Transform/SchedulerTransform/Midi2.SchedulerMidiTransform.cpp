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
//    OutputDebugString(L"" __FUNCTION__);


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


_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform::CalculateSafeSleepTime(internal::MidiTimestamp nextWakeupWindowTimestamp, uint32_t & sleepMS)
{
    if (!m_continueProcessing) return E_FAIL;

    HRESULT ret = E_FAIL;
    sleepMS = 0;

    try
    {
        auto now = internal::Shared::GetCurrentMidiTimestamp();

        auto diff = (nextWakeupWindowTimestamp - now) / internal::Shared::GetMidiTimestampFrequency();

        sleepMS = (uint32_t)(diff * 1000);

        return S_OK;
    }
    catch (...)
    {
        ret = E_FAIL;
    }

    return ret;
}





#define MIDI_OUTBOUND_EMPTY_QUEUE_SLEEP_DURATION_MS 60000

void CMidi2SchedulerMidiTransform::QueueWorker()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    try
    {
        const uint64_t totalExpectedLatency = m_deviceLatencyTicks + LOCK_AND_SEND_FUNCTION_LATENCY_TICKS;

        while (m_continueProcessing)
        {
            // check to see if the queue is empty, and if so, go to sleep until we're signaled
            // to wake up due to a new message arriving or due to shut down.
            if (m_continueProcessing && m_messageQueue.empty())
            {
                // queue empty so sleep until we get notified to wake up
                m_messageProcessorWakeup.wait(MIDI_OUTBOUND_EMPTY_QUEUE_SLEEP_DURATION_MS);
            }
            else if (m_continueProcessing && !m_messageQueue.empty())
            {
                internal::MidiTimestamp topTimestamp = 0;

                if (SUCCEEDED(GetTopMessageTimestamp(topTimestamp)))
                {
                    uint64_t nextMessageSendTime = topTimestamp - m_tickWindow - totalExpectedLatency;

                    uint32_t sleepTime{ 0 };

                    if (SUCCEEDED(CalculateSafeSleepTime(nextMessageSendTime, sleepTime)))
                    {
                        if (sleepTime > 0)
                        {
                            m_messageProcessorWakeup.wait(sleepTime);
                        }
                    }

                    // check to see if it's time to send the message. If not, we'll just
                    // wrap back around
                    if (shared::GetCurrentMidiTimestamp() >= nextMessageSendTime)
                    {
                        // we're inside the tick window + latency, so need to just go ahead and send it

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
                    // couldn't get top timestamp for some reason.
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

        } // main loop
    }
    catch (...)
    {
        // TODO: Log
    }

    OutputDebugString(L"" __FUNCTION__ " Exit");
}


