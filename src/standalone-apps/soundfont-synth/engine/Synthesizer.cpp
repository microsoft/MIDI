// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "Synthesizer.h"
#include "Modulation.h"

#include "MidiSynth/ChorusEffect.h"
#include "MidiSynth/ReverbEffect.h"

namespace SoundFontSynth
{
    namespace
    {
        // Envelopes, LFOs, pitch and the filter are updated this often. Gains are ramped across
        // each block so nothing steps audibly.
        constexpr uint32_t ControlBlockFrames = 32;

        // A hostile bank could layer hundreds of zones on one key. Each one is a voice.
        constexpr uint32_t MaximumVoicesPerNoteOn = 32;

        // Extra voices kept back so a stolen voice can fade instead of being cut off mid waveform.
        constexpr uint32_t ReserveVoices = 32;

        constexpr uint32_t MaximumRenderFrames = 4096;
        constexpr uint32_t MaximumVoiceModulators = 64;

        constexpr double SilenceDb = -100.0;
        constexpr double KillSeconds = 0.005;

        // Headroom for chords. Measured against the in-box General MIDI synth, -6 dB plays a
        // General MIDI bank at about the same loudness; the mixer's limiter catches the rest.
        constexpr double EngineGainDb = -6.0;

        // SoundFont banks are voiced on Creative's hardware, which applies a bank's own initial
        // attenuation at 40 percent of its face value. Taken at full value, FluidR3's piano plays
        // 8 dB quieter than it was voiced. The controller modulators keep their full range, which
        // is what gives CC7, CC11 and velocity the standard 40 log10 MIDI curve.
        constexpr double BankAttenuationScale = 0.4;

        constexpr uint8_t DrumChannelIndex = 9;

        constexpr uint8_t ControllerBankSelectMsb = 0;
        constexpr uint8_t ControllerDataEntryMsb = 6;
        constexpr uint8_t ControllerVolume = 7;
        constexpr uint8_t ControllerPan = 10;
        constexpr uint8_t ControllerExpression = 11;
        constexpr uint8_t ControllerBankSelectLsb = 32;
        constexpr uint8_t ControllerDataEntryLsb = 38;
        constexpr uint8_t ControllerSustain = 64;
        constexpr uint8_t ControllerSostenuto = 66;
        constexpr uint8_t ControllerReverbSend = 91;
        constexpr uint8_t ControllerChorusSend = 93;
        constexpr uint8_t ControllerDataIncrement = 96;
        constexpr uint8_t ControllerDataDecrement = 97;
        constexpr uint8_t ControllerNrpnLsb = 98;
        constexpr uint8_t ControllerNrpnMsb = 99;
        constexpr uint8_t ControllerRpnLsb = 100;
        constexpr uint8_t ControllerRpnMsb = 101;
        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerResetAllControllers = 121;
        constexpr uint8_t ControllerAllNotesOff = 123;

        constexpr double Pi = 3.14159265358979323846;

        double TimecentsToSeconds(_In_ double timecents) noexcept
        {
            // The specification's floor is one millisecond, and -32768 means "instant".
            if (timecents <= -12000.0)
            {
                return 0.001;
            }

            return std::exp2((std::min)(timecents, 8000.0) / 1200.0);
        }

        double AbsoluteCentsToHertz(_In_ double cents) noexcept
        {
            return 8.176 * std::exp2(cents / 1200.0);
        }

        double DecibelsToAmplitude(_In_ double decibels) noexcept
        {
            return std::pow(10.0, decibels / 20.0);
        }

        // GM2 and MMA RP-036: volume is the square of the controller, which is 40 log10.
        double ConcaveDb(_In_ double normalized) noexcept
        {
            return (normalized <= 0.0) ? SilenceDb : (std::max)(SilenceDb, 40.0 * std::log10(normalized));
        }
    }

    enum class EnvelopeStage : uint8_t
    {
        Delay,
        Attack,
        Hold,
        Decay,
        Sustain,
        Release,
        Finished,
    };

    // Attack is linear in amplitude; decay and release are linear in decibels, a hundred of them
    // per stage time, as SoundFont 2.04 section 8.1.3 describes.
    struct VolumeEnvelope
    {
        EnvelopeStage Stage{ EnvelopeStage::Delay };
        double StageTime{ 0.0 };

        double DelaySeconds{ 0.0 };
        double AttackSeconds{ 0.0 };
        double HoldSeconds{ 0.0 };
        double DecaySeconds{ 0.0 };
        double ReleaseSeconds{ 0.0 };
        double SustainDb{ 0.0 };

        double AttackLevel{ 0.0 };
        double LevelDb{ SilenceDb };

        double Amplitude() const noexcept
        {
            switch (Stage)
            {
            case EnvelopeStage::Delay: return 0.0;
            case EnvelopeStage::Attack: return AttackLevel;
            case EnvelopeStage::Hold: return 1.0;
            case EnvelopeStage::Finished: return 0.0;
            default: return DecibelsToAmplitude(LevelDb);
            }
        }

        void Advance(_In_ double seconds) noexcept
        {
            while (seconds > 0.0)
            {
                switch (Stage)
                {
                case EnvelopeStage::Delay:
                {
                    auto const remaining = DelaySeconds - StageTime;

                    if (seconds < remaining)
                    {
                        StageTime += seconds;
                        return;
                    }

                    seconds -= (std::max)(remaining, 0.0);
                    Stage = EnvelopeStage::Attack;
                    StageTime = 0.0;
                    AttackLevel = 0.0;
                    break;
                }

                case EnvelopeStage::Attack:
                {
                    auto const time = StageTime + seconds;

                    if (AttackSeconds > 0.0 && time < AttackSeconds)
                    {
                        StageTime = time;
                        AttackLevel = time / AttackSeconds;
                        return;
                    }

                    seconds = (AttackSeconds > 0.0) ? time - AttackSeconds : seconds;
                    Stage = EnvelopeStage::Hold;
                    StageTime = 0.0;
                    AttackLevel = 1.0;
                    LevelDb = 0.0;
                    break;
                }

                case EnvelopeStage::Hold:
                {
                    auto const remaining = HoldSeconds - StageTime;

                    if (seconds < remaining)
                    {
                        StageTime += seconds;
                        return;
                    }

                    seconds -= (std::max)(remaining, 0.0);
                    Stage = EnvelopeStage::Decay;
                    StageTime = 0.0;
                    LevelDb = 0.0;
                    break;
                }

                case EnvelopeStage::Decay:
                {
                    auto const rate = 100.0 / (std::max)(DecaySeconds, 0.001);
                    auto const timeToSustain = (LevelDb - SustainDb) / rate;

                    if (seconds < timeToSustain)
                    {
                        LevelDb -= rate * seconds;
                        return;
                    }

                    seconds -= (std::max)(timeToSustain, 0.0);
                    LevelDb = SustainDb;
                    Stage = (SustainDb <= SilenceDb) ? EnvelopeStage::Finished : EnvelopeStage::Sustain;
                    break;
                }

                case EnvelopeStage::Release:
                {
                    auto const rate = 100.0 / (std::max)(ReleaseSeconds, 0.001);

                    LevelDb -= rate * seconds;

                    if (LevelDb <= SilenceDb)
                    {
                        LevelDb = SilenceDb;
                        Stage = EnvelopeStage::Finished;
                    }

                    return;
                }

                default:
                    return;
                }
            }
        }

        void Release() noexcept
        {
            switch (Stage)
            {
            case EnvelopeStage::Finished:
                return;

            case EnvelopeStage::Delay:
                LevelDb = SilenceDb;
                break;

            case EnvelopeStage::Attack:
                LevelDb = (AttackLevel > 0.00001) ? 20.0 * std::log10(AttackLevel) : SilenceDb;
                break;

            case EnvelopeStage::Hold:
                LevelDb = 0.0;
                break;

            default:
                break;
            }

            Stage = EnvelopeStage::Release;
            StageTime = 0.0;
        }
    };

    // Values run 0 to 1. Every stage is a straight line, with decay and release covering the
    // full range in their stage time.
    struct ModulationEnvelope
    {
        EnvelopeStage Stage{ EnvelopeStage::Delay };
        double StageTime{ 0.0 };

        double DelaySeconds{ 0.0 };
        double AttackSeconds{ 0.0 };
        double HoldSeconds{ 0.0 };
        double DecaySeconds{ 0.0 };
        double ReleaseSeconds{ 0.0 };
        double SustainLevel{ 1.0 };

        double Level{ 0.0 };

