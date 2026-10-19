// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <vector>

#include "AudioSink.h"
#include "SpscRingBuffer.h"
#include "SynthEngine.h"
#include "UmpDispatcher.h"

namespace MidiSynth
{
    inline constexpr uint32_t MaxPendingUmpPerBlock = 256;

    struct QueuedUmp
    {
        uint32_t Words[4]{};
        uint8_t WordCount{ 0 };

        // QPC at arrival, so the render thread can place the event at the right sample offset
        // rather than collapsing a whole period of messages onto the block start.
        int64_t Timestamp{ 0 };
    };

    using UmpInboundQueue = SpscRingBuffer<QueuedUmp, 8192>;

    // Drives the engine from a queue of timestamped UMP, placing each message at its own offset
    // within the block rather than at the block boundary, and resampling when the engine runs at a
    // rate the device does not.
    class UmpRenderSource final : public IAudioRenderSource
    {
    public:
        // Allocates its working buffer, so it can throw std::bad_alloc.
        UmpRenderSource(
            _In_ SynthEngine& engine,
            _In_ UmpDispatcher& dispatcher,
            _In_ UmpInboundQueue& queue,
            _In_ uint32_t deviceSampleRate,
            _In_ uint32_t maxFrames,
            _In_opt_ HANDLE drainedEvent);

        // Signals the drained event once every voice has finished, so a caller can fade out and
        // stop the stream without clipping the tail.
        void RequestDrain() noexcept { m_draining.store(true, std::memory_order_release); }

        // Any thread. Only the audio thread may change the engine while it runs, so it makes these
        // changes itself, before its next block. Silence fades every voice out. A drum channel
        // request changes that channel only, and the latest request for a channel wins. Bank select
        // requests leave the engine as it would be had each been applied as it came.
        void RequestSilence() noexcept { m_silenceRequested.store(true, std::memory_order_release); }
        void RequestBankSelectMode(_In_ BankSelectMode mode) noexcept;
        void RequestDrumChannel(_In_ uint8_t channel, _In_ bool isDrumChannel) noexcept;

        // Makes the changes requested above. The audio thread calls this before every block. Any
        // other thread may call it only once the stream has stopped, to finish what the audio
        // thread was asked to do and didn't get to.
        void ApplyRequests() noexcept;

        // Voices sounding as of the last rendered block. Published here because the engine's own
        // count walks the voice array, which only the render thread may touch.
        uint32_t LastActiveVoiceCount() const noexcept
        {
            return m_lastActiveVoiceCount.load(std::memory_order_acquire);
        }

        void RenderAudio(
            _Out_writes_(frameCount * 2) float* interleavedStereo,
            _In_ uint32_t frameCount) noexcept override;

    private:
        void RenderChunk(
            _Out_writes_(frameCount * 2) float* interleavedStereo,
            _In_ uint32_t frameCount) noexcept;

        SynthEngine& m_engine;
        UmpDispatcher& m_dispatcher;
        UmpInboundQueue& m_queue;

        uint32_t m_deviceSampleRate{ 48000 };
        int64_t m_ticksPerSecond{ 1 };

        HANDLE m_drainedEvent{ nullptr };
        std::atomic<bool> m_draining{ false };

        std::atomic<bool> m_silenceRequested{ false };

        // Zero means none. Otherwise the mode, plus one.
        std::atomic<uint32_t> m_requestedBankSelectMode{ 0 };

        // The last mode other than Automatic asked for since the requests were last applied, the same
        // way. Choosing a convention also sets the one Automatic follows, so a choice followed at once
        // by Automatic still has to leave that convention behind.
        std::atomic<uint32_t> m_requestedExplicitBankSelectMode{ 0 };

        // The low sixteen bits say which channels change, the high sixteen what each becomes.
        std::atomic<uint32_t> m_requestedDrumChannels{ 0 };
        std::atomic<uint32_t> m_lastActiveVoiceCount{ 0 };

        QueuedUmp m_pending[MaxPendingUmpPerBlock]{};
        uint32_t m_pendingOffset[MaxPendingUmpPerBlock]{};

        double m_ratio{ 1.0 };
        double m_position{ 0.0 };
        uint32_t m_available{ 0 };
        std::vector<float> m_engineFrames;
    };
}
