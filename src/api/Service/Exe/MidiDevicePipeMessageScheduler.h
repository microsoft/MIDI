// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#include <queue>
#include <thread>


// TODO: This base is arbitrary. We need to calculate this based on testing
// it's also likely to be different on different PCs, so TBD if we need something
// more dynamic for this value. Initial value here is 5000, which on my dev PC comes
// out to 5 microseconds.
#define BASE_MESSAGE_SCHEDULER_TICK_WINDOW 5000


// we keep a copy of the data here because the original data
// pointer is not guaranteed to be valid by the time we get
// around to sending the message, especially if it's scheduled
// far into the future
struct ScheduledUmpMessage
{
    internal::MidiTimestamp Timestamp{ 0 };
    uint64_t ReceivedIndex{ 0 };            // this allows us to preserve order for messages with the same timestamp
    UINT ByteCount{ 0 };
    BYTE Data[MAXIMUM_UMP_DATASIZE];        // pre-define this array to avoid another allocation/indirection
};


class MidiDevicePipeMessageScheduler
{
public:
    MidiDevicePipeMessageScheduler() {}
    ~MidiDevicePipeMessageScheduler() { }

    HRESULT Initialize(
        _In_ wil::com_ptr_nothrow<CMidiDevicePipe>& devicePipe,
        _In_ DWORD* MmcssTaskId);

    HRESULT Cleanup();

    // incoming message from the Device Pipe Plugin Processor
    HRESULT ProcessIncomingMidiMessage(
        _In_ PVOID, 
        _In_ UINT, 
        _In_ LONGLONG);

    // We could call the device directly, but instead we always call back into the 
    // device pipe so there aren't two different paths for sending messages
    HRESULT SendMidiMessageNow(
        _In_ ScheduledUmpMessage const message);     // for queued messages

    // for messages that bypass the queue, or have had their time come up
    HRESULT SendMidiMessageNow(
        _In_ PVOID, 
        _In_ UINT, 
        _In_ LONGLONG);   

private:
    // priority queue with comparison set up to compare the timestamps
    // we want the smallest timestamp as front/top of the queue.

    // NOTE: As expected, this works for small-ish numbers of queued 
    // messages. But when you start getting up into the thousands, the
    // delay becomes larger. Example: just over 1ms for 10,000 messages.
    // Not really any other std:: options here, at least for the data
    // structure we need. It's already using a heap / tree underneath.
    // 
    // Consider a separate thread for writing to the queue to make that return quickly.
    std::priority_queue<ScheduledUmpMessage, std::vector<ScheduledUmpMessage>, auto(*)(ScheduledUmpMessage&, ScheduledUmpMessage&)->bool>
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
    uint64_t m_tickWindow{ BASE_MESSAGE_SCHEDULER_TICK_WINDOW };

    // TODO: we may allow device latency to be set for the device as a property value with key
    // PKEY_MIDI_MidiOutLatencyTicks. We'd then pre-fetch from the queue by this amount.
    uint64_t m_deviceLatencyTicks{ 0 };

    // pointer back to the device pipe that owns this scheduler
    wil::com_ptr_nothrow<CMidiDevicePipe> m_devicePipe;

    void QueueWorker();

    
    wil::critical_section m_queueLock;
    uint64_t m_currentReceivedIndex{ 0 };     // need to use the queueLock before writing to this

    bool m_continueProcessing{ true };

    std::thread m_queueWorkerThread;

    wil::unique_event_nothrow m_messageProcessorWakeup;

};
