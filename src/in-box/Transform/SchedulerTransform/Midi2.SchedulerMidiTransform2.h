// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Scheduler V2. Selected in CMidi2SchedulerTransform::Activate when
// Feature_Servicing_MIDI2SchedulerV2 is enabled; CMidi2SchedulerMidiTransform is used otherwise.
//
// The difference from V1 is how the worker waits. V1 could only ever sleep in whole seconds and
// refused to sleep at all below two of them, so anything scheduled less than two seconds out was
// busy waited at an elevated priority, which cost a full core per endpoint. V2 waits on a
// high-resolution waitable timer and busy waits only for a bounded guard band immediately before
// a message is due.
class CMidi2SchedulerMidiTransform2 :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDataTransform>
{
public:

    STDMETHOD(Initialize(_In_ LPCWSTR, _In_ PTRANSFORMCREATIONPARAMS, _In_ DWORD*, _In_opt_ IMidiCallback*, _In_ LONGLONG, _In_ IMidiDeviceManager*));
    STDMETHOD(SendMidiMessage(_In_ MessageOptionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG));
    STDMETHOD(Shutdown)();

private:

    void QueueWorker();

    HRESULT SendMidiMessageNow(
        _In_ MessageOptionFlags optionFlags,
        _In_ PVOID data,
        _In_ UINT size,
        _In_ LONGLONG timestamp);

    // Moves every message already due into m_dueMessages, and reports the due time of the earliest
    // message left behind. Holds the lock only for the move.
    bool TakeDueMessages(
        _In_ internal::MidiTimestamp const now,
        _Out_ internal::MidiTimestamp& nextDueTime);

    uint64_t CalculateSpinBudgetTicks(_In_ internal::MidiTimestamp const dueTime) const noexcept;

    void SpinUntil(_In_ internal::MidiTimestamp const target) const noexcept;

    void ReadDeviceLatency();

    std::wstring m_endpointDeviceId{};

    IMidiCallback* m_callback{ nullptr };
    LONGLONG m_context{ 0 };

    std::priority_queue<ScheduledUmpMessage2, std::vector<ScheduledUmpMessage2>, ScheduledUmpMessage2Comparer>
        m_messageQueue;

    std::vector<ScheduledUmpMessage2> m_dueMessages;

    std::mutex m_queueMutex;
    std::atomic<size_t> m_queueCount{ 0 };
    uint64_t m_currentReceivedIndex{ 0 };

    // Kernel objects so the timer and the two signals can be waited on together.
    wil::unique_handle m_wakeupEvent;
    wil::unique_handle m_stopEvent;
    wil::unique_handle m_timer;

    std::thread m_queueWorkerThread;
    std::atomic<bool> m_stopRequested{ false };

    uint64_t m_timestampFrequency{ internal::GetMidiTimestampFrequency() };

    uint64_t m_maxForwardSchedulingTicks{ 0 };
    uint64_t m_tickWindow{ 0 };
    uint64_t m_deviceLatencyTicks{ 0 };
    uint64_t m_maxDeviceLatencyTicks{ 0 };
    uint64_t m_sendPathLatencyTicks{ 0 };

    uint64_t m_maxSpinTicks{ 0 };
    uint64_t m_timerOvershootTicks{ 0 };
    uint64_t m_minimumTimerWaitTicks{ 0 };

    // Due time of the message serviced most recently, used to size the next spin budget.
    internal::MidiTimestamp m_previousDueTime{ 0 };
};