        void Advance(_In_ double seconds) noexcept
        {
            while (seconds > 0.0)
            {
                switch (Stage)
                {
                case EnvelopeStage::Delay:
                {
                    auto const remaining = DelaySeconds - StageTime;

                    if (seconds < remaining)
                    {
                        StageTime += seconds;
                        return;
                    }

                    seconds -= (std::max)(remaining, 0.0);
                    Stage = EnvelopeStage::Attack;
                    StageTime = 0.0;
                    break;
                }

                case EnvelopeStage::Attack:
                {
                    auto const time = StageTime + seconds;

                    if (AttackSeconds > 0.0 && time < AttackSeconds)
                    {
                        StageTime = time;
                        Level = time / AttackSeconds;
                        return;
                    }

                    seconds = (AttackSeconds > 0.0) ? time - AttackSeconds : seconds;
                    Stage = EnvelopeStage::Hold;
                    StageTime = 0.0;
                    Level = 1.0;
                    break;
                }

                case EnvelopeStage::Hold:
                {
                    auto const remaining = HoldSeconds - StageTime;

                    if (seconds < remaining)
                    {
                        StageTime += seconds;
                        return;
                    }

                    seconds -= (std::max)(remaining, 0.0);
                    Stage = EnvelopeStage::Decay;
                    StageTime = 0.0;
                    break;
                }

                case EnvelopeStage::Decay:
                {
                    auto const rate = 1.0 / (std::max)(DecaySeconds, 0.001);
                    auto const timeToSustain = (Level - SustainLevel) / rate;

                    if (seconds < timeToSustain)
                    {
                        Level -= rate * seconds;
                        return;
                    }

                    seconds -= (std::max)(timeToSustain, 0.0);
                    Level = SustainLevel;
                    Stage = EnvelopeStage::Sustain;
                    break;
                }

                case EnvelopeStage::Release:
                {
                    auto const rate = 1.0 / (std::max)(ReleaseSeconds, 0.001);

                    Level -= rate * seconds;

                    if (Level <= 0.0)
                    {
                        Level = 0.0;
                        Stage = EnvelopeStage::Finished;
                    }

                    return;
                }

                default:
                    return;
                }
            }
        }

        void Release() noexcept
        {
            if (Stage != EnvelopeStage::Finished)
            {
                Stage = EnvelopeStage::Release;
                StageTime = 0.0;
            }
        }
    };

    // Triangle, starting at zero and rising, once its delay has passed.
    struct Lfo
    {
        double DelaySeconds{ 0.0 };
        double FrequencyHz{ 8.176 };
        double Elapsed{ 0.0 };
        double Phase{ 0.0 };

        void Advance(_In_ double seconds) noexcept
        {
            if (Elapsed < DelaySeconds)
            {
                Elapsed += seconds;

                if (Elapsed <= DelaySeconds)
                {
                    return;
                }

                seconds = Elapsed - DelaySeconds;
            }

            Phase += FrequencyHz * seconds;
            Phase -= std::floor(Phase);
        }

        double Value() const noexcept
        {
            if (Elapsed < DelaySeconds)
            {
                return 0.0;
            }

            if (Phase < 0.25)
            {
                return 4.0 * Phase;
            }

            if (Phase < 0.75)
            {
                return 2.0 - 4.0 * Phase;
            }

            return 4.0 * Phase - 4.0;
        }
    };

    // Two pole resonant low pass, transposed direct form II.
    struct LowPassFilter
    {
        float B0{ 1.0f };
        float B1{ 0.0f };
        float B2{ 0.0f };
        float A1{ 0.0f };
        float A2{ 0.0f };
        float Z1{ 0.0f };
        float Z2{ 0.0f };

        double AppliedHz{ -1.0 };
        double AppliedQ{ -1.0 };

        void Configure(_In_ double cutoffHz, _In_ double q, _In_ double sampleRate) noexcept
        {
            cutoffHz = (std::clamp)(cutoffHz, 5.0, sampleRate * 0.45);
            q = (std::clamp)(q, 0.5, 100.0);

            if (std::abs(cutoffHz - AppliedHz) < 0.5 && std::abs(q - AppliedQ) < 0.001)
            {
                return;
            }

            AppliedHz = cutoffHz;
            AppliedQ = q;

            auto const omega = 2.0 * Pi * cutoffHz / sampleRate;
            auto const cosine = std::cos(omega);
            auto const alpha = std::sin(omega) / (2.0 * q);
            auto const a0 = 1.0 + alpha;

            B0 = static_cast<float>((1.0 - cosine) / 2.0 / a0);
            B1 = static_cast<float>((1.0 - cosine) / a0);
            B2 = B0;
            A1 = static_cast<float>(-2.0 * cosine / a0);
            A2 = static_cast<float>((1.0 - alpha) / a0);
        }

        float Process(_In_ float input) noexcept
        {
            auto const output = B0 * input + Z1;

            Z1 = B1 * input - A1 * output + Z2;
            Z2 = B2 * input - A2 * output;

            return output;
        }
    };

    struct Voice
    {
        bool Active{ false };
        bool Killing{ false };

        uint8_t Channel{ 0 };
        uint8_t Note{ 0 };
        uint32_t NoteOnId{ 0 };
        uint64_t StartOrder{ 0 };

        bool KeyHeld{ false };
        bool SustainHeld{ false };
        bool SostenutoHeld{ false };
        bool Detached{ false };
        int32_t ExclusiveClass{ 0 };

        double VelocityNormalized{ 0.0 };
        uint8_t ScalingKey{ 60 };
        double PitchNote{ 60.0 };

        int16_t const* Data{ nullptr };
        uint8_t const* Data24{ nullptr };
        int64_t Start{ 0 };
        int64_t End{ 0 };
        int64_t LoopStart{ 0 };
        int64_t LoopEnd{ 0 };
        uint8_t LoopMode{ 0 };
        bool Looping{ false };
        double Phase{ 0.0 };
        double RateRatio{ 1.0 };
        double RootKey{ 60.0 };
        double ScaleTuning{ 100.0 };
        double PitchCorrectionCents{ 0.0 };

        std::array<int32_t, Gen::Count> Generators{};
        std::array<double, Gen::Count> Modulation{};
        std::array<Sf2Modulator, MaximumVoiceModulators> Modulators{};
        uint32_t ModulatorCount{ 0 };

        double PitchCents{ 0.0 };
        double AttenuationDb{ 0.0 };
        float PanLeft{ 0.70710678f };
        float PanRight{ 0.70710678f };
        float ReverbSend{ 0.0f };
        float ChorusSend{ 0.0f };
        double ModLfoToPitch{ 0.0 };
        double VibLfoToPitch{ 0.0 };
        double ModEnvToPitch{ 0.0 };
        double ModLfoToFilter{ 0.0 };
        double ModEnvToFilter{ 0.0 };
        double ModLfoToVolumeDb{ 0.0 };
        double FilterCutoffCents{ 13500.0 };
        double FilterQ{ 0.70710678 };

        VolumeEnvelope VolEnv{};
        ModulationEnvelope ModEnv{};
        Lfo ModLfo{};
        Lfo VibLfo{};
        LowPassFilter Filter{};

        float CurrentGain{ 0.0f };
        float KillGain{ 1.0f };

        double PerNoteBendCents{ 0.0 };
        double PerNoteTuningCents{ 0.0 };
        double PerNoteGainDb{ 0.0 };
        double PerNotePan{ 0.0 };

        bool IsReleased() const noexcept
        {
            return VolEnv.Stage == EnvelopeStage::Release || VolEnv.Stage == EnvelopeStage::Finished;
        }
    };

    struct ChannelState
    {
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t Program{ 0 };

        // Set by configuration or by the GS rhythm part message. A bank select can also choose a
        // kit under XG and GM2; EffectiveDrum covers both.
        bool IsDrumChannel{ false };
        bool EffectiveDrum{ false };
        int32_t PresetIndex{ -1 };

        std::array<double, 128> Controllers{};
        std::array<uint8_t, 128> RawControllers{};

        double PitchWheel{ 0.5 };
        double PitchBendRangeSemitones{ 2.0 };
        double FineTuneCents{ 0.0 };
        double CoarseTuneSemitones{ 0.0 };
        double ChannelPressure{ 0.0 };
        std::array<double, 128> PolyPressure{};

        bool NrpnSelected{ false };
        bool SustainPedal{ false };
        bool SostenutoPedal{ false };
    };

    struct Synthesizer::Impl
    {
        std::shared_ptr<SoundFont const> Font{};
        SynthesizerConfig Config{};
        double SampleRate{ 48000.0 };
        bool Initialized{ false };

        std::vector<Voice> Voices{};
        std::array<ChannelState, MidiChannelCount> Channels{};
        std::array<std::atomic<uint64_t>, MidiChannelCount> PublishedSelection{};

        uint64_t NextStartOrder{ 1 };
        uint32_t NextNoteOnId{ 1 };

        BankSelectMode DetectedBankMode{ BankSelectMode::RolandGS };

        double MasterVolumeDb{ 0.0 };
        double MasterFineCents{ 0.0 };
        double MasterCoarseCents{ 0.0 };
        std::atomic<double> UserVolumeDb{ 0.0 };

        MidiSynth::ReverbEffect Reverb{};
        MidiSynth::ChorusEffect Chorus{};
        std::vector<float> ReverbSendBuffer{};
        std::vector<float> ChorusSendBuffer{};

        bool ActiveSensingSeen{ false };
        double SecondsSinceActiveSensing{ 0.0 };

