#include "MidiSynth/SynthEngine.h"
#include "MidiSynth/DlsUnits.h"

#include <algorithm>
#include <cmath>

namespace MidiSynth
{
    namespace
    {
        constexpr uint16_t F_RGN_OPTION_SELFNONEXCLUSIVE = 0x0001;

        constexpr uint8_t ControllerBankSelectMsb = 0;
        constexpr uint8_t ControllerModulation = 1;
        constexpr uint8_t ControllerDataEntryMsb = 6;
        constexpr uint8_t ControllerVolume = 7;
        constexpr uint8_t ControllerPan = 10;
        constexpr uint8_t ControllerExpression = 11;
        constexpr uint8_t ControllerBankSelectLsb = 32;
        constexpr uint8_t ControllerSustainPedal = 64;
        constexpr uint8_t ControllerRpnLsb = 100;
        constexpr uint8_t ControllerRpnMsb = 101;
        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerResetAllControllers = 121;
        constexpr uint8_t ControllerAllNotesOff = 123;

        constexpr uint8_t DrumChannelIndex = 9;

        // Below this the voice is inaudible and the engine reclaims it.
        constexpr double VoiceCutoffDb = -95.0;

        // Long enough to avoid a step in the waveform, short enough that a retriggered note still
        // sounds retriggered rather than overlapped.
        constexpr double VoiceKillSeconds = 0.005;

        constexpr double LimiterThreshold = 0.95;

        // Time constant for the limiter to return to unity once the overload passes.
        constexpr double LimiterReleaseSeconds = 0.100;

        double SampleAt(
            _In_reads_(frameCount) const int16_t* samples,
            _In_ uint32_t frameCount,
            _In_ double phase,
            _In_ InterpolationQuality quality) noexcept
        {
            const auto index = static_cast<int64_t>(phase);

            if (index < 0 || static_cast<uint32_t>(index) >= frameCount)
            {
                return 0.0;
            }

            const auto i0 = static_cast<uint32_t>(index);

            if (quality == InterpolationQuality::None)
            {
                return samples[i0];
            }

            const double fraction = phase - static_cast<double>(index);
            const uint32_t i1 = (i0 + 1 < frameCount) ? i0 + 1 : i0;

            if (quality == InterpolationQuality::Linear)
            {
                return samples[i0] + (samples[i1] - samples[i0]) * fraction;
            }

            // Four point Hermite. Clamps at the edges rather than wrapping, because a wrap
            // would read the wrong side of a loop point.
            const uint32_t iMinus1 = (i0 > 0) ? i0 - 1 : i0;
            const uint32_t i2 = (i1 + 1 < frameCount) ? i1 + 1 : i1;

            const double yMinus1 = samples[iMinus1];
            const double y0 = samples[i0];
            const double y1 = samples[i1];
            const double y2 = samples[i2];

            const double c0 = y0;
            const double c1 = 0.5 * (y1 - yMinus1);
            const double c2 = yMinus1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
            const double c3 = 0.5 * (y2 - yMinus1) + 1.5 * (y0 - y1);

            return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
        }
    }

    _Use_decl_annotations_
    bool SynthEngine::Initialize(const DlsCollection* collection, const SynthConfig& config)
    {
        if (collection == nullptr || collection->Instruments().empty())
        {
            return false;
        }

        m_collection = collection;
        m_config = config;
        m_renderSampleRate = static_cast<double>(config.RenderSampleRate());

        if (m_renderSampleRate <= 0.0)
        {
            return false;
        }

        m_voices.assign((std::max)(1u, config.MaxVoices), SynthVoice{});

        SystemReset();

        return true;
    }

    void SynthEngine::SystemReset()
    {
        for (auto& voice : m_voices)
        {
            voice = SynthVoice{};
        }

        for (size_t channel = 0; channel < MidiChannelCount; channel++)
        {
            m_channels[channel] = SynthChannelState{};
            m_channels[channel].IsDrumChannel = (channel == DrumChannelIndex);
            ResolveInstrument(static_cast<uint8_t>(channel));
        }

        m_nextStartOrder = 1;
        m_stolenVoiceCount = 0;
        m_droppedNoteCount = 0;
        m_clippedSampleCount = 0;
        m_limiterGain = 1.0;
        m_lowestLimiterGain = 1.0;
        m_peakOutput = 0.0;
    }

