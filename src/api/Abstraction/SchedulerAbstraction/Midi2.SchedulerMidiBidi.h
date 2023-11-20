// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//class CMidi2SchedulerMidiBiDi :
//    public Microsoft::WRL::RuntimeClass<
//        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
//        IMidiBiDi,
//        IMidiCallback>
//{
//public:
//
//    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ DWORD *, _In_opt_ IMidiCallback *));
//    STDMETHOD(SendMidiMessage(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
//    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);
//    STDMETHOD(Cleanup)();
//
//private:
//    IMidiCallback* m_callback;
//
//    // priority queue with comparison set up to compare the timestamps
//    // we want the smallest timestamp as front/top of the queue.
//
//    // NOTE: As expected, this works for small-ish numbers of queued 
//    // messages. But when you start getting up into the thousands, the
//    // delay becomes larger. Example: just over 1ms for 10,000 messages.
//    // Not really any other std:: options here, at least for the data
//    // structure we need. It's already using a heap / tree underneath.
//    // 
//    // Consider a separate thread for writing to the queue to make that return quickly.
//    std::priority_queue<ScheduledMidiMessage, std::vector<ScheduledMidiMessage>, auto(*)(ScheduledMidiMessage&, ScheduledMidiMessage&)->bool>
//        m_messageQueue{ [](_In_ ScheduledMidiMessage& left, _In_ ScheduledMidiMessage& right) -> bool { return left.Timestamp > right.Timestamp; } };
//
//    // this is the minimum amount of ticks into the future to send immediately vs scheduling
//    // it is essentially our resolution, and will need to be set based on calculating
//    // the actual wake-up frequency
//    uint64_t m_tickWindow{ BASE_MESSAGE_SCHEDULER_TICK_WINDOW };
//
//    // TODO: we may allow device latency to be set for the device as a property value with key
//    // PKEY_MIDI_MidiOutLatencyTicks. We'd then pre-fetch from the queue by this amount.
//    uint64_t m_deviceLatencyTicks{ 0 };
//
//    // pointer back to the device pipe that owns this scheduler
//    wil::com_ptr_nothrow<CMidiDevicePipe> m_devicePipe;
//
//    void QueueWorker();
//
//
//    wil::critical_section m_queueLock;
//    bool m_continueProcessing{ true };
//
//    std::thread m_queueWorkerThread;
//
//    wil::unique_event_nothrow m_messageProcessorWakeup;
//
//};