        std::atomic<uint64_t> DroppedNotes{ 0 };
        std::atomic<uint64_t> StolenVoices{ 0 };
        std::atomic<uint32_t> PeakVoices{ 0 };
        std::atomic<uint32_t> ActiveVoices{ 0 };

        Impl() noexcept
        {
            ResetChannels();
        }

        void ResetChannels() noexcept
        {
            for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
            {
                auto& state = Channels[channel];

                state = ChannelState{};
                state.IsDrumChannel = (channel == DrumChannelIndex);

                SetControllerRaw(state, ControllerVolume, 100);
                SetControllerRaw(state, ControllerPan, 64);
                SetControllerRaw(state, ControllerExpression, 127);
                SetControllerRaw(state, ControllerReverbSend, 40);
                SetControllerRaw(state, ControllerRpnLsb, 127);
                SetControllerRaw(state, ControllerRpnMsb, 127);
                SetControllerRaw(state, ControllerNrpnLsb, 127);
                SetControllerRaw(state, ControllerNrpnMsb, 127);

                ResolvePreset(channel);
            }
        }

        static void SetControllerRaw(_Inout_ ChannelState& state, _In_ uint8_t controller, _In_ uint8_t value) noexcept
        {
            state.RawControllers[controller & 0x7F] = value & 0x7F;
            state.Controllers[controller & 0x7F] = NormalizeCentered7(value);
        }

        void Publish(_In_ uint8_t channel) noexcept
        {
            auto const& state = Channels[channel];

            uint64_t packed = static_cast<uint64_t>(state.BankMsb) |
                (static_cast<uint64_t>(state.BankLsb) << 8) |
                (static_cast<uint64_t>(state.Program) << 16) |
                (static_cast<uint64_t>(state.EffectiveDrum ? 1 : 0) << 24) |
                (static_cast<uint64_t>(static_cast<uint32_t>(state.PresetIndex)) << 32);

            PublishedSelection[channel].store(packed, std::memory_order_release);
        }

        // Turns the bank select controllers into a SoundFont bank number for this addressing.
        uint16_t ResolveBank(_In_ ChannelState const& state, _Out_ bool& isDrum) const noexcept
        {
            isDrum = state.IsDrumChannel;

            if (isDrum)
            {
                return SoundFont::PercussionBank;
            }

            switch (DetectedBankMode)
            {
            case BankSelectMode::YamahaXG:
                if (state.BankMsb == 127 || state.BankMsb == 126)
                {
                    isDrum = true;
                    return SoundFont::PercussionBank;
                }

                return (state.BankMsb == 0) ? state.BankLsb : state.BankMsb;

            case BankSelectMode::GeneralMidi2:
                if (state.BankMsb == 120)
                {
                    isDrum = true;
                    return SoundFont::PercussionBank;
                }

                return (state.BankMsb == 121) ? state.BankLsb : state.BankMsb;

            default:
                return state.BankMsb;
            }
        }

        void ResolvePreset(_In_ uint8_t channel) noexcept
        {
            auto& state = Channels[channel];

            bool isDrum = false;
            auto const bank = ResolveBank(state, isDrum);

            state.EffectiveDrum = isDrum;
            state.PresetIndex = -1;

            if (Font != nullptr)
            {
                auto const& font = *Font;

                auto index = font.FindPreset(bank, state.Program);

                // The General MIDI way: a missing variation falls back to the capital tone, and a
                // missing kit to the standard kit.
                if (index < 0)
                {
                    index = isDrum
                        ? font.FindPreset(SoundFont::PercussionBank, 0)
                        : font.FindPreset(0, state.Program);
                }

                if (index < 0)
                {
                    for (size_t i = 0; i < font.Presets().size(); i++)
                    {
                        auto const& preset = font.Presets()[i];

                        if ((preset.Bank == SoundFont::PercussionBank) == isDrum)
                        {
                            index = static_cast<int32_t>(i);
                            break;
                        }
                    }
                }

                state.PresetIndex = index;
            }

            Publish(channel);
        }

        // ------------------------------------------------------------------------- voices

        uint32_t CountActiveVoices() const noexcept
        {
            uint32_t count = 0;

            for (auto const& voice : Voices)
            {
                if (voice.Active)
                {
                    count++;
                }
            }

            return count;
        }

        void KillVoice(_Inout_ Voice& voice) noexcept
        {
            if (voice.Active && !voice.Killing)
            {
                voice.Killing = true;
                voice.KillGain = 1.0f;
            }
        }

        void ReleaseVoice(_Inout_ Voice& voice) noexcept
        {
            voice.KeyHeld = false;
            voice.SustainHeld = false;
            voice.SostenutoHeld = false;

            voice.VolEnv.Release();
            voice.ModEnv.Release();

            // Loop mode 3 loops only while the key is down, then plays the rest of the sample.
            if (voice.LoopMode == 3)
            {
                voice.Looping = false;
            }
        }

        _Ret_maybenull_ Voice* AllocateVoice() noexcept
        {
            uint32_t sounding = 0;

            for (auto const& voice : Voices)
            {
                if (voice.Active && !voice.Killing)
                {
                    sounding++;
                }
            }

            // Over the limit: fade out the least important voice. Released voices go before held
            // ones, and the oldest goes first.
            if (sounding >= Config.Polyphony)
            {
                Voice* victim = nullptr;

                for (auto& voice : Voices)
                {
                    if (!voice.Active || voice.Killing)
                    {
                        continue;
                    }

                    if (victim == nullptr ||
                        (voice.IsReleased() && !victim->IsReleased()) ||
                        (voice.IsReleased() == victim->IsReleased() && voice.StartOrder < victim->StartOrder))
                    {
                        victim = &voice;
                    }
                }

                if (victim != nullptr)
                {
                    KillVoice(*victim);
                    StolenVoices.fetch_add(1, std::memory_order_relaxed);
                }
            }

            for (auto& voice : Voices)
            {
                if (!voice.Active)
                {
                    return &voice;
                }
            }

            // Every slot, reserve included, is busy. Cut the quietest fading voice outright.
            Voice* fallback = nullptr;

            for (auto& voice : Voices)
            {
                if (fallback == nullptr || voice.StartOrder < fallback->StartOrder)
                {
                    fallback = &voice;
                }
            }

            if (fallback != nullptr)
            {
                StolenVoices.fetch_add(1, std::memory_order_relaxed);
            }

            return fallback;
        }

        ModulationInputs InputsFor(_In_ Voice const& voice) const noexcept
        {
            auto const& state = Channels[voice.Channel];

            ModulationInputs inputs{};

            inputs.Controllers = state.Controllers.data();
            inputs.Velocity = voice.VelocityNormalized;
            inputs.Key = static_cast<double>(voice.ScalingKey) / 127.0;
            inputs.PolyPressure = state.PolyPressure[voice.Note & 0x7F];
            inputs.ChannelPressure = state.ChannelPressure;
            inputs.PitchWheel = state.PitchWheel;
            inputs.PitchWheelSensitivity = (std::clamp)(state.PitchBendRangeSemitones / 127.0, 0.0, 1.0);

            return inputs;
        }

        double Effective(_In_ Voice const& voice, _In_ uint16_t generator) const noexcept
        {
            return static_cast<double>(voice.Generators[generator]) + voice.Modulation[generator];
        }

        // Everything a controller can change while a note sounds.
        void RefreshDynamic(_Inout_ Voice& voice) noexcept
        {
            auto const& state = Channels[voice.Channel];

            auto pitch = voice.ScaleTuning * (voice.PitchNote - voice.RootKey);

            pitch += Effective(voice, Gen::CoarseTune) * 100.0;
            pitch += Effective(voice, Gen::FineTune);
            pitch += voice.PitchCorrectionCents;
            pitch += voice.Modulation[Gen::InitialPitch];
            pitch += state.FineTuneCents + state.CoarseTuneSemitones * 100.0;
            pitch += MasterFineCents + MasterCoarseCents;
            pitch += voice.PerNoteTuningCents + voice.PerNoteBendCents;

            voice.PitchCents = pitch;

            voice.AttenuationDb = (std::clamp)(
                static_cast<double>(voice.Generators[Gen::InitialAttenuation]) * BankAttenuationScale +
                    voice.Modulation[Gen::InitialAttenuation],
                0.0, 1440.0) / 10.0;

            auto const pan = (std::clamp)(Effective(voice, Gen::Pan) + voice.PerNotePan, -500.0, 500.0);
            auto const angle = (pan + 500.0) / 1000.0 * (Pi / 2.0);

            voice.PanLeft = static_cast<float>(std::cos(angle));
            voice.PanRight = static_cast<float>(std::sin(angle));

            voice.ReverbSend = static_cast<float>((std::clamp)(Effective(voice, Gen::ReverbEffectsSend) / 1000.0, 0.0, 1.0));
            voice.ChorusSend = static_cast<float>((std::clamp)(Effective(voice, Gen::ChorusEffectsSend) / 1000.0, 0.0, 1.0));

            voice.ModLfoToPitch = (std::clamp)(Effective(voice, Gen::ModLfoToPitch), -12000.0, 12000.0);
            voice.VibLfoToPitch = (std::clamp)(Effective(voice, Gen::VibLfoToPitch), -12000.0, 12000.0);
            voice.ModEnvToPitch = (std::clamp)(Effective(voice, Gen::ModEnvToPitch), -12000.0, 12000.0);
            voice.ModLfoToFilter = (std::clamp)(Effective(voice, Gen::ModLfoToFilterFc), -12000.0, 12000.0);
            voice.ModEnvToFilter = (std::clamp)(Effective(voice, Gen::ModEnvToFilterFc), -12000.0, 12000.0);
            voice.ModLfoToVolumeDb = (std::clamp)(Effective(voice, Gen::ModLfoToVolume), -960.0, 960.0) / 10.0;

            voice.FilterCutoffCents = Effective(voice, Gen::InitialFilterFc);

            // Zero resonance is a flat response, and every 20 centibels doubles the peak.
            voice.FilterQ = 0.70710678 * std::pow(10.0, (std::clamp)(Effective(voice, Gen::InitialFilterQ), 0.0, 960.0) / 200.0);

            voice.ModLfo.FrequencyHz = AbsoluteCentsToHertz((std::clamp)(Effective(voice, Gen::FreqModLfo), -16000.0, 4500.0));
            voice.VibLfo.FrequencyHz = AbsoluteCentsToHertz((std::clamp)(Effective(voice, Gen::FreqVibLfo), -16000.0, 4500.0));
        }

