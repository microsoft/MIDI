// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "SynthCore.h"

#include <random>

namespace SoundFontSynth
{
    namespace
    {
        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;
    }

    _Use_decl_annotations_
    SynthCore::SynthCore(
        std::shared_ptr<SoundFont const> font,
        SynthIdentity const& identity,
        std::string const& productInstanceId,
        std::string const& modelName) :
        m_font(std::move(font))
    {
        if (m_font == nullptr)
        {
            throw std::invalid_argument("A synthesizer needs a SoundFont.");
        }

        // Initialized at once, so MIDI-CI can describe the bank before any audio device is open.
        SynthesizerConfig config{};
        config.SampleRate = DefaultSampleRate;

        if (!m_synthesizer.Initialize(m_font, config))
        {
            throw std::runtime_error("The synthesizer could not be initialized.");
        }

        m_properties.Build(*m_font, identity, modelName.c_str());

        m_dispatcher.Initialize(&m_synthesizer, SynthEndpointShape::FirstGroupIndex, CreateRandomMuid());
        m_dispatcher.SetOutput(this, identity);
        m_dispatcher.SetProductInstanceId(productInstanceId.c_str());
    }

    uint32_t SynthCore::CreateRandomMuid() noexcept
    {
        try
        {
            // The whole range below the reserved block, so neither a reserved value nor the
            // broadcast value can come out.
            std::random_device generator;
            std::uniform_int_distribution<uint32_t> distribution(
                0, WindowsMidiServicesCapabilityInquiry::MuidReservedStart - 1);

            return distribution(generator);
        }
        catch (...)
        {
            // MIDI-CI stays silent rather than answering with an identifier we did not draw properly.
            return 0;
        }
    }

    _Use_decl_annotations_
    bool SynthCore::PrepareForSampleRate(uint32_t sampleRate)
    {
        if (m_synthesizer.SampleRate() == sampleRate && m_synthesizer.IsInitialized())
        {
            return true;
        }

        auto const volume = m_synthesizer.UserVolumeDb();

        SynthesizerConfig config{};
        config.SampleRate = sampleRate;

        if (!m_synthesizer.Initialize(m_font, config))
        {
            return false;
        }

        m_synthesizer.SetUserVolumeDb(volume);

        return true;
    }