    _Use_decl_annotations_
    void SynthEngine::ResolveInstrument(uint8_t channel) noexcept
    {
        auto& state = m_channels[channel];

        const uint32_t bankMsb = state.IsDrumChannel ? 0u : state.BankMsb;
        const uint32_t bankLsb = state.IsDrumChannel ? 0u : state.BankLsb;

        state.Instrument = m_collection->FindInstrument(bankMsb, bankLsb, state.Program, state.IsDrumChannel);

        // Fall back the way a GS device does: unknown variation drops to the capital tone.
        if (state.Instrument == nullptr && bankMsb != 0)
        {
            state.Instrument = m_collection->FindInstrument(0, bankLsb, state.Program, state.IsDrumChannel);
        }

        if (state.Instrument == nullptr && bankLsb != 0)
        {
            state.Instrument = m_collection->FindInstrument(0, 0, state.Program, state.IsDrumChannel);
        }

        if (state.Instrument == nullptr && state.IsDrumChannel)
        {
            state.Instrument = m_collection->FindInstrument(0, 0, 0, true);
        }
    }

    _Use_decl_annotations_
    const DlsRegion* SynthEngine::SelectRegion(
        const DlsInstrument& instrument,
        uint8_t note,
        uint8_t velocity7) const noexcept
    {
        for (const auto& region : instrument.Regions)
        {
            if (note >= region.KeyLow && note <= region.KeyHigh &&
                velocity7 >= region.VelocityLow && velocity7 <= region.VelocityHigh)
            {
                return &region;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    SynthVoice* SynthEngine::AllocateVoice(uint8_t channel) noexcept
    {
        for (auto& voice : m_voices)
        {
            if (!voice.Active)
            {
                return &voice;
            }
        }

        SynthVoice* candidate = nullptr;

        if (m_config.StealingPolicy == VoiceStealingPolicy::LowestChannelPriority)
        {
            // The in-box synth favors lower channel numbers, so a voice on a higher channel than
            // the incoming note is the first thing to go.
            for (auto& voice : m_voices)
            {
                if (voice.Channel < channel)
                {
                    continue;
                }

                if (candidate == nullptr ||
                    voice.Channel > candidate->Channel ||
                    (voice.Channel == candidate->Channel && voice.StartOrder < candidate->StartOrder))
                {
                    candidate = &voice;
                }
            }
        }
        else
        {
            for (auto& voice : m_voices)
            {
                if (candidate == nullptr)
                {
                    candidate = &voice;
                    continue;
                }

                const bool voiceReleasing = voice.Stage == EnvelopeStage::Release;
                const bool candidateReleasing = candidate->Stage == EnvelopeStage::Release;

                if (voiceReleasing != candidateReleasing)
                {
                    if (voiceReleasing)
                    {
                        candidate = &voice;
                    }

                    continue;
                }

                if (voice.EnvelopeLevelDb < candidate->EnvelopeLevelDb ||
                    (voice.EnvelopeLevelDb == candidate->EnvelopeLevelDb &&
                     voice.StartOrder < candidate->StartOrder))
                {
                    candidate = &voice;
                }
            }
        }

        if (candidate != nullptr)
        {
            m_stolenVoiceCount++;
            *candidate = SynthVoice{};
        }

        return candidate;
    }

    _Use_decl_annotations_
    void SynthEngine::StartVoice(
        SynthVoice& voice,
        const DlsRegion& region,
        const DlsWave& wave,
        const DlsInstrument& instrument,
        uint8_t channel,
        uint8_t note,
        uint16_t velocity) noexcept
    {
        const DlsWaveSample& waveSample = region.HasWaveSample ? region.WaveSample : wave.WaveSample;

        voice = SynthVoice{};

        voice.Active = true;
        voice.Channel = channel;
        voice.Note = note;
        voice.Velocity = velocity;
        voice.KeyGroup = region.KeyGroup;
        voice.SelfNonExclusive = (region.Options & F_RGN_OPTION_SELFNONEXCLUSIVE) != 0;
        voice.NoteHeld = true;
        voice.StartOrder = m_nextStartOrder++;

        voice.Samples = reinterpret_cast<const int16_t*>(wave.SampleData.data());
        voice.SampleFrameCount = wave.FrameCount();

        if (!waveSample.Loops.empty() && voice.SampleFrameCount > 0)
        {
            const auto& loop = waveSample.Loops.front();
            const uint64_t loopEnd = static_cast<uint64_t>(loop.LoopStart) + loop.LoopLength;

            if (loop.LoopLength > 0 && loopEnd <= voice.SampleFrameCount)
            {
                voice.Looping = true;
                voice.LoopStart = loop.LoopStart;
                voice.LoopEnd = static_cast<uint32_t>(loopEnd);
            }
        }

        voice.Articulation = ResolveArticulation(instrument.Connections, region.Connections);

        const double waveRate = (wave.SamplesPerSecond > 0)
            ? static_cast<double>(wave.SamplesPerSecond)
            : m_renderSampleRate;

        const double tuningCents =
            (static_cast<double>(note) - static_cast<double>(waveSample.UnityNote)) * 100.0 +
            static_cast<double>(waveSample.FineTune);

        voice.BasePitchRatio = (waveRate / m_renderSampleRate) * CentsToPitchRatio(tuningCents);

        // Velocity uses the full 16 bit range so MIDI 2.0 resolution survives to the envelope.
        const double normalizedVelocity = static_cast<double>(velocity) / 65535.0;

        voice.StaticGainDb =
            ConcaveTransformDb(normalizedVelocity) + RelativeGainToDb(waveSample.Attenuation);

        const auto& articulation = voice.Articulation;

        // Velocity and key number scale the envelope times, additively in time cents, which is
        // multiplicative in seconds.
        const double velocityAttackScale =
            TimeCentsToSeconds(static_cast<int32_t>(articulation.Eg1VelocityToAttackTimeCents * normalizedVelocity));
        const double keyDecayScale =
            TimeCentsToSeconds(static_cast<int32_t>(articulation.Eg1KeyToDecayTimeCents * (note / 128.0)));

        voice.AttackSeconds = articulation.Eg1AttackSeconds * ((velocityAttackScale > 0.0) ? velocityAttackScale : 1.0);
        voice.DecaySeconds = articulation.Eg1DecaySeconds * ((keyDecayScale > 0.0) ? keyDecayScale : 1.0);
        voice.ReleaseSeconds = articulation.Eg1ReleaseSeconds;

        voice.SustainDb = (articulation.Eg1SustainFraction <= 0.0)
            ? DlsSilenceDb
            : 20.0 * std::log10(articulation.Eg1SustainFraction);

        if (voice.AttackSeconds > 0.0)
        {
            voice.Stage = EnvelopeStage::Attack;
            voice.AttackProgress = 0.0;
            voice.EnvelopeLevelDb = DlsSilenceDb;
        }
        else
        {
            voice.Stage = EnvelopeStage::Decay;
            voice.AttackProgress = 1.0;
            voice.EnvelopeLevelDb = 0.0;
        }

        voice.LfoDelayRemainingSeconds = articulation.LfoStartDelaySeconds;
    }

    _Use_decl_annotations_
    void SynthEngine::NoteOn(uint8_t channel, uint8_t note, uint16_t velocity)
    {
        if (channel >= MidiChannelCount || note > 127 || m_collection == nullptr)
        {
            return;
        }

        // Velocity zero is NOT treated as a note off here. That is a MIDI 1.0 convention and
        // belongs to whatever decodes MIDI 1.0; in MIDI 2.0 zero is simply the lowest velocity.
        auto& state = m_channels[channel];

        if (state.Instrument == nullptr)
        {
            m_droppedNoteCount++;
            return;
        }

        const auto velocity7 = static_cast<uint8_t>(velocity >> 9);
        const DlsRegion* region = SelectRegion(*state.Instrument, note, velocity7);

        if (region == nullptr)
        {
            m_droppedNoteCount++;
            return;
        }

        // A second note on of the same note kills the first unless the region opts out. This is
        // the DLS Level 1 default, not a quirk of any particular implementation.
        for (auto& voice : m_voices)
        {
            if (voice.Active && voice.Channel == channel && voice.Note == note && !voice.SelfNonExclusive)
            {
                KillVoice(voice);
            }
        }

        // Key groups make drum regions mutually exclusive, which is how a closed hi-hat chokes
        // an open one.
        if (region->KeyGroup != 0)
        {
            for (auto& voice : m_voices)
            {
                if (voice.Active && voice.Channel == channel && voice.KeyGroup == region->KeyGroup)
                {
                    KillVoice(voice);
                }
            }
        }

        SynthVoice* voice = AllocateVoice(channel);

        if (voice == nullptr)
        {
            m_droppedNoteCount++;
            return;
        }

        const auto& wave = m_collection->Waves()[region->WaveIndex];

        if (wave.FrameCount() == 0)
        {
            m_droppedNoteCount++;
            return;
        }

        StartVoice(*voice, *region, wave, *state.Instrument, channel, note, velocity);
    }

    _Use_decl_annotations_
    void SynthEngine::ReleaseVoice(SynthVoice& voice) noexcept
    {
        if (voice.Stage == EnvelopeStage::Release)
        {
            return;
        }

        voice.Stage = EnvelopeStage::Release;

        if (voice.AttackProgress < 1.0)
        {
            voice.EnvelopeLevelDb = (voice.AttackProgress <= 0.0)
                ? DlsSilenceDb
                : 20.0 * std::log10(voice.AttackProgress);
        }
    }

    _Use_decl_annotations_
    void SynthEngine::KillVoice(SynthVoice& voice) noexcept
    {
        if (!voice.Active)
        {
            return;
        }

        ReleaseVoice(voice);

        voice.NoteHeld = false;
        voice.SustainHeld = false;
        voice.ReleaseSeconds = VoiceKillSeconds;

        // A killed voice must not win a later note off for the same key.
        voice.Note = 0xFF;
        voice.KeyGroup = 0;
    }

    _Use_decl_annotations_
    void SynthEngine::NoteOff(uint8_t channel, uint8_t note)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        const bool pedalDown = m_channels[channel].SustainPedal;

        for (auto& voice : m_voices)
        {
            if (!voice.Active || voice.Channel != channel || voice.Note != note || !voice.NoteHeld)
            {
                continue;
            }

            voice.NoteHeld = false;

            if (pedalDown)
            {
                voice.SustainHeld = true;
            }
            else
            {
                ReleaseVoice(voice);
            }
        }
    }

    _Use_decl_annotations_
    void SynthEngine::ControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        auto& state = m_channels[channel];
        const double normalized = static_cast<double>(value & 0x7F) / 127.0;

        switch (controller)
        {
        case ControllerBankSelectMsb:
            state.BankMsb = value & 0x7F;
            break;

        case ControllerBankSelectLsb:
            state.BankLsb = value & 0x7F;
            break;

        case ControllerModulation:
            state.Modulation = normalized;
            break;

        case ControllerVolume:
            state.Volume = normalized;
            break;

        case ControllerPan:
            // Kept in the original form so that value 64 is exactly centered.
            state.PanOffset = (static_cast<double>(value & 0x7F) - 64.0) / 127.0;
            break;

        case ControllerExpression:
            state.Expression = normalized;
            break;

        case ControllerSustainPedal:
        {
            const bool down = value >= 64;

            if (state.SustainPedal && !down)
            {
                for (auto& voice : m_voices)
                {
                    if (voice.Active && voice.Channel == channel && voice.SustainHeld)
                    {
                        voice.SustainHeld = false;
                        ReleaseVoice(voice);
                    }
                }
            }

            state.SustainPedal = down;
            break;
        }

        case ControllerRpnMsb:
            state.RpnMsb = value & 0x7F;
            break;

        case ControllerRpnLsb:
            state.RpnLsb = value & 0x7F;
            break;

        case ControllerDataEntryMsb:
            if (state.RpnMsb == 0 && state.RpnLsb == 0)
            {
                state.PitchBendRangeSemitones = value & 0x7F;
            }
            else if (state.RpnMsb == 0 && state.RpnLsb == 1)
            {
                state.FineTuneCents = (static_cast<double>(value & 0x7F) - 64.0) * 100.0 / 64.0;
            }
            else if (state.RpnMsb == 0 && state.RpnLsb == 2)
            {
                state.CoarseTuneSemitones = static_cast<double>(value & 0x7F) - 64.0;
            }
            break;

        case ControllerAllSoundOff:
            AllSoundOff(channel);
            break;

        case ControllerResetAllControllers:
            ResetAllControllers(channel);
            break;

        case ControllerAllNotesOff:
            AllNotesOff(channel);
            break;

        default:
            break;
        }
    }

    _Use_decl_annotations_
    void SynthEngine::ControlChange32(uint8_t channel, uint8_t controller, uint32_t value)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        auto& state = m_channels[channel];
        const double normalized = static_cast<double>(value) / 4294967295.0;

        switch (controller)
        {
        case ControllerModulation:
            state.Modulation = normalized;
            break;

        case ControllerVolume:
            state.Volume = normalized;
            break;

        case ControllerExpression:
            state.Expression = normalized;
            break;

        case ControllerPan:
            state.PanOffset = normalized - 0.5;
            break;

        default:
            // Anything without a wider meaning behaves as its 7 bit equivalent.
            ControlChange(channel, controller, static_cast<uint8_t>(value >> 25));
            break;
        }
    }