        void RefreshModulation(_Inout_ Voice& voice) noexcept
        {
            auto const inputs = InputsFor(voice);

            voice.Modulation.fill(0.0);

            for (uint32_t i = 0; i < voice.ModulatorCount; i++)
            {
                auto const& modulator = voice.Modulators[i];

                if (modulator.Destination < Gen::Count)
                {
                    voice.Modulation[modulator.Destination] += EvaluateModulator(modulator, inputs);
                }
            }

            RefreshDynamic(voice);
        }

        void RefreshChannel(_In_ uint8_t channel) noexcept
        {
            for (auto& voice : Voices)
            {
                if (voice.Active && voice.Channel == channel)
                {
                    RefreshModulation(voice);
                }
            }
        }

        static void AddModulator(
            _Inout_ Voice& voice,
            _In_ Sf2Modulator const& modulator,
            _In_ bool replaceSame) noexcept
        {
            if (replaceSame)
            {
                for (uint32_t i = 0; i < voice.ModulatorCount; i++)
                {
                    if (voice.Modulators[i].IsSameAs(modulator))
                    {
                        voice.Modulators[i] = modulator;
                        return;
                    }
                }
            }

            if (voice.ModulatorCount < MaximumVoiceModulators)
            {
                voice.Modulators[voice.ModulatorCount++] = modulator;
            }
        }

        bool StartVoice(
            _In_ uint8_t channel,
            _In_ uint8_t note,
            _In_ uint16_t velocity,
            _In_ double pitchNote,
            _In_ Sf2Preset const& preset,
            _In_ Sf2Zone const& presetZone,
            _In_ Sf2Instrument const& instrument,
            _In_ Sf2Zone const& instrumentZone,
            _In_ uint32_t noteOnId) noexcept
        {
            auto const& font = *Font;
            auto const& sample = font.Samples()[instrumentZone.Link];

            Voice* slot = AllocateVoice();

            if (slot == nullptr)
            {
                return false;
            }

            auto& voice = *slot;

            voice = Voice{};

            // Instrument generators replace each other, global then local. Preset generators are
            // added on top, global then local.
            for (uint16_t generator = 0; generator < Gen::Count; generator++)
            {
                int32_t value = GeneratorDefault(generator);

                if (instrument.HasGlobalZone && instrument.GlobalZone.IsSet(generator))
                {
                    value = instrument.GlobalZone.Values[generator];
                }

                if (instrumentZone.IsSet(generator))
                {
                    value = instrumentZone.Values[generator];
                }

                if (GeneratorAllowedAtPresetLevel(generator))
                {
                    if (presetZone.IsSet(generator))
                    {
                        value += presetZone.Values[generator];
                    }
                    else if (preset.HasGlobalZone && preset.GlobalZone.IsSet(generator))
                    {
                        value += preset.GlobalZone.Values[generator];
                    }
                }

                voice.Generators[generator] = value;
            }

            // Instrument modulators replace the defaults and each other; preset modulators add to them.
            for (auto const& modulator : DefaultModulators())
            {
                AddModulator(voice, modulator, false);
            }

            if (instrument.HasGlobalZone)
            {
                for (auto const& modulator : instrument.GlobalZone.Modulators)
                {
                    AddModulator(voice, modulator, true);
                }
            }

            for (auto const& modulator : instrumentZone.Modulators)
            {
                AddModulator(voice, modulator, true);
            }

            if (preset.HasGlobalZone)
            {
                for (auto const& modulator : preset.GlobalZone.Modulators)
                {
                    bool replacedByLocal = false;

                    for (auto const& local : presetZone.Modulators)
                    {
                        replacedByLocal = replacedByLocal || local.IsSameAs(modulator);
                    }

                    if (!replacedByLocal)
                    {
                        AddModulator(voice, modulator, false);
                    }
                }
            }

            for (auto const& modulator : presetZone.Modulators)
            {
                AddModulator(voice, modulator, false);
            }

            // Sample addressing, with the zone's offsets, kept inside the sample data.
            auto const dataSize = static_cast<int64_t>(font.SampleData().size());
            auto const& g = voice.Generators;

            auto start = static_cast<int64_t>(sample.Start) + g[Gen::StartAddrsOffset] + 32768LL * g[Gen::StartAddrsCoarseOffset];
            auto end = static_cast<int64_t>(sample.End) + g[Gen::EndAddrsOffset] + 32768LL * g[Gen::EndAddrsCoarseOffset];
            auto loopStart = static_cast<int64_t>(sample.LoopStart) + g[Gen::StartloopAddrsOffset] + 32768LL * g[Gen::StartloopAddrsCoarseOffset];
            auto loopEnd = static_cast<int64_t>(sample.LoopEnd) + g[Gen::EndloopAddrsOffset] + 32768LL * g[Gen::EndloopAddrsCoarseOffset];

            start = (std::clamp)(start, 0LL, dataSize);
            end = (std::clamp)(end, 0LL, dataSize);

            if (end - start < 2)
            {
                voice.Active = false;
                return false;
            }

            loopStart = (std::clamp)(loopStart, start, end);
            loopEnd = (std::clamp)(loopEnd, start, end);

            auto loopMode = static_cast<uint8_t>(g[Gen::SampleModes] & 3);

            if (loopMode == 2 || !sample.LoopValid || loopEnd - loopStart < 2)
            {
                loopMode = 0;
            }

            voice.Data = font.SampleData().data();
            voice.Data24 = font.SampleData24().empty() ? nullptr : font.SampleData24().data();
            voice.Start = start;
            voice.End = end;
            voice.LoopStart = loopStart;
            voice.LoopEnd = loopEnd;
            voice.LoopMode = loopMode;
            voice.Looping = (loopMode == 1 || loopMode == 3);
            voice.Phase = static_cast<double>(start);
            voice.RateRatio = static_cast<double>(sample.SampleRate) / SampleRate;

            auto const keyOverride = g[Gen::Keynum];
            auto const velocityOverride = g[Gen::Velocity];

            voice.Channel = channel;
            voice.Note = note;
            voice.NoteOnId = noteOnId;
            voice.StartOrder = NextStartOrder++;
            voice.KeyHeld = true;
            voice.ExclusiveClass = g[Gen::ExclusiveClass];

            voice.ScalingKey = static_cast<uint8_t>((keyOverride >= 0 && keyOverride <= 127) ? keyOverride : note);
            voice.PitchNote = (keyOverride >= 0 && keyOverride <= 127) ? static_cast<double>(keyOverride) : pitchNote;
            voice.VelocityNormalized = (velocityOverride >= 0 && velocityOverride <= 127)
                ? static_cast<double>(velocityOverride) / 127.0
                : static_cast<double>(velocity) / 65535.0;

            auto const rootOverride = g[Gen::OverridingRootKey];

            voice.RootKey = (rootOverride >= 0 && rootOverride <= 127) ? rootOverride : sample.OriginalPitch;
            voice.ScaleTuning = (std::clamp)(static_cast<double>(g[Gen::ScaleTuning]), 0.0, 1200.0);
            voice.PitchCorrectionCents = sample.PitchCorrection;

            voice.Active = true;

            RefreshModulation(voice);

            // Envelope and LFO timing is fixed when the note starts.
            auto const keyOffset = 60.0 - static_cast<double>(voice.ScalingKey);

            auto& volume = voice.VolEnv;

            volume.DelaySeconds = TimecentsToSeconds(Effective(voice, Gen::DelayVolEnv));
            volume.AttackSeconds = TimecentsToSeconds(Effective(voice, Gen::AttackVolEnv));
            volume.HoldSeconds = TimecentsToSeconds(Effective(voice, Gen::HoldVolEnv) + Effective(voice, Gen::KeynumToVolEnvHold) * keyOffset);
            volume.DecaySeconds = TimecentsToSeconds(Effective(voice, Gen::DecayVolEnv) + Effective(voice, Gen::KeynumToVolEnvDecay) * keyOffset);
            volume.ReleaseSeconds = TimecentsToSeconds(Effective(voice, Gen::ReleaseVolEnv));
            volume.SustainDb = -(std::clamp)(Effective(voice, Gen::SustainVolEnv), 0.0, 1440.0) / 10.0;

            auto& modulation = voice.ModEnv;

            modulation.DelaySeconds = TimecentsToSeconds(Effective(voice, Gen::DelayModEnv));
            modulation.AttackSeconds = TimecentsToSeconds(Effective(voice, Gen::AttackModEnv));
            modulation.HoldSeconds = TimecentsToSeconds(Effective(voice, Gen::HoldModEnv) + Effective(voice, Gen::KeynumToModEnvHold) * keyOffset);
            modulation.DecaySeconds = TimecentsToSeconds(Effective(voice, Gen::DecayModEnv) + Effective(voice, Gen::KeynumToModEnvDecay) * keyOffset);
            modulation.ReleaseSeconds = TimecentsToSeconds(Effective(voice, Gen::ReleaseModEnv));
            modulation.SustainLevel = 1.0 - (std::clamp)(Effective(voice, Gen::SustainModEnv), 0.0, 1000.0) / 1000.0;

            voice.ModLfo.DelaySeconds = TimecentsToSeconds(Effective(voice, Gen::DelayModLfo));
            voice.VibLfo.DelaySeconds = TimecentsToSeconds(Effective(voice, Gen::DelayVibLfo));

            auto const active = CountActiveVoices();

            ActiveVoices.store(active, std::memory_order_relaxed);

            if (active > PeakVoices.load(std::memory_order_relaxed))
            {
                PeakVoices.store(active, std::memory_order_relaxed);
            }

            return true;
        }

