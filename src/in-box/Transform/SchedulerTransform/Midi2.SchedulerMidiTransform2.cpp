// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.Schedulertransform.h"

namespace
{
    uint64_t MicrosecondsToTicks(_In_ uint64_t const microseconds, _In_ uint64_t const frequency)
    {
        return (microseconds * frequency) / MICROSECONDS_PER_SECOND;
    }
}


_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform2::Initialize(
    LPCWSTR deviceId,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD* mmcssTaskId,
    IMidiCallback* callback,
    LONGLONG context,
    IMidiDeviceManager* /*midiDeviceManager*/
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

    m_maxForwardSchedulingTicks = MIDI_OUTGOING_MESSAGE_QUEUE_MAX_FUTURE_SCHEDULING_SECONDS * m_timestampFrequency;

    m_sendPathLatencyTicks = MIDI_SCHEDULER_LOCK_AND_SEND_FUNCTION_LATENCY_TICKS;
    m_tickWindow = MIDI_SCHEDULER_LOCK_AND_SEND_FUNCTION_LATENCY_TICKS + MIDI_SCHEDULER_ENQUEUE_OVERHEAD_LATENCY_TICKS;

    m_maxSpinTicks = MicrosecondsToTicks(MIDI_SCHEDULER_V2_MAX_SPIN_MICROSECONDS, m_timestampFrequency);
    m_timerOvershootTicks = MicrosecondsToTicks(MIDI_SCHEDULER_V2_TIMER_OVERSHOOT_MICROSECONDS, m_timestampFrequency);
    m_minimumTimerWaitTicks = MicrosecondsToTicks(MIDI_SCHEDULER_V2_MINIMUM_TIMER_WAIT_MICROSECONDS, m_timestampFrequency);
    m_maxDeviceLatencyTicks = MicrosecondsToTicks(MIDI_SCHEDULER_V2_MAX_DEVICE_LATENCY_MICROSECONDS, m_timestampFrequency);

    m_wakeupEvent.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    RETURN_LAST_ERROR_IF_NULL(m_wakeupEvent);

    m_stopEvent.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    RETURN_LAST_ERROR_IF_NULL(m_stopEvent);

    // Half a millisecond of resolution without touching the global timer interrupt rate. Falls
    // back to a normal timer if the flag is refused, which costs accuracy but still does not spin.
    m_timer.reset(CreateWaitableTimerExW(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));

    if (!m_timer)
    {
        m_timer.reset(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_MANUAL_RESET, TIMER_ALL_ACCESS));

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"High resolution timer unavailable. Using default resolution.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }

    RETURN_LAST_ERROR_IF_NULL(m_timer);

    m_dueMessages.reserve(MIDI_SCHEDULER_MAX_MESSAGES_TO_PROCESS_AT_ONCE);

    ReadDeviceLatency();

    m_queueWorkerThread = std::thread(&CMidi2SchedulerMidiTransform2::QueueWorker, this);

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
CMidi2SchedulerMidiTransform2::Shutdown()
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
        m_stopRequested = true;

        if (m_stopEvent)
        {
            SetEvent(m_stopEvent.get());
        }

        if (m_queueWorkerThread.joinable())
        {
            m_queueWorkerThread.join();
        }

        m_callback = nullptr;

        {
            std::scoped_lock<std::mutex> lock(m_queueMutex);
            m_messageQueue = {};
            m_queueCount = 0;
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

        return S_OK;
    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);

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


_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform2::SendMidiMessageNow(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT size,
    LONGLONG timestamp)
{
    try
    {
        if (m_callback != nullptr && data != nullptr)
        {
            RETURN_IF_FAILED(m_callback->Callback(optionFlags, data, size, timestamp, m_context));

            return S_OK;
        }

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
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);

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


_Use_decl_annotations_
HRESULT
CMidi2SchedulerMidiTransform2::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID data,
    UINT size,
    LONGLONG timestamp)
{
    if (m_stopRequested) return S_OK;

    auto const timestampNow = internal::GetCurrentMidiTimestamp();

    // A zero timestamp, or one already past, bypasses scheduling entirely.
    if (timestamp == 0 || static_cast<uint64_t>(timestamp) <= timestampNow)
    {
        auto const hr = SendMidiMessageNow(optionFlags, data, size, timestamp);

        if (SUCCEEDED(hr))
        {
            return HR_S_MIDI_SENDMSG_IMMEDIATE;
        }

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

    if (static_cast<uint64_t>(timestamp) - timestampNow > m_maxForwardSchedulingTicks)
    {
        RETURN_IF_FAILED(HR_E_MIDI_SENDMSG_TOO_FAR_INTO_FUTURE);
    }

    try
    {
        auto const compensation = m_tickWindow + m_deviceLatencyTicks;

        // Already inside the window where scheduling cannot help, so send it straight through.
        if (static_cast<uint64_t>(timestamp) <= timestampNow + compensation)
        {
            auto const hr = SendMidiMessageNow(optionFlags, data, size, timestamp);

            if (SUCCEEDED(hr))
            {
                return HR_S_MIDI_SENDMSG_IMMEDIATE;
            }

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

        if (size < MINIMUM_UMP_DATASIZE || size > MAXIMUM_UMP_DATASIZE)
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

            return HR_E_MIDI_SENDMSG_INVALID_MESSAGE;
        }

        bool queued{ false };

        {
            std::scoped_lock<std::mutex> lock(m_queueMutex);

            if (m_messageQueue.size() < MIDI_OUTGOING_MESSAGE_QUEUE_MAX_MESSAGE_COUNT)
            {
                if (m_messageQueue.empty())
                {
                    m_currentReceivedIndex = 0;
                }
                else
                {
                    m_currentReceivedIndex++;
                }

                m_messageQueue.emplace(
                    static_cast<internal::MidiTimestamp>(timestamp),
                    m_currentReceivedIndex,
                    optionFlags,
                    size,
                    static_cast<BYTE const*>(data));

                m_queueCount = m_messageQueue.size();

                queued = true;
            }
        }

        if (!queued)
        {
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

        if (!m_stopRequested)
        {
            SetEvent(m_wakeupEvent.get());
        }

        return HR_S_MIDI_SENDMSG_SCHEDULED;
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
bool
CMidi2SchedulerMidiTransform2::TakeDueMessages(
    internal::MidiTimestamp const now,
    internal::MidiTimestamp& nextDueTime)
{
    nextDueTime = 0;

    m_dueMessages.clear();

    std::scoped_lock<std::mutex> lock(m_queueMutex);

    auto const compensation = m_deviceLatencyTicks + m_sendPathLatencyTicks;

    while (!m_messageQueue.empty() && m_dueMessages.size() < MIDI_SCHEDULER_MAX_MESSAGES_TO_PROCESS_AT_ONCE)
    {
        auto const& top = m_messageQueue.top();

        // Guarded subtraction: timestamps below the compensation only occur in test scenarios.
        auto const dueTime = (top.Timestamp > compensation) ? (top.Timestamp - compensation) : 0;

        if (dueTime > now)
        {
            nextDueTime = dueTime;
            break;
        }

        m_dueMessages.push_back(top);
        m_messageQueue.pop();
    }

    if (nextDueTime == 0 && !m_messageQueue.empty())
    {
        auto const& top = m_messageQueue.top();
        nextDueTime = (top.Timestamp > compensation) ? (top.Timestamp - compensation) : 0;
    }

    m_queueCount = m_messageQueue.size();

    return !m_dueMessages.empty();
}


void
CMidi2SchedulerMidiTransform2::ReadDeviceLatency()
{
    // Deliberately no WIL exception macros here. This binary's pch does not include wil/cppwinrt.h,
    // so LOG_CAUGHT_EXCEPTION on a cppwinrt exception would fail fast the whole service.
    try
    {
        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks);
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutCustomLatencyTicks);
        additionalProperties.Append(STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride);

        auto deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            m_endpointDeviceId,
            additionalProperties,
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        if (deviceInfo == nullptr) return;

        auto const properties = deviceInfo.Properties();

        auto const calculatedTicks = internal::GetDeviceInfoProperty<uint64_t>(
            properties, STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks, 0);

        auto const customTicks = internal::GetDeviceInfoProperty<uint64_t>(
            properties, STRING_PKEY_MIDI_MidiOutCustomLatencyTicks, 0);

        auto const useCustom = internal::GetDeviceInfoProperty<bool>(
            properties, STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride, false);

        auto latencyTicks = useCustom ? customTicks : calculatedTicks;

        if (latencyTicks > m_maxDeviceLatencyTicks)
        {
            latencyTicks = m_maxDeviceLatencyTicks;
        }

        m_deviceLatencyTicks = latencyTicks;

        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Outgoing latency compensation resolved", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingUInt64(calculatedTicks, "calculated ticks"),
            TraceLoggingUInt64(customTicks, "custom ticks"),
            TraceLoggingBool(useCustom, "use custom"),
            TraceLoggingUInt64(m_deviceLatencyTicks, "applied ticks")
        );
    }
    catch (...)
    {
        // Leaving the compensation at zero is the same as not having the properties at all.
        TraceLoggingWrite(
            MidiSchedulerTransformTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to read outgoing latency properties. Using no compensation.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );
    }
}


_Use_decl_annotations_
uint64_t
CMidi2SchedulerMidiTransform2::CalculateSpinBudgetTicks(internal::MidiTimestamp const dueTime) const noexcept{
    // Capped at a fraction of the gap to the previously serviced message, so a dense stream cannot
    // turn the guard band into a continuous busy wait.
    if (m_previousDueTime != 0 && dueTime > m_previousDueTime)
    {
        auto const gap = dueTime - m_previousDueTime;
        auto const allowed = gap / MIDI_SCHEDULER_V2_SPIN_GAP_DIVISOR;

        if (allowed < m_maxSpinTicks)
        {
            return allowed;
        }
    }

    return m_maxSpinTicks;
}


_Use_decl_annotations_
void
CMidi2SchedulerMidiTransform2::SpinUntil(internal::MidiTimestamp const target) const noexcept
{
    while (internal::GetCurrentMidiTimestamp() < target)
    {
        if (m_stopRequested) return;

        YieldProcessor();
    }
}


void CMidi2SchedulerMidiTransform2::QueueWorker()
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
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        HANDLE waitHandles[]{ m_stopEvent.get(), m_wakeupEvent.get(), m_timer.get() };

        while (!m_stopRequested)
        {
            auto const now = internal::GetCurrentMidiTimestamp();

            internal::MidiTimestamp nextDueTime{ 0 };

            if (TakeDueMessages(now, nextDueTime))
            {
                for (auto const& message : m_dueMessages)
                {
                    if (m_stopRequested) break;

                    auto const hr = SendMidiMessageNow(
                        message.OptionFlags,
                        const_cast<PVOID>(static_cast<void const*>(message.Data)),
                        message.ByteCount,
                        static_cast<LONGLONG>(message.Timestamp));

                    if (FAILED(hr))
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

                        break;
                    }
                }

                m_dueMessages.clear();

                m_previousDueTime = now;

                continue;
            }

            if (nextDueTime == 0)
            {
                // Nothing queued. Sleep until a message arrives or shutdown, with no periodic wake.
                WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                continue;
            }

            auto const remaining = (nextDueTime > now) ? (nextDueTime - now) : 0;

            if (remaining == 0) continue;

            auto const spinBudget = CalculateSpinBudgetTicks(nextDueTime);

            if (remaining <= spinBudget)
            {
                SpinUntil(nextDueTime);
                continue;
            }

            // Aim the timer to land one spin budget short of the due time, allowing for the fact
            // that a high resolution timer wakes consistently late. The clock is re-read on the
            // next pass, so an imperfect estimate costs at most one extra short wait.
            auto const waitTicks = remaining - spinBudget - (std::min)(m_timerOvershootTicks, remaining - spinBudget);

            if (waitTicks < m_minimumTimerWaitTicks)
            {
                SpinUntil(nextDueTime);
                continue;
            }

            LARGE_INTEGER dueTime{};
            dueTime.QuadPart = -static_cast<LONGLONG>((waitTicks * 10000000ull) / m_timestampFrequency);

            if (SetWaitableTimer(m_timer.get(), &dueTime, 0, nullptr, nullptr, FALSE))
            {
                WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, INFINITE);
                CancelWaitableTimer(m_timer.get());
            }
            else
            {
                LOG_LAST_ERROR();

                SpinUntil(nextDueTime);
            }
        }
    }
    catch (...)
    {
        LOG_IF_FAILED(E_FAIL);

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
}
