// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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
    LONGLONG context,
    IMidiDeviceManagerInterface* /*MidiDeviceManager*/
)
{
    UNREFERENCED_PARAMETER(creationParams);
    UNREFERENCED_PARAMETER(mmcssTaskId);
    
    m_endpointDeviceId = deviceId;


    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );


    m_callback = callback;
    m_context = context;

    // create the queue worker thread
    std::jthread workerThread(
        &CMidi2SchedulerMidiTransform::QueueWorker,
        this);

    m_queueWorkerThread = std::move(workerThread);
    m_queueWorkerThreadStopToken = m_queueWorkerThread.get_stop_token();

    // start up the worker thread
    m_queueWorkerThread.detach();


    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

HRESULT
CMidi2SchedulerMidiTransform::Shutdown()
{
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

    try
    {

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Setting wakeup event", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );


        // tell the thread to quit. Also call SetEvent in case it is in a wait
        m_queueWorkerThread.request_stop();

        // Alert the thread in case it's in a wait state, and then give the thread a chance to terminate
        m_messageProcessorWakeup.SetEvent();

        uint16_t cleanupAttempts{ 0 };
        while (!m_queueWorkerThreadCleanlyExited && cleanupAttempts < 100)
        {
            // note: with typical settings, thread will sleep for 15.625ms minimum
            std::this_thread::sleep_for(10ms);
            cleanupAttempts++;
        }

        m_callback = nullptr;

        if (!m_queueWorkerThreadCleanlyExited)
        {
            TraceLoggingWrite(
                MidiSchedulerTransformTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exiting, but worker thread did not cleanly exit.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            return E_FAIL;
        }
        else
        {
            m_messageQueue = {};    // clear the queue

            TraceLoggingWrite(
                MidiSchedulerTransformTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
            );

            return S_OK;
        }

    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);      // cause fallback error to be logged

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception cleaning up", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

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
#ifdef MIDI_DETAILED_TRACING
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)

    );
#endif

    try
    {
        if (m_callback != nullptr && data != nullptr)
        {
            RETURN_IF_FAILED(m_callback->Callback(data, size, timestamp, m_context));

            return S_OK;
        }
        else
        {
            TraceLoggingWrite(
                MidiSchedulerTransformTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Callback or data is nullptr", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)

            );

            return E_FAIL;
        }
    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);      // cause fallback error to be logged

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception sending MIDI Message", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)

        );

        return E_FAIL;
    }

}


// This is called for messages coming out of the scheduler queue
_Use_decl_annotations_
HRESULT 
CMidi2SchedulerMidiTransform::SendMidiMessageNow(
    ScheduledUmpMessage const& message)
{
    if (m_queueWorkerThreadStopToken.stop_requested()) return S_OK;

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
#ifdef MIDI_DETAILED_TRACING
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );
#endif

    if (m_queueWorkerThreadStopToken.stop_requested()) return S_OK;

    // check to see if we're bypassing scheduling by sending a zero timestamp
    if (timestamp == 0)
    {
        // bypass scheduling logic completely
        auto hr = SendMidiMessageNow(data, size, timestamp);

        if (SUCCEEDED(hr))
        {
            return HR_S_MIDI_SENDMSG_IMMEDIATE;
        }
        else
        {
            TraceLoggingWrite(
                MidiSchedulerTransformTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Error sending MIDI Message now (bypass queue)", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            return hr;
        }
    }


    try
    {
        // some timestamps are just indexes for debugging, so we need to check for < 0 after the subtraction
        if ((timestamp <= (LONGLONG)(m_tickWindow + m_deviceLatencyTicks)) ||
            internal::GetCurrentMidiTimestamp() >= timestamp - m_tickWindow - m_deviceLatencyTicks)
        {
            // timestamp is in the past or within our tick window: so send now
            auto hr = SendMidiMessageNow(data, size, timestamp);

            if (SUCCEEDED(hr))
            {
                return HR_S_MIDI_SENDMSG_IMMEDIATE;
            }
            else
            {
                TraceLoggingWrite(
                    MidiSchedulerTransformTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Error sending MIDI Message now (message timestamp older than window)", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                    TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
                );

                return hr;
            }
        }
        else
        {
            // otherwise, we schedule the message

            if (size >= MINIMUM_UMP_DATASIZE && size <= MAXIMUM_UMP_DATASIZE)
            {
                // schedule the message for sending in the future

                if (m_messageQueue.size() < MIDI_OUTGOING_MESSAGE_QUEUE_MAX_MESSAGE_COUNT)
                {
#ifdef MIDI_DETAILED_TRACING
                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Adding message to outgoing queue", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );
#endif

#ifdef MIDI_DETAILED_TRACING
                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Locking scheduler queue", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );
#endif
                    {   // scope for lock

                        std::scoped_lock<std::mutex> lock(m_queueMutex);

#ifdef MIDI_DETAILED_TRACING
                        TraceLoggingWrite(
                            MidiSchedulerTransformTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Lock acquired", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );
#endif
                        // reset the current received index whenever the queue is empty. Prevents long-long-term wrapping
                        if (m_messageQueue.empty())
                        {
                            m_currentReceivedIndex = 0;
                        }
                        else
                        {
                            m_currentReceivedIndex++;
                        }

                        m_messageQueue.emplace(timestamp, m_currentReceivedIndex, size, (byte*)data);
                    }
                    
#ifdef MIDI_DETAILED_TRACING
                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Added message to queue. About to wakeup worker thread.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );
#endif
                    // notify the worker thread
                    if (!m_queueWorkerThreadStopToken.stop_requested()) m_messageProcessorWakeup.SetEvent();
                       
#ifdef MIDI_DETAILED_TRACING
                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Scheduling complete.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );
#endif

                    return HR_S_MIDI_SENDMSG_SCHEDULED;
                }
                else
                {
                    // priority queue doesn't give us any way to pop from the back
                    // so we just have to fail. 

                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Outbound message queue full", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );

                    return HR_E_MIDI_SENDMSG_SCHEDULER_QUEUE_FULL;
                }
            }
            else
            {
                TraceLoggingWrite(
                    MidiSchedulerTransformTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Invalid message data size", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                // invalid data size
                return HR_E_MIDI_SENDMSG_INVALID_MESSAGE;
            }
        }
    }
    catch (...)
    {
        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception scheduling message", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return E_FAIL;
    }

}