        void NoteOn(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint16_t velocity, _In_ double pitchNote) noexcept
        {
            channel &= 0x0F;
            note &= 0x7F;

            auto& state = Channels[channel];

            if (!Initialized || Font == nullptr || state.PresetIndex < 0 ||
                static_cast<size_t>(state.PresetIndex) >= Font->Presets().size())
            {
                DroppedNotes.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            // Striking a key that is already down releases the earlier strike, as a damper would.
            for (auto& voice : Voices)
            {
                if (voice.Active && voice.Channel == channel && voice.Note == note && voice.KeyHeld && !voice.Detached)
                {
                    ReleaseVoice(voice);
                }
            }

            auto const& font = *Font;
            auto const& preset = font.Presets()[static_cast<size_t>(state.PresetIndex)];
            auto const velocity7 = static_cast<uint8_t>(velocity >> 9);
            auto const noteOnId = NextNoteOnId++;

            uint32_t started = 0;
            bool exclusiveHandled = false;

            for (auto const& presetZone : preset.Zones)
            {
                if (!presetZone.Matches(note, velocity7))
                {
                    continue;
                }

                auto const& instrument = font.Instruments()[presetZone.Link];

                for (auto const& instrumentZone : instrument.Zones)
                {
                    if (started >= MaximumVoicesPerNoteOn)
                    {
                        break;
                    }

                    if (!instrumentZone.Matches(note, velocity7))
                    {
                        continue;
                    }

                    // An exclusive class silences the others in its class on this channel, which is
                    // how a closed hi-hat cuts off an open one.
                    if (!exclusiveHandled)
                    {
                        int32_t exclusiveClass = instrument.HasGlobalZone && instrument.GlobalZone.IsSet(Gen::ExclusiveClass)
                            ? instrument.GlobalZone.Values[Gen::ExclusiveClass]
                            : 0;

                        if (instrumentZone.IsSet(Gen::ExclusiveClass))
                        {
                            exclusiveClass = instrumentZone.Values[Gen::ExclusiveClass];
                        }

                        if (exclusiveClass != 0)
                        {
                            for (auto& voice : Voices)
                            {
                                if (voice.Active && voice.Channel == channel &&
                                    voice.ExclusiveClass == exclusiveClass && voice.NoteOnId != noteOnId)
                                {
                                    KillVoice(voice);
                                }
                            }
                        }

                        exclusiveHandled = true;
                    }

                    if (StartVoice(channel, note, velocity, pitchNote, preset, presetZone, instrument, instrumentZone, noteOnId))
                    {
                        started++;
                    }
                }
            }

            if (started == 0)
            {
                DroppedNotes.fetch_add(1, std::memory_order_relaxed);
            }
        }

        void NoteOff(_In_ uint8_t channel, _In_ uint8_t note) noexcept
        {
            channel &= 0x0F;
            note &= 0x7F;

            auto const& state = Channels[channel];

            for (auto& voice : Voices)
            {
                if (!voice.Active || voice.Channel != channel || voice.Note != note || !voice.KeyHeld)
                {
                    continue;
                }

                if (state.SustainPedal)
                {
                    voice.KeyHeld = false;
                    voice.SustainHeld = true;
                }
                else if (voice.SostenutoHeld)
                {
                    voice.KeyHeld = false;
                }
                else
                {
                    ReleaseVoice(voice);
                }
            }
        }

        void ApplyDataEntry(_In_ uint8_t channel) noexcept
        {
            auto& state = Channels[channel];

            if (state.NrpnSelected)
            {
                return;
            }

            auto const msb = state.RawControllers[ControllerRpnMsb];
            auto const lsb = state.RawControllers[ControllerRpnLsb];
            auto const dataMsb = state.RawControllers[ControllerDataEntryMsb];
            auto const dataLsb = state.RawControllers[ControllerDataEntryLsb];

            if (msb != 0)
            {
                return;
            }

            switch (lsb)
            {
            case 0:
                state.PitchBendRangeSemitones = static_cast<double>(dataMsb) + static_cast<double>(dataLsb) / 100.0;
                break;

            case 1:
                state.FineTuneCents = (static_cast<double>((dataMsb << 7) | dataLsb) - 8192.0) / 8192.0 * 100.0;
                break;

            case 2:
                state.CoarseTuneSemitones = static_cast<double>(dataMsb) - 64.0;
                break;

            default:
                return;
            }

            RefreshChannel(channel);
        }

        void SetSustain(_In_ uint8_t channel, _In_ bool down) noexcept
        {
            auto& state = Channels[channel];

            state.SustainPedal = down;

            if (down)
            {
                return;
            }

            for (auto& voice : Voices)
            {
                if (voice.Active && voice.Channel == channel && voice.SustainHeld && !voice.KeyHeld && !voice.SostenutoHeld)
                {
                    ReleaseVoice(voice);
                }
            }
        }

        void SetSostenuto(_In_ uint8_t channel, _In_ bool down) noexcept
        {
            auto& state = Channels[channel];

            if (down == state.SostenutoPedal)
            {
                return;
            }

            state.SostenutoPedal = down;

            for (auto& voice : Voices)
            {
                if (!voice.Active || voice.Channel != channel)
                {
                    continue;
                }

                if (down)
                {
                    voice.SostenutoHeld = voice.KeyHeld;
                }
                else if (voice.SostenutoHeld)
                {
                    voice.SostenutoHeld = false;

                    if (!voice.KeyHeld)
                    {
                        if (state.SustainPedal)
                        {
                            voice.SustainHeld = true;
                        }
                        else
                        {
                            ReleaseVoice(voice);
                        }
                    }
                }
            }
        }

        void ControlChange(_In_ uint8_t channel, _In_ uint8_t controller, _In_ double normalized, _In_ uint8_t value7) noexcept
        {
            channel &= 0x0F;
            controller &= 0x7F;
            value7 &= 0x7F;

            auto& state = Channels[channel];

            switch (controller)
            {
            case ControllerAllSoundOff:
                AllSoundOff(channel);
                return;

            case ControllerResetAllControllers:
                ResetAllControllers(channel);
                return;

            case ControllerAllNotesOff:
            case 124:
            case 125:
            case 126:
            case 127:
                AllNotesOff(channel);
                return;

            default:
                break;
            }

            state.Controllers[controller] = normalized;
            state.RawControllers[controller] = value7;

            switch (controller)
            {
            case ControllerBankSelectMsb:
                state.BankMsb = value7;
                return;

            case ControllerBankSelectLsb:
                state.BankLsb = value7;
                return;

            case ControllerRpnLsb:
            case ControllerRpnMsb:
                state.NrpnSelected = false;
                state.RawControllers[ControllerDataEntryLsb] = 0;
                return;

            case ControllerNrpnLsb:
            case ControllerNrpnMsb:
                state.NrpnSelected = true;
                return;

            case ControllerDataEntryMsb:
            case ControllerDataEntryLsb:
                ApplyDataEntry(channel);
                return;

            case ControllerDataIncrement:
            case ControllerDataDecrement:
                if (!state.NrpnSelected && state.RawControllers[ControllerRpnMsb] == 0 && state.RawControllers[ControllerRpnLsb] == 0)
                {
                    auto const step = (controller == ControllerDataIncrement) ? 1.0 : -1.0;

                    state.PitchBendRangeSemitones = (std::clamp)(state.PitchBendRangeSemitones + step, 0.0, 127.0);
                    RefreshChannel(channel);
                }
                return;

            case ControllerSustain:
                SetSustain(channel, value7 >= 64);
                return;

            case ControllerSostenuto:
                SetSostenuto(channel, value7 >= 64);
                return;

            default:
                break;
            }

            RefreshChannel(channel);
        }

        void AllSoundOff(_In_ uint8_t channel) noexcept
        {
            for (auto& voice : Voices)
            {
                if (voice.Active && voice.Channel == channel)
                {
                    KillVoice(voice);
                }
            }
        }

        void AllNotesOff(_In_ uint8_t channel) noexcept
        {
            for (auto& voice : Voices)
            {
                if (voice.Active && voice.Channel == channel && voice.KeyHeld)
                {
                    NoteOff(channel, voice.Note);
                }
            }
        }

        // GM2 section 3.5.2 and M2-113 Appendix A agree on the scope: volume, pan, bank, program
        // and the effect sends are not reset.
        void ResetAllControllers(_In_ uint8_t channel) noexcept
        {
            auto& state = Channels[channel];

            SetControllerRaw(state, 1, 0);
            SetControllerRaw(state, ControllerExpression, 127);
            SetControllerRaw(state, 65, 0);
            SetControllerRaw(state, 67, 0);
            SetControllerRaw(state, ControllerRpnLsb, 127);
            SetControllerRaw(state, ControllerRpnMsb, 127);
            SetControllerRaw(state, ControllerNrpnLsb, 127);
            SetControllerRaw(state, ControllerNrpnMsb, 127);

            state.NrpnSelected = false;
            state.PitchWheel = 0.5;
            state.ChannelPressure = 0.0;
            state.PolyPressure.fill(0.0);

            SetControllerRaw(state, ControllerSostenuto, 0);
            SetSostenuto(channel, false);

            SetControllerRaw(state, ControllerSustain, 0);
            SetSustain(channel, false);

            RefreshChannel(channel);
        }

        void RenderVoice(_Inout_ Voice& voice, _Inout_updates_(frames * 2) float* output, _In_ uint32_t frames,
            _Inout_updates_opt_(frames * 2) float* reverb, _Inout_updates_opt_(frames * 2) float* chorus) noexcept
        {
            if (voice.Data24 != nullptr)
            {
                RenderVoiceSamples<true>(voice, output, frames, reverb, chorus);
            }
            else
            {
                RenderVoiceSamples<false>(voice, output, frames, reverb, chorus);
            }
        }

        template <bool With24Bit>
        static float FetchSample(_In_ Voice const& voice, _In_ int64_t index) noexcept
        {
            if (voice.Looping && index >= voice.LoopEnd)
            {
                index = voice.LoopStart + (index - voice.LoopEnd) % (voice.LoopEnd - voice.LoopStart);
            }

            if (index >= voice.End)
            {
                return 0.0f;
            }

            if (index < voice.Start)
            {
                index = voice.Start;
            }

            if constexpr (With24Bit)
            {
                auto const value = (static_cast<int32_t>(voice.Data[index]) * 256) | voice.Data24[index];
                return static_cast<float>(value) * (1.0f / 8388608.0f);
            }
            else
            {
                return static_cast<float>(voice.Data[index]) * (1.0f / 32768.0f);
            }
        }

        // Four point Hermite. Reads go through FetchSample whenever any of the four points could
        // fall outside the sample or across the loop seam.
        template <bool With24Bit>
        static float Interpolate(_In_ Voice const& voice) noexcept
        {
            auto const index = static_cast<int64_t>(voice.Phase);
            auto const fraction = static_cast<float>(voice.Phase - static_cast<double>(index));
            auto const limit = voice.Looping ? voice.LoopEnd : voice.End;

            float yMinus1;
            float y0;
            float y1;
            float y2;

            if (index - 1 >= voice.Start && index + 2 < limit)
            {
                if constexpr (With24Bit)
                {
                    yMinus1 = FetchSample<true>(voice, index - 1);
                    y0 = FetchSample<true>(voice, index);
                    y1 = FetchSample<true>(voice, index + 1);
                    y2 = FetchSample<true>(voice, index + 2);
                }
                else
                {
                    constexpr float scale = 1.0f / 32768.0f;

                    yMinus1 = voice.Data[index - 1] * scale;
                    y0 = voice.Data[index] * scale;
                    y1 = voice.Data[index + 1] * scale;
                    y2 = voice.Data[index + 2] * scale;
                }
            }
            else
            {
                yMinus1 = FetchSample<With24Bit>(voice, index - 1);
                y0 = FetchSample<With24Bit>(voice, index);
                y1 = FetchSample<With24Bit>(voice, index + 1);
                y2 = FetchSample<With24Bit>(voice, index + 2);
            }

            auto const c1 = 0.5f * (y1 - yMinus1);
            auto const c2 = yMinus1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            auto const c3 = 0.5f * (y2 - yMinus1) + 1.5f * (y0 - y1);

            return ((c3 * fraction + c2) * fraction + c1) * fraction + y0;
        }

        template <bool With24Bit>
        void RenderVoiceSamples(_Inout_ Voice& voice, _Inout_updates_(frames * 2) float* output, _In_ uint32_t frames,
            _Inout_updates_opt_(frames * 2) float* reverb, _Inout_updates_opt_(frames * 2) float* chorus) noexcept
        {
            uint32_t done = 0;

            while (done < frames && voice.Active)
            {
                auto const count = (std::min)(ControlBlockFrames, frames - done);
                auto const seconds = static_cast<double>(count) / SampleRate;

                voice.VolEnv.Advance(seconds);
                voice.ModEnv.Advance(seconds);
                voice.ModLfo.Advance(seconds);
                voice.VibLfo.Advance(seconds);

                auto const envelope = voice.ModEnv.Level;
                auto const modLfo = voice.ModLfo.Value();
                auto const vibLfo = voice.VibLfo.Value();

                auto const pitchCents = (std::clamp)(
                    voice.PitchCents + vibLfo * voice.VibLfoToPitch + modLfo * voice.ModLfoToPitch + envelope * voice.ModEnvToPitch,
                    -12800.0, 12800.0);

                auto const increment = std::exp2(pitchCents / 1200.0) * voice.RateRatio;

                auto const cutoffCents = (std::clamp)(
                    voice.FilterCutoffCents + envelope * voice.ModEnvToFilter + modLfo * voice.ModLfoToFilter,
                    1500.0, 13500.0);

                voice.Filter.Configure(AbsoluteCentsToHertz(cutoffCents), voice.FilterQ, SampleRate);

                auto const gainDb = -voice.AttenuationDb + modLfo * voice.ModLfoToVolumeDb + voice.PerNoteGainDb;
                auto targetGain = static_cast<float>(voice.VolEnv.Amplitude() * DecibelsToAmplitude(gainDb));

                bool finished = voice.VolEnv.Stage == EnvelopeStage::Finished;

                if (voice.Killing)
                {
                    voice.KillGain -= static_cast<float>(seconds / KillSeconds);

                    if (voice.KillGain <= 0.0f)
                    {
                        voice.KillGain = 0.0f;
                        finished = true;
                    }

                    targetGain *= voice.KillGain;
                }

                auto const step = (targetGain - voice.CurrentGain) / static_cast<float>(count);
                auto gain = voice.CurrentGain;

                for (uint32_t frame = 0; frame < count; frame++)
                {
                    auto sample = voice.Filter.Process(Interpolate<With24Bit>(voice));

                    gain += step;
                    sample *= gain;

                    auto const left = sample * voice.PanLeft;
                    auto const right = sample * voice.PanRight;
                    auto const at = static_cast<size_t>(done + frame) * 2;

                    output[at] += left;
                    output[at + 1] += right;

                    if (reverb != nullptr)
                    {
                        reverb[at] += left * voice.ReverbSend;
                        reverb[at + 1] += right * voice.ReverbSend;
                    }

                    if (chorus != nullptr)
                    {
                        chorus[at] += left * voice.ChorusSend;
                        chorus[at + 1] += right * voice.ChorusSend;
                    }

                    voice.Phase += increment;

                    if (voice.Looping)
                    {
                        if (voice.Phase >= static_cast<double>(voice.LoopEnd))
                        {
                            auto const length = static_cast<double>(voice.LoopEnd - voice.LoopStart);

                            voice.Phase = static_cast<double>(voice.LoopStart) +
                                std::fmod(voice.Phase - static_cast<double>(voice.LoopStart), length);
                        }
                    }
                    else if (voice.Phase >= static_cast<double>(voice.End))
                    {
                        finished = true;
                        break;
                    }
                }

                voice.CurrentGain = targetGain;

                if (finished)
                {
                    voice.Active = false;
                }

                done += count;
            }
        }

        void Render(_Out_writes_(frameCount * 2) float* output, _In_ uint32_t frameCount) noexcept
        {
            std::fill_n(output, static_cast<size_t>(frameCount) * 2, 0.0f);

            if (!Initialized || Font == nullptr)
            {
                return;
            }

            uint32_t rendered = 0;

            while (rendered < frameCount)
            {
                auto const frames = (std::min)(MaximumRenderFrames, frameCount - rendered);
                auto* const chunk = output + static_cast<size_t>(rendered) * 2;
                auto const samples = static_cast<size_t>(frames) * 2;

                auto const effects =
                    Config.EnableEffects &&
                    ReverbSendBuffer.size() >= samples &&
                    ChorusSendBuffer.size() >= samples;

                if (effects)
                {
                    std::fill_n(ReverbSendBuffer.data(), samples, 0.0f);
                    std::fill_n(ChorusSendBuffer.data(), samples, 0.0f);
                }

                if (ActiveSensingSeen)
                {
                    SecondsSinceActiveSensing += static_cast<double>(frames) / SampleRate;

                    // A sender that used active sensing and went quiet for 300 ms has gone away.
                    if (SecondsSinceActiveSensing > 0.3)
                    {
                        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
                        {
                            AllSoundOff(channel);
                        }

                        ActiveSensingSeen = false;
                        SecondsSinceActiveSensing = 0.0;
                    }
                }

                for (auto& voice : Voices)
                {
                    if (voice.Active)
                    {
                        RenderVoice(voice, chunk, frames,
                            effects ? ReverbSendBuffer.data() : nullptr,
                            effects ? ChorusSendBuffer.data() : nullptr);
                    }
                }

                if (effects)
                {
                    Chorus.Process(ChorusSendBuffer.data(), chunk, frames);
                    Reverb.Process(ReverbSendBuffer.data(), chunk, frames);
                }

                auto const gain = static_cast<float>(DecibelsToAmplitude(
                    EngineGainDb + MasterVolumeDb + UserVolumeDb.load(std::memory_order_relaxed)));

                for (size_t i = 0; i < samples; i++)
                {
                    chunk[i] *= gain;
                }

                rendered += frames;
            }

            ActiveVoices.store(CountActiveVoices(), std::memory_order_relaxed);
        }
    };

