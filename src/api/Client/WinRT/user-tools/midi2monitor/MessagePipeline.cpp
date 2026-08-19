// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessagePipeline.h"
#include "MidiMessageDecoder.h"

namespace midi2monitor
{
    MessagePipeline::~MessagePipeline()
    {
        Stop();
    }

    void MessagePipeline::Start() noexcept
    {
        try
        {
            if (m_worker.joinable())
            {
                return;
            }

            m_callbackInterface = m_capture.as<IMidiEndpointConnectionMessagesReceivedCallback>();

            m_wakeEvent.create(wil::EventOptions::ManualReset);
            m_stopRequested.store(false, std::memory_order_relaxed);

            m_worker = std::thread([this]() { WorkerLoop(); });

            MIDI_MONITOR_LOG_INFO(L"Capture worker thread started.");
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to start the capture worker thread.")
    }

    void MessagePipeline::Stop() noexcept
    {
        try
        {
            m_capture->Enabled(false);
            m_stopRequested.store(true, std::memory_order_relaxed);

            if (m_wakeEvent)
            {
                m_wakeEvent.SetEvent();
            }

            if (m_worker.joinable())
            {
                // bounded join. The worker only ever waits on our own event, so this returns
                // promptly, but we never want app shutdown to hang on it.
                if (::WaitForSingleObject(m_worker.native_handle(), WorkerStopJoinMilliseconds) == WAIT_OBJECT_0)
                {
                    m_worker.join();
                }
                else
                {
                    LOG_HR_MSG(E_UNEXPECTED, "Capture worker did not exit in time. Detaching.");
                    m_worker.detach();
                }
            }

            ContentChangedHandler(nullptr);
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to stop the capture worker thread cleanly.")
    }

    _Use_decl_annotations_
    void MessagePipeline::ContentChangedHandler(std::function<void()> handler) noexcept
    {
        std::lock_guard<std::mutex> guard{ m_handlerLock };
        m_contentChangedHandler = std::move(handler);
    }

    _Use_decl_annotations_
    void MessagePipeline::CaptureEnabled(bool value) noexcept
    {
        m_capture->Enabled(value);

        if (value && m_wakeEvent)
        {
            m_wakeEvent.SetEvent();
        }
    }

    _Use_decl_annotations_
    void MessagePipeline::Filter(uint8_t groupNumber, uint8_t channelNumber) noexcept
    {
        m_capture->SetFilter(groupNumber, channelNumber);
    }

    void MessagePipeline::PostClear() noexcept
    {
        PostCommand(Command{ CommandKind::Clear, {}, 0 });
    }

    _Use_decl_annotations_
    void MessagePipeline::PostNotice(winrt::hstring const& text) noexcept
    {
        PostCommand(Command{ CommandKind::Notice, text, 0 });
    }

    _Use_decl_annotations_
    void MessagePipeline::PostHiddenTraits(MessageTraits traits) noexcept
    {
        PostCommand(Command{ CommandKind::HiddenTraits, {}, static_cast<uint32_t>(traits) });
    }

    _Use_decl_annotations_
    void MessagePipeline::PostCapacity(uint32_t capacity) noexcept
    {
        PostCommand(Command{ CommandKind::Capacity, {}, capacity });
    }

    _Use_decl_annotations_
    void MessagePipeline::PostCommand(Command&& command) noexcept
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard{ m_commandLock };
                m_commands.push_back(std::move(command));
            }

            if (m_wakeEvent)
            {
                m_wakeEvent.SetEvent();
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to queue a capture command.")
    }

    void MessagePipeline::AcknowledgeContentChanged() noexcept
    {
        m_contentChangePending.store(false, std::memory_order_release);
    }

    void MessagePipeline::RaiseContentChanged() noexcept
    {
        // Coalesce: while the UI has an unprocessed notification there is no value in queueing
        // another one, no matter how much MIDI arrives in the meantime.
        if (m_contentChangePending.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        std::function<void()> handler{};

        {
            std::lock_guard<std::mutex> guard{ m_handlerLock };
            handler = m_contentChangedHandler;
        }

        if (handler)
        {
            handler();
        }
        else
        {
            m_contentChangePending.store(false, std::memory_order_release);
        }
    }

    void MessagePipeline::WorkerLoop() noexcept
    {
        ::SetThreadDescription(::GetCurrentThread(), L"MIDI Monitor Capture");

        while (!m_stopRequested.load(std::memory_order_relaxed))
        {
            try
            {
                ::WaitForSingleObject(m_wakeEvent.get(), WorkerWaitMilliseconds);
                m_wakeEvent.ResetEvent();

                if (m_stopRequested.load(std::memory_order_relaxed))
                {
                    break;
                }

                IngestStagedMessages();
                ProcessCommands();
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Exception in the capture worker loop. Continuing.")
        }
    }

    void MessagePipeline::IngestStagedMessages() noexcept
    {
        m_capture->Drain(m_drainBuffer);

        auto const dropped = m_capture->TakeDroppedCount();

        if (m_drainBuffer.empty() && dropped == 0)
        {
            return;
        }

        // Everything expensive happens here, outside the lock and off the UI thread.
        std::vector<MessageRecord> records{};
        records.reserve(m_drainBuffer.size());

        auto const timebase = m_captureTimebase.load(std::memory_order_relaxed);

        for (auto const& captured : m_drainBuffer)
        {
            MessageRecord record{};

            record.Timestamp = captured.Timestamp;
            record.OffsetTicks = (captured.Timestamp > timebase) ? captured.Timestamp - timebase : 0;
            record.BatchId = captured.BatchId;
            record.Words = captured.Words;
            record.WordCount = captured.WordCount;
            record.Kind = RecordKind::MidiMessage;

            auto const info = InspectMessage(captured.Words[0]);

            if (!m_hasLastBatchId || captured.BatchId != m_lastBatchId)
            {
                m_batchParity = !m_batchParity;
                m_lastBatchId = captured.BatchId;
                m_hasLastBatchId = true;
            }

            record.AlternateBatch = m_batchParity;
            record.Traits = info.Traits;
            record.HasGroup = info.HasGroup;
            record.HasChannel = info.HasChannel;
            record.GroupNumber = info.HasGroup ? static_cast<uint8_t>(info.GroupIndex + 1) : uint8_t{ 0 };
            record.ChannelNumber = info.HasChannel ? static_cast<uint8_t>(info.ChannelIndex + 1) : uint8_t{ 0 };

            record.DeltaTicks = (m_hasLastTimestamp && captured.Timestamp > m_lastTimestamp)
                ? captured.Timestamp - m_lastTimestamp
                : 0;

            m_lastTimestamp = captured.Timestamp;
            m_hasLastTimestamp = true;

            records.push_back(std::move(record));
        }

        {
            std::unique_lock<std::shared_mutex> guard{ m_storeLock };

            if (dropped > 0)
            {
                m_store.AddDroppedMessages(dropped);
            }

            for (auto& record : records)
            {
                record.MessageIndex = m_store.NextMessageIndex();
                m_store.Append(std::move(record));
            }
        }

        RaiseContentChanged();
    }

    void MessagePipeline::ProcessCommands() noexcept
    {
        std::vector<Command> commands{};

        {
            std::lock_guard<std::mutex> guard{ m_commandLock };

            if (m_commands.empty())
            {
                return;
            }

            commands.swap(m_commands);
        }

        {
            std::unique_lock<std::shared_mutex> guard{ m_storeLock };

            for (auto& command : commands)
            {
                switch (command.Kind)
                {
                case CommandKind::Clear:
                    m_store.Clear();
                    m_hasLastTimestamp = false;
                    m_lastTimestamp = 0;
                    m_hasLastBatchId = false;
                    break;

                case CommandKind::Notice:
                {
                    MessageRecord record{};
                    record.Kind = RecordKind::Notice;
                    record.NoticeText = command.Text;
                    record.Timestamp = m_lastTimestamp;
                    m_store.Append(std::move(record));
                    break;
                }

                case CommandKind::HiddenTraits:
                    m_store.HiddenTraits(static_cast<MessageTraits>(static_cast<uint8_t>(command.Value)));
                    break;

                case CommandKind::Capacity:
                    m_store.Capacity(command.Value);
                    break;
                }
            }
        }

        RaiseContentChanged();
    }

    MessageStoreSnapshot MessagePipeline::Snapshot() const noexcept
    {
        std::shared_lock<std::shared_mutex> guard{ m_storeLock };
        return m_store.Snapshot();
    }

    _Use_decl_annotations_
    bool MessagePipeline::TryGetVisibleRecord(size_t visibleIndex, MessageRecord& record) const noexcept
    {
        std::shared_lock<std::shared_mutex> guard{ m_storeLock };
        return m_store.TryGetVisibleRecord(visibleIndex, record);
    }

    _Use_decl_annotations_
    bool MessagePipeline::TrySetComment(uint64_t sequence, winrt::hstring const& comment) noexcept
    {
        std::unique_lock<std::shared_mutex> guard{ m_storeLock };
        return m_store.TrySetComment(sequence, comment);
    }

    std::vector<MessageRecord> MessagePipeline::CopyRetainedRecords() const noexcept
    {
        std::vector<MessageRecord> records{};

        try
        {
            std::shared_lock<std::shared_mutex> guard{ m_storeLock };

            records.reserve(m_store.RetainedCount());

            for (size_t i = 0; i < m_store.RetainedCount(); i++)
            {
                records.push_back(m_store.RetainedAt(i));
            }
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to copy the retained messages for export.")

        return records;
    }
}