    _Use_decl_annotations_
    void SynthEngine::PitchBend32(uint8_t channel, uint32_t value)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        m_channels[channel].PitchBendNormalized = (static_cast<double>(value) / 4294967295.0) * 2.0 - 1.0;
    }

    _Use_decl_annotations_
    void SynthEngine::ProgramChangeWithBank(
        uint8_t channel,
        uint8_t bankMsb,
        uint8_t bankLsb,
        uint8_t program)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        auto& state = m_channels[channel];
        state.BankMsb = bankMsb & 0x7F;
        state.BankLsb = bankLsb & 0x7F;
        state.Program = program & 0x7F;

        ResolveInstrument(channel);
    }

    _Use_decl_annotations_
    void SynthEngine::ProgramChange(uint8_t channel, uint8_t program)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        m_channels[channel].Program = program & 0x7F;
        ResolveInstrument(channel);
    }

    _Use_decl_annotations_
    void SynthEngine::PitchBend(uint8_t channel, int32_t value)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        const int32_t clamped = (std::clamp)(value, 0, 16383);
        m_channels[channel].PitchBendNormalized = (static_cast<double>(clamped) - 8192.0) / 8192.0;
    }

    _Use_decl_annotations_
    void SynthEngine::AllSoundOff(uint8_t channel)
    {
        for (auto& voice : m_voices)
        {
            if (voice.Active && voice.Channel == channel)
            {
                KillVoice(voice);
            }
        }
    }

    _Use_decl_annotations_
    void SynthEngine::AllNotesOff(uint8_t channel)
    {
        const bool pedalDown = (channel < MidiChannelCount) && m_channels[channel].SustainPedal;

        for (auto& voice : m_voices)
        {
            if (!voice.Active || voice.Channel != channel || !voice.NoteHeld)
            {
                continue;
            }

            voice.NoteHeld = false;

            if (pedalDown)
            {
                voice.SustainHeld = true;
            }
            else
            {
                ReleaseVoice(voice);
            }
        }
    }

    _Use_decl_annotations_
    void SynthEngine::ResetAllControllers(uint8_t channel)
    {
        if (channel >= MidiChannelCount)
        {
            return;
        }

        auto& state = m_channels[channel];

        state.Volume = 100.0 / 127.0;
        state.Expression = 1.0;
        state.PanOffset = 0.0;
        state.Modulation = 0.0;
        state.SustainPedal = false;
        state.PitchBendNormalized = 0.0;
        state.PitchBendRangeSemitones = 2.0;
        state.RpnMsb = 0x7F;
        state.RpnLsb = 0x7F;
    }

    _Use_decl_annotations_
    void SynthEngine::UpdateVoiceControl(SynthVoice& voice, uint32_t frames) noexcept
    {
        const double blockSeconds = static_cast<double>(frames) / m_renderSampleRate;

        switch (voice.Stage)
        {
        case EnvelopeStage::Attack:
            voice.AttackProgress += (voice.AttackSeconds > 0.0) ? (blockSeconds / voice.AttackSeconds) : 1.0;

            if (voice.AttackProgress >= 1.0)
            {
                voice.AttackProgress = 1.0;
                voice.Stage = EnvelopeStage::Decay;
                voice.EnvelopeLevelDb = 0.0;
            }
            break;

        case EnvelopeStage::Decay:
            if (voice.DecaySeconds > 0.0)
            {
                // The specification defines decay as the time to fall the full 96 dB, so the
                // time actually spent reaching the sustain level scales with that rate.
                voice.EnvelopeLevelDb -= (96.0 / voice.DecaySeconds) * blockSeconds;
            }
            else
            {
                voice.EnvelopeLevelDb = voice.SustainDb;
            }

            if (voice.EnvelopeLevelDb <= voice.SustainDb)
            {
                voice.EnvelopeLevelDb = voice.SustainDb;
                voice.Stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Sustain:
            voice.EnvelopeLevelDb = voice.SustainDb;
            break;

        case EnvelopeStage::Release:
            if (voice.ReleaseSeconds > 0.0)
            {
                voice.EnvelopeLevelDb -= (96.0 / voice.ReleaseSeconds) * blockSeconds;
            }
            else
            {
                voice.EnvelopeLevelDb = DlsSilenceDb;
            }
            break;

        case EnvelopeStage::Idle:
        default:
            break;
        }

        if (voice.Stage != EnvelopeStage::Attack && voice.EnvelopeLevelDb <= VoiceCutoffDb)
        {
            voice.Active = false;
            return;
        }

        const auto& state = m_channels[voice.Channel];
        const auto& articulation = voice.Articulation;

        double envelopeGain = (voice.Stage == EnvelopeStage::Attack)
            ? voice.AttackProgress
            : DecibelsToLinear(voice.EnvelopeLevelDb);

        const double modulationDepth = state.Modulation;

        double lfoValue = 0.0;

        if (voice.LfoDelayRemainingSeconds > 0.0)
        {
            voice.LfoDelayRemainingSeconds -= blockSeconds;
        }
        else
        {
            lfoValue = std::sin(voice.LfoPhase * 2.0 * 3.14159265358979323846);
            voice.LfoPhase += articulation.LfoFrequencyHertz * blockSeconds;
            voice.LfoPhase -= std::floor(voice.LfoPhase);
        }

        const double lfoAttenuationDb =
            lfoValue * (articulation.LfoToAttenuationDb + articulation.LfoModWheelToAttenuationDb * modulationDepth);

        const double channelGainDb =
            ConcaveTransformDb(state.Volume) +
            ConcaveTransformDb(state.Expression);

        const double totalGainDb = voice.StaticGainDb + channelGainDb + lfoAttenuationDb + m_config.MasterGainDb;

        const double amplitude = envelopeGain * DecibelsToLinear(totalGainDb) / 32768.0;

        // Equal power pan, combining the region's placement with the channel's.
        const double pan = (std::clamp)(articulation.PanFraction + state.PanOffset, -0.5, 0.5) + 0.5;

        const double panAngle = pan * (3.14159265358979323846 / 2.0);

        const double targetLeft = amplitude * std::cos(panAngle);
        const double targetRight = amplitude * std::sin(panAngle);

        if (!voice.GainsPrimed)
        {
            voice.CurrentGainLeft = targetLeft;
            voice.CurrentGainRight = targetRight;
            voice.GainsPrimed = true;
        }

        voice.TargetGainLeft = targetLeft;
        voice.TargetGainRight = targetRight;

        // Pitch for this block.
        const double bendCents =
            state.PitchBendNormalized * state.PitchBendRangeSemitones * 100.0;

        const double lfoPitchCents =
            lfoValue * (articulation.LfoToPitchCents + articulation.LfoModWheelToPitchCents * modulationDepth);

        const double totalCents =
            bendCents + lfoPitchCents + state.FineTuneCents + state.CoarseTuneSemitones * 100.0;

        voice.BlockPitchRatio = voice.BasePitchRatio * CentsToPitchRatio(totalCents);
    }

    _Use_decl_annotations_
    void SynthEngine::RenderVoiceBlock(SynthVoice& voice, float* output, uint32_t frames) noexcept
    {
        const double gainStepLeft = (voice.TargetGainLeft - voice.CurrentGainLeft) / frames;
        const double gainStepRight = (voice.TargetGainRight - voice.CurrentGainRight) / frames;

        const auto quality = m_config.Interpolation;

        for (uint32_t frame = 0; frame < frames; frame++)
        {
            if (voice.Phase >= static_cast<double>(voice.SampleFrameCount))
            {
                voice.Active = false;
                break;
            }

            const double sample = SampleAt(voice.Samples, voice.SampleFrameCount, voice.Phase, quality);

            voice.CurrentGainLeft += gainStepLeft;
            voice.CurrentGainRight += gainStepRight;

            output[frame * 2] += static_cast<float>(sample * voice.CurrentGainLeft);
            output[frame * 2 + 1] += static_cast<float>(sample * voice.CurrentGainRight);

            voice.Phase += voice.BlockPitchRatio;

            if (voice.Looping && voice.Phase >= static_cast<double>(voice.LoopEnd))
            {
                const double loopLength = static_cast<double>(voice.LoopEnd) - static_cast<double>(voice.LoopStart);

                if (loopLength > 0.0)
                {
                    voice.Phase -= loopLength;
                }
            }
        }
    }

    _Use_decl_annotations_
    void SynthEngine::Render(float* interleavedStereo, uint32_t frameCount) noexcept
    {
        std::fill_n(interleavedStereo, static_cast<size_t>(frameCount) * 2, 0.0f);

        if (m_collection == nullptr)
        {
            return;
        }

        const uint32_t controlBlock = (std::max)(1u, m_config.ControlRateFrames);

        uint32_t rendered = 0;

        while (rendered < frameCount)
        {
            const uint32_t frames = (std::min)(controlBlock, frameCount - rendered);
            float* blockOutput = interleavedStereo + static_cast<size_t>(rendered) * 2;

            for (auto& voice : m_voices)
            {
                if (!voice.Active)
                {
                    continue;
                }

                UpdateVoiceControl(voice, frames);

                if (!voice.Active)
                {
                    continue;
                }

                RenderVoiceBlock(voice, blockOutput, frames);
            }

            rendered += frames;
        }

        // Instant attack so nothing overshoots, which means no lookahead and so no added latency.
        // The cost is that recovery is audible if it is too fast, hence the release smoothing.
        const double releaseCoefficient =
            1.0 - std::exp(-1.0 / (LimiterReleaseSeconds * m_renderSampleRate));

        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            double left = interleavedStereo[frame * 2];
            double right = interleavedStereo[frame * 2 + 1];

            if (m_config.EnableLimiter)
            {
                const double peak = (std::max)(std::abs(left), std::abs(right));

                if (peak * m_limiterGain > LimiterThreshold)
                {
                    m_limiterGain = LimiterThreshold / peak;

                    if (m_limiterGain < m_lowestLimiterGain)
                    {
                        m_lowestLimiterGain = m_limiterGain;
                    }
                }
                else
                {
                    m_limiterGain += (1.0 - m_limiterGain) * releaseCoefficient;
                }

                left *= m_limiterGain;
                right *= m_limiterGain;
            }

            if (left > 1.0 || left < -1.0 || right > 1.0 || right < -1.0)
            {
                m_clippedSampleCount++;
            }

            const double outputPeak = (std::max)(std::abs(left), std::abs(right));

            if (outputPeak > m_peakOutput)
            {
                m_peakOutput = outputPeak;
            }

            interleavedStereo[frame * 2] = static_cast<float>((std::clamp)(left, -1.0, 1.0));
            interleavedStereo[frame * 2 + 1] = static_cast<float>((std::clamp)(right, -1.0, 1.0));
        }
    }

    double SynthEngine::PeakGainReductionDb() const noexcept
    {
        return (m_lowestLimiterGain < 1.0) ? -20.0 * std::log10(m_lowestLimiterGain) : 0.0;
    }

    double SynthEngine::PeakOutputDbfs() const noexcept
    {
        return (m_peakOutput > 0.0) ? 20.0 * std::log10(m_peakOutput) : -200.0;
    }

    uint32_t SynthEngine::ActiveVoiceCount() const noexcept
    {
        uint32_t count = 0;

        for (const auto& voice : m_voices)
        {
            if (voice.Active)
            {
                count++;
            }
        }

        return count;
    }
}