    Synthesizer::Synthesizer() noexcept :
        m_impl(std::make_unique<Impl>())
    {
    }

    Synthesizer::~Synthesizer() = default;

    _Use_decl_annotations_
    bool Synthesizer::Initialize(std::shared_ptr<SoundFont const> font, SynthesizerConfig const& config)
    {
        auto& impl = *m_impl;

        if (font == nullptr || font->Presets().empty() || config.SampleRate < 8000 || config.SampleRate > 768000)
        {
            return false;
        }

        impl.Font = std::move(font);
        impl.Config = config;
        impl.Config.Polyphony = (std::clamp)(config.Polyphony, 1u, 1024u);
        impl.SampleRate = static_cast<double>(config.SampleRate);

        impl.DetectedBankMode = (config.BankSelect == BankSelectMode::Automatic)
            ? BankSelectMode::RolandGS
            : config.BankSelect;

        impl.Voices.assign(static_cast<size_t>(impl.Config.Polyphony) + ReserveVoices, Voice{});

        if (config.EnableEffects)
        {
            impl.ReverbSendBuffer.assign(static_cast<size_t>(MaximumRenderFrames) * 2, 0.0f);
            impl.ChorusSendBuffer.assign(static_cast<size_t>(MaximumRenderFrames) * 2, 0.0f);

            if (!impl.Reverb.Configure(config.SampleRate) || !impl.Chorus.Configure(config.SampleRate))
            {
                return false;
            }

            // The same character as the in-box General MIDI synthesizer.
            impl.Reverb.SetParameter(MidiSynth::AudioEffectParameter::WetLevel, 0.9);
            impl.Reverb.SetParameter(MidiSynth::AudioEffectParameter::Time, 1.8);
            impl.Reverb.SetParameter(MidiSynth::AudioEffectParameter::Damping, 0.4);

            impl.Chorus.SetParameter(MidiSynth::AudioEffectParameter::WetLevel, 0.9);
            impl.Chorus.SetParameter(MidiSynth::AudioEffectParameter::Rate, 0.8);
            impl.Chorus.SetParameter(MidiSynth::AudioEffectParameter::Depth, 0.4);
        }
        else
        {
            impl.ReverbSendBuffer.clear();
            impl.ChorusSendBuffer.clear();
        }

        impl.Initialized = true;

        // The presets point into the new bank, so every channel looks its preset up again.
        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            impl.ResolvePreset(channel);
        }

