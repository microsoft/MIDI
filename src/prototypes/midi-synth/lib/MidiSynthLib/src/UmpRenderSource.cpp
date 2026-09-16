// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/UmpRenderSource.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace MidiSynth
{
    _Use_decl_annotations_
    UmpRenderSource::UmpRenderSource(
        SynthEngine& engine,
        UmpDispatcher& dispatcher,
        UmpInboundQueue& queue,
        uint32_t deviceSampleRate,
        uint32_t maxFrames,
        HANDLE drainedEvent) noexcept
        : m_engine(engine)
        , m_dispatcher(dispatcher)
        , m_queue(queue)
        , m_deviceSampleRate(deviceSampleRate)
        , m_drainedEvent(drainedEvent)
    {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        m_ticksPerSecond = frequency.QuadPart;

        m_ratio = static_cast<double>(engine.Config().RenderSampleRate())
            / static_cast<double>(deviceSampleRate);

        // Worst case engine frames for one device block, plus slack for interpolation and the
        // fractional carry. Allocated once so the render thread never does.
        const auto capacity = static_cast<size_t>(maxFrames * m_ratio) + 8;
        m_engineFrames.assign(capacity * 2, 0.0f);
    }

    _Use_decl_annotations_
    void UmpRenderSource::RenderAudio(float* interleavedStereo, uint32_t frameCount) noexcept
    {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);

        // Audio in this block represents the period that just elapsed, so a message that arrived
        // part way through it belongs part way through the block.
        const double blockTicks =
            static_cast<double>(frameCount) * static_cast<double>(m_ticksPerSecond)
            / static_cast<double>(m_deviceSampleRate);

        const double blockStartTicks = static_cast<double>(now.QuadPart) - blockTicks;

        uint32_t pendingCount = 0;
        QueuedUmp message;

        while (pendingCount < MaxPendingUmpPerBlock && m_queue.TryPop(message))
        {
            double offset = 0.0;

            if (blockTicks > 0.0)
            {
                offset = (static_cast<double>(message.Timestamp) - blockStartTicks)
                    / blockTicks * static_cast<double>(frameCount);
            }

            const double maxOffset = static_cast<double>(frameCount > 0 ? frameCount - 1 : 0);

            m_pending[pendingCount] = message;
            m_pendingOffset[pendingCount] =
                static_cast<uint32_t>((std::clamp)(offset, 0.0, maxOffset));

            pendingCount++;
        }

        uint32_t rendered = 0;
        uint32_t nextEvent = 0;

        while (rendered < frameCount)
        {
            while (nextEvent < pendingCount && m_pendingOffset[nextEvent] <= rendered)
            {
                (void)m_dispatcher.ProcessWords(
                    m_pending[nextEvent].Words, m_pending[nextEvent].WordCount);

                nextEvent++;
            }

            uint32_t limit = frameCount;

            if (nextEvent < pendingCount)
            {
                limit = (std::min)(limit, (std::max)(m_pendingOffset[nextEvent], rendered + 1));
            }

            const uint32_t frames = limit - rendered;

            RenderChunk(interleavedStereo + static_cast<size_t>(rendered) * 2, frames);
            rendered += frames;
        }

        if (m_draining.load(std::memory_order_acquire) && m_engine.ActiveVoiceCount() == 0)
        {
            if (m_drainedEvent != nullptr)
            {
                SetEvent(m_drainedEvent);
            }
        }
    }

    _Use_decl_annotations_
    void UmpRenderSource::RenderChunk(float* interleavedStereo, uint32_t frameCount) noexcept
    {
        // Compatible mode renders at 22050, so it has to be resampled up to the device rate.
        if (std::abs(m_ratio - 1.0) < 1e-9)
        {
            m_engine.Render(interleavedStereo, frameCount);
            return;
        }

        const double endPosition = m_position + frameCount * m_ratio;
        const auto needed = static_cast<uint32_t>(std::floor(endPosition)) + 2;

        if (static_cast<size_t>(needed) * 2 > m_engineFrames.size())
        {
            std::fill_n(interleavedStereo, static_cast<size_t>(frameCount) * 2, 0.0f);
            return;
        }

        if (needed > m_available)
        {
            m_engine.Render(m_engineFrames.data() + static_cast<size_t>(m_available) * 2,
                needed - m_available);

            m_available = needed;
        }

        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            const auto index = static_cast<uint32_t>(m_position);
            const auto fraction = static_cast<float>(m_position - index);

            const size_t a = static_cast<size_t>(index) * 2;
            const size_t b = a + 2;

            interleavedStereo[frame * 2] =
                m_engineFrames[a] + (m_engineFrames[b] - m_engineFrames[a]) * fraction;
            interleavedStereo[frame * 2 + 1] =
                m_engineFrames[a + 1] + (m_engineFrames[b + 1] - m_engineFrames[a + 1]) * fraction;

            m_position += m_ratio;
        }

        const auto consumed = static_cast<uint32_t>(m_position);

        if (consumed > 0 && consumed <= m_available)
        {
            const uint32_t remaining = m_available - consumed;

            std::memmove(m_engineFrames.data(),
                m_engineFrames.data() + static_cast<size_t>(consumed) * 2,
                static_cast<size_t>(remaining) * 2 * sizeof(float));

            m_available = remaining;
            m_position -= consumed;
        }
    }
}
