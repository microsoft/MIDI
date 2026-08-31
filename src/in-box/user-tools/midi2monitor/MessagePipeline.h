// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageCapture.h"
#include "MessageStore.h"

namespace midi2monitor
{
    // Owns everything between the service callback and the UI:
    //
    //   service worker thread  -> MessageCapture::MessagesReceived (split, filter, stage)
    //   capture worker thread  -> Ingest (classify, index, delta, append to the store)
    //   UI thread              -> read-only snapshots and per row lookups
    //
    // The UI thread never parses, classifies or appends a message. It only takes the shared
    // lock briefly to copy the rows it is about to draw, so a burst of MIDI traffic can never
    // stall the window.
    class MessagePipeline
    {
    public:
        ~MessagePipeline();

        void Start() noexcept;
        void Stop() noexcept;

        // The SDK attaches this pointer without taking a reference, so the pipeline holds the
        // only one for as long as the callback is registered.
        IMidiEndpointConnectionMessagesReceivedCallback* CallbackInterface() const noexcept
        {
            return m_callbackInterface.get();
        }

        // Raised on the capture worker thread, already coalesced. The handler is responsible
        // for marshaling to the UI thread, and calls AcknowledgeContentChanged when it runs.
        void ContentChangedHandler(std::function<void()> handler) noexcept;
        void AcknowledgeContentChanged() noexcept;

        void CaptureEnabled(bool value) noexcept;
        bool CaptureEnabled() const noexcept { return m_capture->Enabled(); }

        // resets the zero point used for relative message timestamps
        void CaptureTimebase(uint64_t timestampTicks) noexcept
        {
            m_captureTimebase.store(timestampTicks, std::memory_order_relaxed);
        }

        void Filter(uint8_t groupNumber, uint8_t channelNumber) noexcept;

        void PostClear() noexcept;
        void PostNotice(winrt::hstring const& text) noexcept;
        void PostHiddenTraits(MessageTraits traits) noexcept;
        void PostCapacity(uint32_t capacity) noexcept;

        MessageStoreSnapshot Snapshot() const noexcept;
        bool TryGetVisibleRecord(size_t visibleIndex, MessageRecord& record) const noexcept;
        bool TryGetRecord(uint64_t sequence, MessageRecord& record) const noexcept;
        bool TrySetComment(uint64_t sequence, winrt::hstring const& comment) noexcept;

        // copies every retained record, including hidden ones, for export
        std::vector<MessageRecord> CopyRetainedRecords() const noexcept;

    private:
        enum class CommandKind : uint8_t
        {
            Clear,
            Notice,
            HiddenTraits,
            Capacity
        };

        struct Command
        {
            CommandKind Kind{ CommandKind::Clear };
            winrt::hstring Text{};
            uint32_t Value{ 0 };
        };

        void WorkerLoop() noexcept;
        void IngestStagedMessages() noexcept;
        void ProcessCommands() noexcept;
        void PostCommand(Command&& command) noexcept;
        void RaiseContentChanged() noexcept;

        static constexpr DWORD WorkerWaitMilliseconds = 25;
        static constexpr DWORD WorkerStopJoinMilliseconds = 2000;

        winrt::com_ptr<MessageCapture> m_capture{ winrt::make_self<MessageCapture>() };
        winrt::com_ptr<IMidiEndpointConnectionMessagesReceivedCallback> m_callbackInterface{ nullptr };

        mutable std::shared_mutex m_storeLock{};
        MessageStore m_store{};

        std::mutex m_commandLock{};
        std::vector<Command> m_commands{};

        std::mutex m_handlerLock{};
        std::function<void()> m_contentChangedHandler{};

        std::thread m_worker{};
        wil::unique_event m_wakeEvent{};
        std::atomic<bool> m_stopRequested{ false };
        std::atomic<bool> m_contentChangePending{ false };
        std::atomic<uint64_t> m_captureTimebase{ 0 };

        // worker-thread-only ingest state, so no lock is needed while building records
        std::vector<CapturedMessage> m_drainBuffer{};
        uint64_t m_lastTimestamp{ 0 };
        bool m_hasLastTimestamp{ false };
        uint32_t m_lastBatchId{ 0 };
        bool m_hasLastBatchId{ false };
        bool m_batchParity{ false };
    };
}
