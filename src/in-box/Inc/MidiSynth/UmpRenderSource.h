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
        UmpRenderSource(
            _In_ SynthEngine& engine,
            _In_ UmpDispatcher& dispatcher,
            _In_ UmpInboundQueue& queue,
            _In_ uint32_t deviceSampleRate,
            _In_ uint32_t maxFrames,
            _In_opt_ HANDLE drainedEvent) noexcept;

        // Signals the drained event once every voice has finished, so a caller can fade out and
        // stop the stream without clipping the tail.
        void RequestDrain() noexcept { m_draining.store(true, std::memory_order_release); }

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

        QueuedUmp m_pending[MaxPendingUmpPerBlock]{};
        uint32_t m_pendingOffset[MaxPendingUmpPerBlock]{};

        double m_ratio{ 1.0 };
        double m_position{ 0.0 };
        uint32_t m_available{ 0 };
        std::vector<float> m_engineFrames;
    };
}
