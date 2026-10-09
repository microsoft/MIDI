// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "AudioEngine.h"

namespace SoundFontSynth
{
    namespace
    {
        int64_t QpcNow() noexcept
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);
            return now.QuadPart;
        }

        // Several synths at full tilt can sum past full scale. The limiter only ever pulls the mix
        // down, quickly, and lets go slowly so it does not pump.
        constexpr float LimiterCeiling = 0.98f;
        constexpr float LimiterReleasePerBlock = 0.02f;
    }

    AudioEngine::AudioEngine()
    {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        m_qpcFrequency = (frequency.QuadPart > 0) ? frequency.QuadPart : 1;

        m_scratch.assign(static_cast<size_t>(AudioOutput::MaximumCallbackFrames) * 2, 0.0f);
    }

    AudioEngine::~AudioEngine()
    {
        StopAndClose(false);
    }

    _Use_decl_annotations_
    void AudioEngine::SetSettings(AudioOutputSettings const& settings)
    {
        std::lock_guard<std::mutex> guard(m_lock);

        m_settings = settings;
        m_settingsChanged = true;
    }

    AudioOutputSettings AudioEngine::Settings() const
    {
        std::lock_guard<std::mutex> guard(m_lock);
        return m_settings;
    }

    AudioEngineStatus AudioEngine::Status() const
    {
        std::lock_guard<std::mutex> guard(m_lock);

        auto status = m_status;
        status.Glitches = m_output.GlitchCount();

        return status;
    }

    _Use_decl_annotations_
    void AudioEngine::PublishStatus(bool unavailable, AudioOutputStatus lastOpenStatus) noexcept
    {
        try
        {
            AudioEngineStatus status{};

            status.Running = m_output.IsRunning();
            status.Unavailable = unavailable;
            status.LastOpenStatus = lastOpenStatus;

            if (status.Running)
            {
                status.ShareMode = m_output.ShareMode();
                status.DeviceName = m_output.DeviceName();
                status.UsingFallbackDevice = m_output.UsingFallbackDevice();
                status.SampleRate = m_output.SampleRate();
                status.PeriodMilliseconds = m_output.PeriodMilliseconds();
                status.BitsPerSample = m_output.BitsPerSample();
                status.IsFloat = m_output.IsFloat();
                status.ChannelCount = m_output.ChannelCount();
            }

            std::lock_guard<std::mutex> guard(m_lock);
            m_status = std::move(status);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool AudioEngine::UpdateRenderList(std::vector<std::shared_ptr<SynthCore>> const& cores) noexcept
    {
        if (cores == m_heldCores)
        {
            return true;
        }

        try
        {
            // A synth joining a running mix has to be built for the device rate before the render
            // thread can see it.
            if (m_output.IsRunning())
            {
                for (auto const& core : cores)
                {
                    if (std::find(m_heldCores.begin(), m_heldCores.end(), core) == m_heldCores.end() &&
                        !core->PrepareForSampleRate(m_output.SampleRate()))
                    {
                        return false;
                    }
                }
            }

            std::vector<SynthCore*> renderCores;
            renderCores.reserve(cores.size());

            for (auto const& core : cores)
            {
                renderCores.push_back(core.get());
            }

            AcquireSRWLockExclusive(&m_renderLock);
            m_renderCores.swap(renderCores);
            ReleaseSRWLockExclusive(&m_renderLock);

            // Released only now, after the render thread has let go of the list that held them.
            m_heldCores = cores;

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool AudioEngine::OpenAndStart(AudioOutputSettings const& settings, std::vector<std::shared_ptr<SynthCore>> const& cores) noexcept
    {
        m_lastOpenStatus = m_output.Open(settings);

        if (m_lastOpenStatus != AudioOutputStatus::Ok)
        {
            return false;
        }

        // Nothing renders yet, so every synth can be rebuilt for the device rate in place.
        for (auto const& core : cores)
        {
            bool prepared{ false };

            try
            {
                prepared = core->PrepareForSampleRate(m_output.SampleRate());
            }
            catch (...)
            {
            }

            if (!prepared)
            {
                m_output.Close();
                m_lastOpenStatus = AudioOutputStatus::Failed;
                return false;
            }
        }

        // Cores already held were built for the previous rate and are rebuilt above, so the list
        // is replaced wholesale.
        m_heldCores.clear();

        if (!UpdateRenderList(cores) || !m_output.Start(this))
        {
            m_output.Close();
            m_lastOpenStatus = AudioOutputStatus::Failed;
            return false;
        }

        m_limiterGain = 1.0f;

        return true;
    }

    _Use_decl_annotations_
    void AudioEngine::StopAndClose(bool fade) noexcept
    {
        if (fade && m_output.IsRunning())
        {
            for (auto const& core : m_heldCores)
            {
                core->RequestSilence();
            }

            auto const deadline = GetTickCount64() + FadeTimeoutMilliseconds;

            while (GetTickCount64() < deadline)
            {
                bool quiet{ true };

                for (auto const& core : m_heldCores)
                {
                    quiet = quiet && core->LastActiveVoiceCount() == 0;
                }

                if (quiet)
                {
                    break;
                }

                Sleep(5);
            }
        }

        m_output.Close();

        AcquireSRWLockExclusive(&m_renderLock);
        m_renderCores.clear();
        ReleaseSRWLockExclusive(&m_renderLock);

        m_heldCores.clear();
    }

    _Use_decl_annotations_
    void AudioEngine::Service(std::vector<std::shared_ptr<SynthCore>> const& cores) noexcept
    {
        auto const now = QpcNow();
        auto const idleTicks = m_qpcFrequency * IdleReleaseMilliseconds / 1000;

        AudioOutputSettings settings{};
        bool settingsChanged{ false };

        try
        {
            std::lock_guard<std::mutex> guard(m_lock);

            settings = m_settings;
            settingsChanged = m_settingsChanged;
            m_settingsChanged = false;
        }
        catch (...)
        {
            return;
        }

        if (m_output.IsRunning())
        {
            if (settingsChanged || m_output.NeedsReopen())
            {
                // Whatever was playing is faded. The synths still want audio, so the device comes
                // straight back below with the new settings.
                StopAndClose(true);
                m_nextOpenAttempt = 0;
                m_openFailing = false;
            }
            else
            {
                (void)UpdateRenderList(cores);

                bool quiet{ true };

                for (auto const& core : m_heldCores)
                {
                    auto const last = core->LastChannelVoiceTimestamp();

                    quiet = quiet &&
                        core->LastActiveVoiceCount() == 0 &&
                        (!core->AudioWanted() || now - last >= idleTicks);
                }

                if (!quiet)
                {
                    return;
                }

                // Lets the device go once every voice has finished and nothing has been played for
                // a while. The wait is long enough that a reverb tail or a pause between phrases
                // never cuts a performance off.
                std::vector<std::pair<std::shared_ptr<SynthCore>, int64_t>> released;

                try
                {
                    for (auto const& core : m_heldCores)
                    {
                        released.emplace_back(core, core->LastChannelVoiceTimestamp());
                    }
                }
                catch (...)
                {
                    return;
                }

                StopAndClose(false);

                // A note that arrived while this was decided keeps its flag, and the next pass
                // opens the device again for it.
                for (auto const& [core, last] : released)
                {
                    core->ClearAudioWanted(last);
                }

                PublishStatus(false, m_lastOpenStatus);
                return;
            }
        }

        bool anyWanted{ false };

        for (auto const& core : cores)
        {
            anyWanted = anyWanted || core->AudioWanted();
        }

        if (anyWanted && now >= m_nextOpenAttempt)
        {
            if (OpenAndStart(settings, cores))
            {
                m_openFailing = false;
                PublishStatus(false, AudioOutputStatus::Ok);
                return;
            }

            m_openFailing = true;
            m_nextOpenAttempt = now + m_qpcFrequency * RetryIntervalMilliseconds / 1000;
            PublishStatus(true, m_lastOpenStatus);
        }
        else if (settingsChanged)
        {
            PublishStatus(m_openFailing && anyWanted, m_lastOpenStatus);
        }

        // Nothing renders, so the worker answers for every synth. With no device to be had, notes
        // are dropped so MIDI-CI and controller changes still get through.
        auto const discardNotes = m_openFailing && anyWanted;

        for (auto const& core : cores)
        {
            core->PumpWithoutAudio(discardNotes);

            // Stop retrying for a synth nobody has played for a while.
            if (discardNotes && core->AudioWanted())
            {
                auto const last = core->LastChannelVoiceTimestamp();

                if (now - last >= idleTicks)
                {
                    core->ClearAudioWanted(last);
                }
            }
        }
    }

    void AudioEngine::Shutdown() noexcept
    {
        StopAndClose(true);
        PublishStatus(false, m_lastOpenStatus);
    }

    _Use_decl_annotations_
    void AudioEngine::RenderAudio(float* interleavedStereo, uint32_t frameCount) noexcept
    {
        auto const samples = static_cast<size_t>(frameCount) * 2;

        std::fill_n(interleavedStereo, samples, 0.0f);

        if (frameCount == 0 || samples > m_scratch.size())
        {
            return;
        }

        if (!TryAcquireSRWLockShared(&m_renderLock))
        {
            return;
        }

        auto const blockEnd = QpcNow();

        for (auto* const core : m_renderCores)
        {
            core->RenderAudio(m_scratch.data(), frameCount, blockEnd, m_qpcFrequency);

            for (size_t i = 0; i < samples; i++)
            {
                interleavedStereo[i] += m_scratch[i];
            }
        }

        ReleaseSRWLockShared(&m_renderLock);

        float peak{ 0.0f };

        for (size_t i = 0; i < samples; i++)
        {
            peak = (std::max)(peak, std::fabs(interleavedStereo[i]));
        }

        auto const wanted = (peak > LimiterCeiling) ? LimiterCeiling / peak : 1.0f;
        auto const target = (wanted < m_limiterGain) ? wanted : (std::min)(wanted, m_limiterGain + LimiterReleasePerBlock);

        if (target == 1.0f && m_limiterGain == 1.0f)
        {
            return;
        }

        // Pulling down takes effect at once, so a peak at the start of the block is still caught.
        // Letting go ramps across the block.
        auto const start = (target < m_limiterGain) ? target : m_limiterGain;
        auto const step = (target - start) / static_cast<float>(frameCount);

        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            auto const gain = start + step * static_cast<float>(frame);

            interleavedStereo[static_cast<size_t>(frame) * 2] *= gain;
            interleavedStereo[static_cast<size_t>(frame) * 2 + 1] *= gain;
        }

        m_limiterGain = target;
    }
}