_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform::GetTopMessageTimestamp(internal::MidiTimestamp &timestamp)
{
#ifdef MIDI_DETAILED_TRACING
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );
#endif

    if (m_queueWorkerThreadStopToken.stop_requested()) return E_FAIL;

    HRESULT ret = E_FAIL;
    timestamp = 0;

    try
    {
        // auto resets on exit
        //auto lock = m_queueLock.try_lock();

#ifdef MIDI_DETAILED_TRACING
        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Acquiring queue lock", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
#endif

        std::scoped_lock<std::mutex> lock(m_queueMutex);

#ifdef MIDI_DETAILED_TRACING
        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Lock acquired", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
#endif

        if (!m_queueWorkerThreadStopToken.stop_requested() && !m_messageQueue.empty())
        {
            timestamp = m_messageQueue.top().Timestamp;
            ret = S_OK;
        }
        else
        {
            ret = E_FAIL;
        }
    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);  // cause fallback error to be logged

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception getting top message timestamp", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        ret = E_FAIL;
    }

#ifdef MIDI_DETAILED_TRACING
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );
#endif


    return ret;
}


_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform::CalculateSafeSleepTime(
    internal::MidiTimestamp nextWakeupWindowTimestamp, 
    uint32_t& sleepMS
)
{
#ifdef MIDI_DETAILED_TRACING
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );
#endif

    if (m_queueWorkerThreadStopToken.stop_requested()) return E_FAIL;

    HRESULT ret = E_FAIL;
    sleepMS = 0;

    try
    {
        auto now = internal::GetCurrentMidiTimestamp();

        if (nextWakeupWindowTimestamp > now)
        {
            // calculate diff in seconds
            auto diff = (nextWakeupWindowTimestamp - now) / m_timestampFrequency;

            // convert to milliseconds
            sleepMS = (uint32_t)(diff * 1000);

            // if the sleep time is under the limit, we don't sleep at all because the timing is not that accurate
            if (sleepMS < MIDI_SCHEDULER_MINIMUM_EVENT_SLEEP_TIME_MS)
            {
                sleepMS = 0;
            }
        }

        return S_OK;
    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL); // fallback error

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception calculating safe sleep time", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        ret = E_FAIL;
    }

    return ret;
}