    _Use_decl_annotations_
    void SynthCore::QueueInbound(uint32_t const* words, uint32_t wordCount, int64_t timestamp) noexcept
    {
        if (words == nullptr)
        {
            return;
        }

        std::lock_guard<std::mutex> guard(m_producerLock);

        for (uint32_t index = 0; index < wordCount; )
        {
            auto const length = SynthDispatcher::PacketWordCount(words[index]);

            // A length that does not fit means the rest cannot be trusted.
            if (length == 0 || length > 4 || index + length > wordCount)
            {
                m_droppedInbound.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            QueuedUmp queued{};
            queued.WordCount = static_cast<uint8_t>(length);
            queued.Timestamp = timestamp;

            for (uint32_t word = 0; word < length; word++)
            {
                queued.Words[word] = words[index + word];
            }

            auto const messageType = (queued.Words[0] >> 28) & 0xF;

            // Only a channel voice message has to be heard. Flagged before the push, so the worker
            // can never drain a note that arrived before the audio device was asked for.
            if (messageType == MessageTypeMidi1ChannelVoice || messageType == MessageTypeMidi2ChannelVoice)
            {
                m_lastChannelVoice.store(timestamp, std::memory_order_release);
                m_audioWanted.store(true, std::memory_order_release);
            }

            if (m_inbound.TryPush(queued))
            {
                m_receivedMessages.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                m_droppedInbound.fetch_add(1, std::memory_order_relaxed);
            }

            index += length;
        }
    }

    _Use_decl_annotations_
    void SynthCore::ClearAudioWanted(int64_t ifLastChannelVoiceWas) noexcept
    {
        // A note that arrived while the owner decided to let audio go keeps the flag set.
        if (m_lastChannelVoice.load(std::memory_order_acquire) == ifLastChannelVoiceWas)
        {
            m_audioWanted.store(false, std::memory_order_release);
        }
    }

    _Use_decl_annotations_
    void SynthCore::RenderAudio(float* interleavedStereo, uint32_t frameCount, int64_t blockEnd, int64_t qpcFrequency) noexcept
    {
        if (m_silenceRequested.exchange(false, std::memory_order_acq_rel))
        {
            for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
            {
                m_synthesizer.AllSoundOff(channel);
            }
        }

        auto const sampleRate = static_cast<double>(m_synthesizer.SampleRate());

        // The block stands for the period that just elapsed, so a message that arrived part way
        // through it belongs part way through the block.
        auto const blockTicks = (sampleRate > 0.0 && qpcFrequency > 0)
            ? static_cast<double>(frameCount) * static_cast<double>(qpcFrequency) / sampleRate
            : 0.0;

        auto const blockStart = static_cast<double>(blockEnd) - blockTicks;
        auto const lastFrame = static_cast<double>(frameCount > 0 ? frameCount - 1 : 0);

        uint32_t pendingCount = 0;
        QueuedUmp message{};

        while (pendingCount < MaximumPendingPerBlock && m_inbound.TryPop(message))
        {
            double offset = 0.0;

            if (blockTicks > 0.0)
            {
                offset = (static_cast<double>(message.Timestamp) - blockStart) / blockTicks * static_cast<double>(frameCount);
            }

            m_pending[pendingCount] = message;
            m_pendingOffset[pendingCount] = static_cast<uint32_t>((std::clamp)(offset, 0.0, lastFrame));

            // Arrival order is preserved even if two timestamps disagree with it.
            if (pendingCount > 0 && m_pendingOffset[pendingCount] < m_pendingOffset[pendingCount - 1])
            {
                m_pendingOffset[pendingCount] = m_pendingOffset[pendingCount - 1];
            }

            pendingCount++;
        }

        uint32_t rendered = 0;
        uint32_t nextEvent = 0;

        while (rendered < frameCount)
        {
            while (nextEvent < pendingCount && m_pendingOffset[nextEvent] <= rendered)
            {
                (void)m_dispatcher.ProcessWords(m_pending[nextEvent].Words, m_pending[nextEvent].WordCount);
                nextEvent++;
            }

            auto limit = frameCount;

            if (nextEvent < pendingCount)
            {
                limit = (std::min)(limit, (std::max)(m_pendingOffset[nextEvent], rendered + 1));
            }

            auto const frames = limit - rendered;

            m_synthesizer.Render(interleavedStereo + static_cast<size_t>(rendered) * 2, frames);

            rendered += frames;
        }

        while (nextEvent < pendingCount)
        {
            (void)m_dispatcher.ProcessWords(m_pending[nextEvent].Words, m_pending[nextEvent].WordCount);
            nextEvent++;
        }

        m_lastActiveVoices.store(m_synthesizer.ActiveVoiceCount(), std::memory_order_release);
    }

    _Use_decl_annotations_
    void SynthCore::PumpWithoutAudio(bool discardNotes) noexcept
    {
        QueuedUmp message{};

        while ((discardNotes || !m_audioWanted.load(std::memory_order_acquire)) && m_inbound.TryPop(message))
        {
            auto const messageType = (message.Words[0] >> 28) & 0xF;
            auto const status = (message.Words[0] >> 20) & 0xF;

            if (discardNotes && status == 0x9 &&
                (messageType == MessageTypeMidi1ChannelVoice || messageType == MessageTypeMidi2ChannelVoice))
            {
                continue;
            }

            (void)m_dispatcher.ProcessWords(message.Words, message.WordCount);
        }
    }

    _Use_decl_annotations_
    void SynthCore::SendSysEx(uint8_t const* payload, size_t count) noexcept
    {
        if (payload == nullptr || count == 0 || count > OutboundSysEx::Capacity)
        {
            m_droppedOutbound.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        OutboundSysEx message{};
        message.Length = static_cast<uint16_t>(count);
        memcpy(message.Bytes, payload, count);

        if (!m_outbound.TryPush(message))
        {
            m_droppedOutbound.fetch_add(1, std::memory_order_relaxed);
        }
    }

    _Use_decl_annotations_
    void SynthCore::ServiceOutbound(ISysExSink& wire) noexcept
    {
        OutboundSysEx message{};

        while (m_outbound.TryPop(message))
        {
            wire.SendSysEx(message.Bytes, message.Length);
        }
    }

    void SynthCore::ReplaceMuidIfNeeded() noexcept
    {
        if (!m_dispatcher.MuidNeedsReplacement())
        {
            return;
        }

        auto const replacement = CreateRandomMuid();

        // Zero means the draw failed; the flag stays set and the next pass tries again.
        if (replacement != 0)
        {
            m_dispatcher.SetMuid(replacement);
        }
    }
}