        return true;
    }

    bool Synthesizer::IsInitialized() const noexcept
    {
        return m_impl->Initialized;
    }

    uint32_t Synthesizer::SampleRate() const noexcept
    {
        return m_impl->Config.SampleRate;
    }

    _Use_decl_annotations_
    void Synthesizer::NoteOn(uint8_t channel, uint8_t note, uint16_t velocity)
    {
        m_impl->NoteOn(channel, note, velocity, static_cast<double>(note & 0x7F));
    }

    _Use_decl_annotations_
    void Synthesizer::NoteOnWithPitch(uint8_t channel, uint8_t note, uint16_t velocity, double pitchNoteNumber)
    {
        m_impl->NoteOn(channel, note, velocity, (std::clamp)(pitchNoteNumber, 0.0, 127.998));
    }

    _Use_decl_annotations_
    void Synthesizer::NoteOff(uint8_t channel, uint8_t note)
    {
        m_impl->NoteOff(channel, note);
    }

    _Use_decl_annotations_
    void Synthesizer::ControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
        m_impl->ControlChange(channel, controller, NormalizeCentered7(value), value);
    }

    _Use_decl_annotations_
    void Synthesizer::ControlChange32(uint8_t channel, uint8_t controller, uint32_t value)
    {
        m_impl->ControlChange(channel, controller, NormalizeCentered32(value), static_cast<uint8_t>(value >> 25));
    }

    _Use_decl_annotations_
    void Synthesizer::ProgramChange(uint8_t channel, uint8_t program)
    {
        channel &= 0x0F;

        m_impl->Channels[channel].Program = program & 0x7F;
        m_impl->ResolvePreset(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::ProgramChangeWithBank(uint8_t channel, uint8_t bankMsb, uint8_t bankLsb, uint8_t program)
    {
        channel &= 0x0F;

        auto& state = m_impl->Channels[channel];

        state.BankMsb = bankMsb & 0x7F;
        state.BankLsb = bankLsb & 0x7F;
        state.Program = program & 0x7F;

        Impl::SetControllerRaw(state, ControllerBankSelectMsb, state.BankMsb);
        Impl::SetControllerRaw(state, ControllerBankSelectLsb, state.BankLsb);

        m_impl->ResolvePreset(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::PitchBend(uint8_t channel, uint16_t value)
    {
        channel &= 0x0F;

        m_impl->Channels[channel].PitchWheel = NormalizeCentered14(value);
        m_impl->RefreshChannel(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::PitchBend32(uint8_t channel, uint32_t value)
    {
        channel &= 0x0F;

        m_impl->Channels[channel].PitchWheel = NormalizeCentered32(value);
        m_impl->RefreshChannel(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::ChannelPressure(uint8_t channel, uint8_t value)
    {
        channel &= 0x0F;

        m_impl->Channels[channel].ChannelPressure = static_cast<double>(value & 0x7F) / 127.0;
        m_impl->RefreshChannel(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::ChannelPressure32(uint8_t channel, uint32_t value)
    {
        channel &= 0x0F;

        m_impl->Channels[channel].ChannelPressure = static_cast<double>(value) / 4294967295.0;
        m_impl->RefreshChannel(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::PolyPressure(uint8_t channel, uint8_t note, uint8_t value)
    {
        PolyPressure32(channel, note, static_cast<uint32_t>((value & 0x7F) * 33818640u));
    }

    _Use_decl_annotations_
    void Synthesizer::PolyPressure32(uint8_t channel, uint8_t note, uint32_t value)
    {
        channel &= 0x0F;
        note &= 0x7F;

        auto& impl = *m_impl;

        impl.Channels[channel].PolyPressure[note] = static_cast<double>(value) / 4294967295.0;

        for (auto& voice : impl.Voices)
        {
            if (voice.Active && voice.Channel == channel && voice.Note == note)
            {
                impl.RefreshModulation(voice);
            }
        }
    }

    _Use_decl_annotations_
    void Synthesizer::RegisteredController(uint8_t channel, uint8_t bank, uint8_t index, uint32_t value)
    {
        channel &= 0x0F;

        auto& state = m_impl->Channels[channel];

        if (bank != 0)
        {
            return;
        }

        switch (index)
        {
        case 0:
            state.PitchBendRangeSemitones = static_cast<double>(value >> 25) + static_cast<double>((value >> 18) & 0x7F) / 100.0;
            break;

        case 1:
            state.FineTuneCents = (NormalizeCentered32(value) * 2.0 - 1.0) * 100.0;
            break;

        case 2:
            state.CoarseTuneSemitones = static_cast<double>(value >> 25) - 64.0;
            break;

        default:
            return;
        }

        m_impl->RefreshChannel(channel);
    }

    _Use_decl_annotations_
    void Synthesizer::PerNotePitchBend(uint8_t channel, uint8_t note, double normalized)
    {
        channel &= 0x0F;
        note &= 0x7F;

        auto& impl = *m_impl;
        auto const cents = (std::clamp)(normalized, -1.0, 1.0) * impl.Channels[channel].PitchBendRangeSemitones * 100.0;

        for (auto& voice : impl.Voices)
        {
            if (voice.Active && voice.Channel == channel && voice.Note == note && !voice.Detached)
            {
                voice.PerNoteBendCents = cents;
                impl.RefreshDynamic(voice);
            }
        }
    }

    _Use_decl_annotations_
    void Synthesizer::PerNoteController(uint8_t channel, uint8_t note, uint8_t controller, uint32_t value)
    {
        channel &= 0x0F;
        note &= 0x7F;

        auto& impl = *m_impl;

        for (auto& voice : impl.Voices)
        {
            if (!voice.Active || voice.Channel != channel || voice.Note != note || voice.Detached)
            {
                continue;
            }

            switch (controller)
            {
            case PerNoteControllerPitch:
            {
                // Pitch 7.25 is absolute, so it becomes an offset from the pitch the note started at.
                auto const pitch = static_cast<double>(value >> 25) + static_cast<double>(value & 0x01FFFFFF) / 33554432.0;

                voice.PerNoteTuningCents = (pitch - voice.PitchNote) * 100.0;
                break;
            }

            case PerNoteControllerVolume:
                voice.PerNoteGainDb = ConcaveDb(static_cast<double>(value) / 4294967295.0);
                break;

            case PerNoteControllerPan:
                voice.PerNotePan = (NormalizeCentered32(value) - 0.5) * 1000.0;
                break;

            default:
                continue;
            }

            impl.RefreshDynamic(voice);
        }
    }

    _Use_decl_annotations_
    void Synthesizer::PerNoteManagement(uint8_t channel, uint8_t note, bool detach, bool resetPerNoteControllers)
    {
        channel &= 0x0F;
        note &= 0x7F;

        auto& impl = *m_impl;

        for (auto& voice : impl.Voices)
        {
            if (!voice.Active || voice.Channel != channel || voice.Note != note || voice.Detached)
            {
                continue;
            }

            if (resetPerNoteControllers)
            {
                voice.PerNoteBendCents = 0.0;
                voice.PerNoteTuningCents = 0.0;
                voice.PerNoteGainDb = 0.0;
                voice.PerNotePan = 0.0;

                impl.RefreshDynamic(voice);
            }

            if (detach)
            {
                voice.Detached = true;
            }
        }
    }

    _Use_decl_annotations_
    void Synthesizer::AllSoundOff(uint8_t channel)
    {
        m_impl->AllSoundOff(channel & 0x0F);
    }

    _Use_decl_annotations_
    void Synthesizer::AllNotesOff(uint8_t channel)
    {
        m_impl->AllNotesOff(channel & 0x0F);
    }

    _Use_decl_annotations_
    void Synthesizer::ResetAllControllers(uint8_t channel)
    {
        m_impl->ResetAllControllers(channel & 0x0F);
    }

    void Synthesizer::SystemReset()
    {
        auto& impl = *m_impl;

        for (auto& voice : impl.Voices)
        {
            impl.KillVoice(voice);
        }

        impl.MasterVolumeDb = 0.0;
        impl.MasterFineCents = 0.0;
        impl.MasterCoarseCents = 0.0;

        impl.ResetChannels();
    }

    _Use_decl_annotations_
    void Synthesizer::SetDrumChannel(uint8_t channel, bool isDrumChannel)
    {
        channel &= 0x0F;

        m_impl->Channels[channel].IsDrumChannel = isDrumChannel;
        m_impl->ResolvePreset(channel);
    }

    _Use_decl_annotations_
    bool Synthesizer::IsDrumChannel(uint8_t channel) const noexcept
    {
        return m_impl->Channels[channel & 0x0F].EffectiveDrum;
    }

    _Use_decl_annotations_
    void Synthesizer::SetBankSelectMode(BankSelectMode mode)
    {
        auto& impl = *m_impl;

        impl.Config.BankSelect = mode;
        impl.DetectedBankMode = (mode == BankSelectMode::Automatic) ? BankSelectMode::RolandGS : mode;

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            impl.ResolvePreset(channel);
        }
    }

    _Use_decl_annotations_
    void Synthesizer::NotifyAddressingConvention(BankSelectMode convention)
    {
        auto& impl = *m_impl;

        if (impl.Config.BankSelect != BankSelectMode::Automatic || convention == BankSelectMode::Automatic)
        {
            return;
        }

        impl.DetectedBankMode = convention;

        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            impl.ResolvePreset(channel);
        }
    }

    _Use_decl_annotations_
    void Synthesizer::SetMasterVolume(uint16_t value) noexcept
    {
        m_impl->MasterVolumeDb = ConcaveDb(static_cast<double>((std::min)(value, static_cast<uint16_t>(16383))) / 16383.0);
    }

    _Use_decl_annotations_
    void Synthesizer::SetMasterFineTuning(uint16_t value) noexcept
    {
        auto& impl = *m_impl;

        impl.MasterFineCents = (static_cast<double>(value & 0x3FFF) - 8192.0) / 8192.0 * 100.0;

        for (auto& voice : impl.Voices)
        {
            if (voice.Active)
            {
                impl.RefreshDynamic(voice);
            }
        }
    }

    _Use_decl_annotations_
    void Synthesizer::SetMasterCoarseTuning(uint8_t msb) noexcept
    {
        auto& impl = *m_impl;

        impl.MasterCoarseCents = (static_cast<double>(msb & 0x7F) - 64.0) * 100.0;

        for (auto& voice : impl.Voices)
        {
            if (voice.Active)
            {
                impl.RefreshDynamic(voice);
            }
        }
    }

    _Use_decl_annotations_
    void Synthesizer::SetUserVolumeDb(double decibels) noexcept
    {
        if (!(decibels == decibels))
        {
            return;
        }

        m_impl->UserVolumeDb.store((std::clamp)(decibels, MinimumUserVolumeDb, MaximumUserVolumeDb), std::memory_order_relaxed);
    }

    double Synthesizer::UserVolumeDb() const noexcept
    {
        return m_impl->UserVolumeDb.load(std::memory_order_relaxed);
    }

    void Synthesizer::ActiveSensing() noexcept
    {
        m_impl->ActiveSensingSeen = true;
        m_impl->SecondsSinceActiveSensing = 0.0;
    }

    MidiSynth::IAudioEffect* Synthesizer::Reverb() noexcept
    {
        return m_impl->Config.EnableEffects ? &m_impl->Reverb : nullptr;
    }

    MidiSynth::IAudioEffect* Synthesizer::Chorus() noexcept
    {
        return m_impl->Config.EnableEffects ? &m_impl->Chorus : nullptr;
    }

    _Use_decl_annotations_
    void Synthesizer::Render(float* interleavedStereo, uint32_t frameCount) noexcept
    {
        m_impl->Render(interleavedStereo, frameCount);
    }

    uint32_t Synthesizer::ActiveVoiceCount() const noexcept
    {
        return m_impl->ActiveVoices.load(std::memory_order_relaxed);
    }

    _Use_decl_annotations_
    ChannelSelection Synthesizer::Selection(uint8_t channel) const noexcept
    {
        auto const packed = m_impl->PublishedSelection[channel & 0x0F].load(std::memory_order_acquire);

        ChannelSelection selection{};

        selection.BankMsb = static_cast<uint8_t>(packed & 0xFF);
        selection.BankLsb = static_cast<uint8_t>((packed >> 8) & 0xFF);
        selection.Program = static_cast<uint8_t>((packed >> 16) & 0xFF);
        selection.IsDrumChannel = ((packed >> 24) & 1) != 0;
        selection.PresetIndex = static_cast<int32_t>(static_cast<uint32_t>(packed >> 32));

        return selection;
    }

    SynthesizerStatistics Synthesizer::Statistics() const noexcept
    {
        SynthesizerStatistics statistics{};

        statistics.DroppedNotes = m_impl->DroppedNotes.load(std::memory_order_relaxed);
        statistics.StolenVoices = m_impl->StolenVoices.load(std::memory_order_relaxed);
        statistics.PeakVoices = m_impl->PeakVoices.load(std::memory_order_relaxed);

        return statistics;
    }

    std::shared_ptr<SoundFont const> const& Synthesizer::Font() const noexcept
    {
        return m_impl->Font;
    }
}