void CMidi2SchedulerMidiTransform::QueueWorker()
{
    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    try
    {
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        const uint64_t totalExpectedLatency = m_deviceLatencyTicks + MIDI_SCHEDULER_LOCK_AND_SEND_FUNCTION_LATENCY_TICKS;

        while (!m_queueWorkerThreadStopToken.stop_requested())
        {
            // check to see if the queue is empty, and if so, go to sleep until we're signaled
            // to wake up due to a new message arriving or due to shut down.
            if (m_messageQueue.empty())
            {
                // queue empty so sleep until we get notified to wake up
                m_messageProcessorWakeup.wait(MIDI_OUTBOUND_EMPTY_QUEUE_SLEEP_DURATION_MS);
                //bool triggered = m_messageProcessorWakeup.wait(MIDI_OUTBOUND_EMPTY_QUEUE_SLEEP_DURATION_MS);
            }
            else if (!m_queueWorkerThreadStopToken.stop_requested() && !m_messageQueue.empty())
            {
                internal::MidiTimestamp topTimestamp = 0;

                // get the timestamp of the most urgent message. We fetch this
                // each time in case messages were added since last loop iteration
                if (SUCCEEDED(GetTopMessageTimestamp(topTimestamp)))
                {
                    uint64_t nextMessageSendTime = topTimestamp - totalExpectedLatency;

                    // check to see if it's time to send the message. If not, we'll just
                    // wrap back around
                    if (internal::GetCurrentMidiTimestamp() >= nextMessageSendTime)
                    {
#ifdef MIDI_DETAILED_TRACING
                        TraceLoggingWrite(
                            MidiSchedulerTransformTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Acquiring queue lock", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );
#endif

                        std::scoped_lock<std::mutex> lock(m_queueMutex);

#ifdef MIDI_DETAILED_TRACING
                        TraceLoggingWrite(
                            MidiSchedulerTransformTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Lock acquired", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                        );
#endif
                        // we have the queue locked, so send ALL messages that have the *same* timestamp
                        // but we need to limit the number to send at once here, so we do.

                        uint32_t processedMessages{ 0 };

                        while (!m_messageQueue.empty() && 
                            !m_queueWorkerThreadStopToken.stop_requested() &&
                            processedMessages < MIDI_SCHEDULER_MAX_MESSAGES_TO_PROCESS_AT_ONCE &&
                            m_messageQueue.top().Timestamp <= topTimestamp)
                        {
                            // send the message
                            auto hr = SendMidiMessageNow(m_messageQueue.top());

                            if (SUCCEEDED(hr))
                            {
                                // pop the top item off the queue
                                m_messageQueue.pop();
                            }
                            else
                            {
                                LOG_IF_FAILED(hr);

                                TraceLoggingWrite(
                                    MidiSchedulerTransformTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_ERROR,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Unable to send message", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                    TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
                                );

                                // we failed to send for some reason, so break out of the loop. We'll catch 
                                // these messages the next time around.
                                break;
                            }

                            processedMessages++;
                        }
                    }
                    else
                    {
                        // not yet time to send the top message, so we'll sleep
                        uint32_t sleepTime{ 0 };

                        if (!m_queueWorkerThreadStopToken.stop_requested() && 
                            SUCCEEDED(CalculateSafeSleepTime(nextMessageSendTime, sleepTime)))
                        {
                            // system timers are, by default, good only to about 15.625ms
                            if (sleepTime > 15)
                            {
#ifdef MIDI_DETAILED_TRACING
                                TraceLoggingWrite(
                                    MidiSchedulerTransformTelemetryProvider::Provider(),
                                    MIDI_TRACE_EVENT_INFO,
                                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                    TraceLoggingPointer(this, "this"),
                                    TraceLoggingWideString(L"Going to sleep", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                    TraceLoggingUInt32(sleepTime, "Sleep Time Milliseconds")
                                );
#endif

                                // waiting will get interrupted if a new message comes in. We want that.
                                m_messageProcessorWakeup.wait(sleepTime);
                            }
                        }
                    }
                }
                else
                {
                    // couldn't get top timestamp for some reason.
#ifdef MIDI_DETAILED_TRACING
                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Couldn't get top timestamp", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );
#endif
                }

            }

            if (!m_queueWorkerThreadStopToken.stop_requested())
            {
                // we're looping, not sleeping now, so we need to reset this to make sure we don't miss new messages
                if (m_messageProcessorWakeup.is_signaled())
                {
#ifdef MIDI_DETAILED_TRACING
                    TraceLoggingWrite(
                        MidiSchedulerTransformTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Wakeup signaled. Resetting event", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                    );
#endif
                    m_messageProcessorWakeup.ResetEvent();
                }

                // return the rest of the time slice so we're not in a tight loop
                std::this_thread::sleep_for(0ms);
            }

        } // main loop
    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);      // cause fallback error to be reported

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Exception processing queue", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }

    TraceLoggingWrite(
        MidiSchedulerTransformTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exit", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );


    m_queueWorkerThreadCleanlyExited = true;

}


