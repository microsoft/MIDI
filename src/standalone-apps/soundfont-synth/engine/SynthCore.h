// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "Synthesizer.h"
#include "SynthDispatcher.h"
#include "SynthPropertySource.h"

#include "MidiSynth/SpscRingBuffer.h"

#include <mutex>
#include <string>

namespace SoundFontSynth
{
    struct QueuedUmp
    {
        uint32_t Words[4]{};
        uint8_t WordCount{ 0 };

        // QPC at arrival, so the render thread can place the message at its own sample offset
        // instead of collapsing a whole period onto the start of the block.
        int64_t Timestamp{ 0 };
    };

    // One complete System Exclusive message on its way out.
    struct OutboundSysEx
    {
        static constexpr size_t Capacity = 256;

        uint16_t Length{ 0 };
        uint8_t Bytes[Capacity]{};
    };

    // One synthesizer: the engine, its MIDI decoding, its MIDI-CI answers, and the queues between
    // the thread that receives MIDI, the thread that renders and the worker that sends replies.
    //
    // Threads:
    //   QueueInbound    any thread that receives MIDI. Serialized internally.
    //   RenderAudio     the audio thread, only while audio runs.
    //   PumpWithoutAudio, ServiceOutbound, the property and MUID calls
    //                   the worker, only while audio does not run.
    // Whoever owns the audio stream guarantees RenderAudio and PumpWithoutAudio never overlap.
    class SynthCore final : private ISysExSink
    {
    public:
        SynthCore(
            _In_ std::shared_ptr<SoundFont const> font,
            _In_ SynthIdentity const& identity,
            _In_ std::string const& productInstanceId,
            _In_ std::string const& modelName);

        ~SynthCore() = default;

        SynthCore(SynthCore const&) = delete;
        SynthCore& operator=(SynthCore const&) = delete;

        // Rebuilds the engine for a new device rate. Channel state survives.
        bool PrepareForSampleRate(_In_ uint32_t sampleRate);

        // Any thread. Splits the words into packets and queues them for whoever runs the dispatcher.
        void QueueInbound(_In_reads_(wordCount) uint32_t const* words, _In_ uint32_t wordCount, _In_ int64_t timestamp) noexcept;

        // Audio thread. Dispatches what arrived during the period that just ended, each message at
        // its own offset, and renders. blockEnd is the QPC the block is anchored to.
        void RenderAudio(
            _Out_writes_(frameCount * 2) float* interleavedStereo,
            _In_ uint32_t frameCount,
            _In_ int64_t blockEnd,
            _In_ int64_t qpcFrequency) noexcept;

        // Worker, while audio is stopped: answers everything that does not need sound. Stops at
        // the first channel voice message so the note that wants audio is still queued for it.
        // When no audio device can be had, discardNotes keeps everything else flowing and drops
        // only note ons, which would otherwise start voices nothing ever renders.
        void PumpWithoutAudio(_In_ bool discardNotes) noexcept;

        // Worker. Sends every reply the dispatcher queued, whole, through the wire.
        void ServiceOutbound(_In_ ISysExSink& wire) noexcept;

        // Worker. A withdrawn MIDI-CI identifier must be replaced or MIDI-CI goes silent for good.
        void ReplaceMuidIfNeeded() noexcept;

        // Set when a channel voice message arrives; cleared by the audio owner after a quiet spell.
        bool AudioWanted() const noexcept { return m_audioWanted.load(std::memory_order_acquire); }
        int64_t LastChannelVoiceTimestamp() const noexcept { return m_lastChannelVoice.load(std::memory_order_acquire); }
        void ClearAudioWanted(_In_ int64_t ifLastChannelVoiceWas) noexcept;

        uint32_t LastActiveVoiceCount() const noexcept { return m_lastActiveVoices.load(std::memory_order_acquire); }

        // Any thread. The next render fades every voice out, so the device can be let go without a click.
        void RequestSilence() noexcept { m_silenceRequested.store(true, std::memory_order_release); }

        void SetUserVolumeDb(_In_ double decibels) noexcept { m_synthesizer.SetUserVolumeDb(decibels); }
        double UserVolumeDb() const noexcept { return m_synthesizer.UserVolumeDb(); }

        Synthesizer& Engine() noexcept { return m_synthesizer; }
        Synthesizer const& Engine() const noexcept { return m_synthesizer; }
        SynthDispatcher& Dispatcher() noexcept { return m_dispatcher; }
        SynthPropertySource& Properties() noexcept { return m_properties; }
        SoundFont const& Font() const noexcept { return *m_font; }

        uint64_t DroppedInboundCount() const noexcept { return m_droppedInbound.load(std::memory_order_relaxed); }
        uint64_t DroppedOutboundCount() const noexcept { return m_droppedOutbound.load(std::memory_order_relaxed); }
        uint64_t ReceivedMessageCount() const noexcept { return m_receivedMessages.load(std::memory_order_relaxed); }

        static uint32_t CreateRandomMuid() noexcept;

        static constexpr uint32_t DefaultSampleRate = 48000;

    private:
        void SendSysEx(_In_reads_(count) uint8_t const* payload, _In_ size_t count) noexcept override;

        static constexpr uint32_t MaximumPendingPerBlock = 256;

        std::shared_ptr<SoundFont const> m_font;
        Synthesizer m_synthesizer;
        SynthDispatcher m_dispatcher;
        SynthPropertySource m_properties;

        MidiSynth::SpscRingBuffer<QueuedUmp, 8192> m_inbound;
        MidiSynth::SpscRingBuffer<OutboundSysEx, 64> m_outbound;
        std::mutex m_producerLock;

        QueuedUmp m_pending[MaximumPendingPerBlock]{};
        uint32_t m_pendingOffset[MaximumPendingPerBlock]{};

        std::atomic<bool> m_audioWanted{ false };
        std::atomic<bool> m_silenceRequested{ false };
        std::atomic<int64_t> m_lastChannelVoice{ 0 };
        std::atomic<uint32_t> m_lastActiveVoices{ 0 };

        std::atomic<uint64_t> m_droppedInbound{ 0 };
        std::atomic<uint64_t> m_droppedOutbound{ 0 };
        std::atomic<uint64_t> m_receivedMessages{ 0 };
    };
}
