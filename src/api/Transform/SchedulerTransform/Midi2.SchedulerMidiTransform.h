// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2SchedulerMidiTransform :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSFORMCREATIONPARAMS, _In_ DWORD *, _In_opt_ IMidiCallback *, _In_ LONGLONG, _In_ IUnknown*));
    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Cleanup)();

private:

    HRESULT GetTopMessageTimestamp(_Out_ internal::MidiTimestamp& timestamp);

    HRESULT CalculateSafeSleepTime(_In_ internal::MidiTimestamp nextWakeupWindowTimestamp, _Out_ uint32_t& sleepMS);


    HRESULT SendMidiMessageNow(
        _In_ PVOID Data,
        _In_ UINT Size,
        _In_ LONGLONG Timestamp);

    HRESULT SendMidiMessageNow(
        _In_ ScheduledUmpMessage const message);

    // priority queue with comparison set up to compare the timestamps and to preserve order.
    // We want the smallest timestamp as front/top of the queue. In the case of duplicate timestamps
    // we want to preserve the order the messages were sent.

    // NOTE: As expected, this works for small-ish numbers of queued messages. But when you start 
    // getting up into the thousands, the delay becomes larger. Example: just over 1ms for 10,000 
    // messages. Not really any other std:: options here, at least for the data structure we need. 
    // It's already using a heap / tree underneath, which is efficient for finding messages.
    // 
    // Consider a separate thread for writing to the queue to make that return quickly.

    std::priority_queue<ScheduledUmpMessage, std::deque<ScheduledUmpMessage>, auto(*)(ScheduledUmpMessage&, ScheduledUmpMessage&)->bool>
        m_messageQueue{ [](_In_ ScheduledUmpMessage& left, _In_ ScheduledUmpMessage& right) ->
            bool
            {
                if (left.Timestamp == right.Timestamp)
                {
                    // preserve receive order for messages with the same timestamp
                    return left.ReceivedIndex > right.ReceivedIndex;
                }
                else
                {
                    return left.Timestamp > right.Timestamp;
                }
            } };

    // this is the minimum amount of ticks into the future to send immediately vs scheduling
    // it is essentially our resolution, and will need to be set based on calculating
    // the actual wake-up frequency
    uint64_t m_tickWindow{ MIDI_SCHEDULER_LOCK_AND_SEND_FUNCTION_LATENCY_TICKS + MIDI_SCHEDULER_ENQUEUE_OVERHEAD_LATENCY_TICKS };

    // TODO: we may allow device latency to be set for the device as a property value with key
    // PKEY_MIDI_MidiOutLatencyTicks. We'd then pre-fetch from the queue by this amount.
    uint64_t m_deviceLatencyTicks{ 0 };

    void QueueWorker();
    
    IMidiCallback* m_callback{ nullptr };
    LONGLONG m_context{ 0 };

    //wil::critical_section m_queueLock;

    std::mutex m_queueMutex;

    uint64_t m_currentReceivedIndex{ 0 };     // need to use the queueLock before writing to this

    //bool m_continueProcessing{ true };
    std::atomic<bool> m_continueProcessing{ true };

    std::thread m_queueWorkerThread;

    //wil::unique_event_nothrow m_messageProcessorWakeup;
    wil::slim_event_manual_reset m_messageProcessorWakeup;

    uint64_t m_timestampFrequency = internal::Shared::GetMidiTimestampFrequency();
};


