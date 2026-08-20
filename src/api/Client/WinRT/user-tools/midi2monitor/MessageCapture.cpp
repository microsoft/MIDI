// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageCapture.h"
#include "MidiMessageDecoder.h"

namespace midi2monitor
{
    _Use_decl_annotations_
    void MessageCapture::SetFilter(uint8_t groupNumber, uint8_t channelNumber) noexcept
    {
        m_groupFilter.store(groupNumber, std::memory_order_relaxed);
        m_channelFilter.store(channelNumber, std::memory_order_relaxed);
    }

    _Use_decl_annotations_
    HRESULT MessageCapture::MessagesReceived(
        GUID sessionId,
        GUID connectionId,
        UINT64 timestamp,
        UINT32 wordCount,
        UINT32* messages)
    {
        UNREFERENCED_PARAMETER(sessionId);
        UNREFERENCED_PARAMETER(connectionId);

        if (messages == nullptr || wordCount == 0)
        {
            return S_OK;
        }

        if (!m_enabled.load(std::memory_order_relaxed))
        {
            return S_OK;
        }

        auto const groupFilter = m_groupFilter.load(std::memory_order_relaxed);
        auto const channelFilter = m_channelFilter.load(std::memory_order_relaxed);
        auto const batchId = m_batchId.fetch_add(1, std::memory_order_relaxed);

        // Written straight into the staging buffer. The only work done here is bit masking, and
        // the worker's Drain is an O(1) swap, so this never blocks the service thread for long.
        std::lock_guard<std::mutex> guard{ m_stagingLock };

        uint32_t position{ 0 };

        while (position < wordCount)
        {
            auto const word0 = messages[position];
            auto const messageWordCount = GetWordCountFromMessageType(GetMessageTypeFromFirstWord(word0));

            if (position + messageWordCount > wordCount)
            {
                // truncated trailing message. The SDK guarantees complete UMPs, so this only
                // happens if something upstream is malformed. Stop rather than read past the end.
                break;
            }

            auto const info = InspectMessage(word0);

            bool keep{ true };

            if (groupFilter != 0)
            {
                keep = info.HasGroup && (info.GroupIndex + 1) == groupFilter;

                if (keep && channelFilter != 0)
                {
                    keep = info.HasChannel && (info.ChannelIndex + 1) == channelFilter;
                }
            }

            if (keep)
            {
                if (m_staged.size() >= MaximumStagedMessages)
                {
                    // the UI is not draining fast enough. Drop rather than grow without bound,
                    // and report the loss so the customer knows the capture has a gap.
                    m_droppedCount.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    auto& captured = m_staged.emplace_back();

                    captured.Timestamp = timestamp;
                    captured.WordCount = messageWordCount;
                    captured.BatchId = batchId;

                    for (uint8_t i = 0; i < messageWordCount; i++)
                    {
                        captured.Words[i] = messages[position + i];
                    }
                }
            }

            position += messageWordCount;
        }

        return S_OK;
    }

    _Use_decl_annotations_
    void MessageCapture::Drain(std::vector<CapturedMessage>& target) noexcept
    {
        target.clear();

        std::lock_guard<std::mutex> guard{ m_stagingLock };

        if (m_staged.empty())
        {
            return;
        }

        target.swap(m_staged);
    }
}
