// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "AudioOutput.h"
#include "SynthCore.h"

#include <mutex>

namespace SoundFontSynth
{
    struct AudioEngineStatus
    {
        bool Running{ false };

        // Set while a synth has something to play and no device could be opened for it.
        bool Unavailable{ false };
        AudioOutputStatus LastOpenStatus{ AudioOutputStatus::Ok };

        AudioShareMode ShareMode{ AudioShareMode::Shared };
        std::wstring DeviceName;
        bool UsingFallbackDevice{ false };
        uint32_t SampleRate{ 0 };
        double PeriodMilliseconds{ 0.0 };
        uint16_t BitsPerSample{ 0 };
        bool IsFloat{ false };
        uint16_t ChannelCount{ 0 };
        uint64_t Glitches{ 0 };
    };

    // The one audio stream every synth plays through. Exclusive mode allows a single stream per
    // device, so the synths are mixed here rather than each opening their own.
    class AudioEngine final : private IAudioRenderCallback
    {
    public:
        AudioEngine();
        ~AudioEngine();

        AudioEngine(AudioEngine const&) = delete;
        AudioEngine& operator=(AudioEngine const&) = delete;

        // Any thread. Applied on the worker's next pass.
        void SetSettings(_In_ AudioOutputSettings const& settings);
        AudioOutputSettings Settings() const;

        // Any thread.
        AudioEngineStatus Status() const;

        // Worker only. Opens the device when a synth has something to play, lets it go after a
        // quiet spell, keeps the mix in step with the synths that exist, and passes MIDI through
        // while nothing renders.
        void Service(_In_ std::vector<std::shared_ptr<SynthCore>> const& cores) noexcept;

        // Worker only, last. Fades whatever still sounds and lets the device go.
        void Shutdown() noexcept;

        static constexpr uint32_t IdleReleaseMilliseconds = 5000;
        static constexpr uint32_t RetryIntervalMilliseconds = 1000;
        static constexpr uint32_t FadeTimeoutMilliseconds = 200;

    private:
        void RenderAudio(_Out_writes_(frameCount * 2) float* interleavedStereo, _In_ uint32_t frameCount) noexcept override;

        bool OpenAndStart(_In_ AudioOutputSettings const& settings, _In_ std::vector<std::shared_ptr<SynthCore>> const& cores) noexcept;
        void StopAndClose(_In_ bool fade) noexcept;
        bool UpdateRenderList(_In_ std::vector<std::shared_ptr<SynthCore>> const& cores) noexcept;
        void PublishStatus(_In_ bool unavailable, _In_ AudioOutputStatus lastOpenStatus) noexcept;

        AudioOutput m_output;
        int64_t m_qpcFrequency{ 1 };

        // The render thread only ever tries for this lock. Losing the race costs one silent block
        // while the worker swaps the list, never a wait on the audio thread.
        SRWLOCK m_renderLock = SRWLOCK_INIT;
        std::vector<SynthCore*> m_renderCores;

        // Worker only. Holds every synth in the mix alive until the render thread can no longer reach it.
        std::vector<std::shared_ptr<SynthCore>> m_heldCores;

        std::vector<float> m_scratch;
        float m_limiterGain{ 1.0f };

        int64_t m_nextOpenAttempt{ 0 };
        bool m_openFailing{ false };
        AudioOutputStatus m_lastOpenStatus{ AudioOutputStatus::Ok };

        mutable std::mutex m_lock;
        AudioOutputSettings m_settings;
        bool m_settingsChanged{ false };
        AudioEngineStatus m_status;
    };
}
