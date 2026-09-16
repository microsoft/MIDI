// Wavetable voice engine over a DLS collection.
//
// Render() produces interleaved stereo float at SynthConfig::RenderSampleRate(), which in
// Compatible mode is 22050 rather than the output rate. Getting that to the device rate is the
// audio sink's job, so nothing here has to know what the device is doing.

#pragma once

#include "Articulation.h"
#include "ChorusEffect.h"
#include "DlsCollection.h"
#include "ReverbEffect.h"
#include "SynthConfig.h"

#include <sal.h>

#include <array>
#include <cstdint>
#include <vector>

namespace MidiSynth
{
    constexpr size_t MidiChannelCount = 16;

    enum class EnvelopeStage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release,
    };

    struct SynthVoice
    {
        bool Active{ false };

        uint8_t Channel{ 0 };
        uint8_t Note{ 0 };
        uint16_t Velocity{ 0 };
        uint16_t KeyGroup{ 0 };
        bool SelfNonExclusive{ false };

        // Note is still physically held. A voice released by the sustain pedal has this clear
        // but stays in Sustain until the pedal lifts.
        bool NoteHeld{ false };
        bool SustainHeld{ false };

        uint64_t StartOrder{ 0 };

        const int16_t* Samples{ nullptr };
        uint32_t SampleFrameCount{ 0 };
        bool Looping{ false };
        uint32_t LoopStart{ 0 };
        uint32_t LoopEnd{ 0 };

        double Phase{ 0.0 };
        double BasePitchRatio{ 1.0 };

        // Recomputed once per control block, held constant across it.
        double BlockPitchRatio{ 1.0 };

        ResolvedArticulation Articulation;

        EnvelopeStage Stage{ EnvelopeStage::Idle };
        double EnvelopeLevelDb{ 0.0 };
        double AttackProgress{ 0.0 };
        double AttackSeconds{ 0.0 };
        double DecaySeconds{ 0.0 };
        double ReleaseSeconds{ 0.0 };
        double SustainDb{ 0.0 };

        double LfoPhase{ 0.0 };
        double LfoDelayRemainingSeconds{ 0.0 };

        double StaticGainDb{ 0.0 };

        // Gains are ramped from Current to Target across a control block so parameter changes
        // do not step the waveform.
        double CurrentGainLeft{ 0.0 };
        double CurrentGainRight{ 0.0 };
        double TargetGainLeft{ 0.0 };
        double TargetGainRight{ 0.0 };
        bool GainsPrimed{ false };
    };

    struct SynthChannelState
    {
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t Program{ 0 };
        bool IsDrumChannel{ false };

        // GM2 effect sends, CC91 and CC93. Reverb defaults to 40 of 127 per GM2.
        double ReverbSend{ 40.0 / 127.0 };
        double ChorusSend{ 0.0 };

        // Held normalized rather than as 7 bit values so a 32 bit MIDI 2.0 controller keeps its
        // resolution. The 7 bit entry points map exactly as before, so MIDI 1.0 is unchanged.
        double Volume{ 100.0 / 127.0 };
        double Expression{ 1.0 };
        double Modulation{ 0.0 };

        // -0.5 is hard left, +0.5 is hard right, 0 is center.
        double PanOffset{ 0.0 };
        bool SustainPedal{ false };

        // -1 to +1 across the bend range, so a 32 bit bend is not quantized to fourteen bits.
        double PitchBendNormalized{ 0.0 };
        double PitchBendRangeSemitones{ 2.0 };
        double FineTuneCents{ 0.0 };
        double CoarseTuneSemitones{ 0.0 };

        uint8_t RpnMsb{ 0x7F };
        uint8_t RpnLsb{ 0x7F };

        const DlsInstrument* Instrument{ nullptr };
    };

    class SynthEngine
    {
    public:
        // The collection must outlive the engine.
        bool Initialize(_In_ const DlsCollection* collection, _In_ const SynthConfig& config);

        void NoteOn(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint16_t velocity);
        void NoteOff(_In_ uint8_t channel, _In_ uint8_t note);
        void ControlChange(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint8_t value);
        void ProgramChange(_In_ uint8_t channel, _In_ uint8_t program);

        // MIDI 2.0 entry points. The value keeps its full width rather than being reduced to
        // 7 bits, which is the whole reason for using them.
        void ControlChange32(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint32_t value);
        void PitchBend32(_In_ uint8_t channel, _In_ uint32_t value);

        // MIDI 2.0 program change carries the bank with it, so it cannot be split across messages.
        void ProgramChangeWithBank(
            _In_ uint8_t channel,
            _In_ uint8_t bankMsb,
            _In_ uint8_t bankLsb,
            _In_ uint8_t program);

        // Fourteen bit value, 8192 is centered.
        void PitchBend(_In_ uint8_t channel, _In_ int32_t value);

        void AllSoundOff(_In_ uint8_t channel);
        void AllNotesOff(_In_ uint8_t channel);
        void ResetAllControllers(_In_ uint8_t channel);
        void SystemReset();

        // GM2 requires a device to respond to Active Sensing. Once a sender has used it, silence
        // for longer than the specified timeout means the link is gone and everything stops.
        void ActiveSensing() noexcept;

        // Exposed so system level parameter messages can reach them without the dispatcher
        // knowing how either effect is built.
        _Ret_maybenull_ IAudioEffect* Reverb() noexcept { return m_config.EnableEffects ? &m_reverb : nullptr; }
        _Ret_maybenull_ IAudioEffect* Chorus() noexcept { return m_config.EnableEffects ? &m_chorus : nullptr; }

        // GM2 device controls. Fourteen bit, full scale is 16383, and the square of the value is
        // proportional to volume, which is the same concave curve CC7 uses.
        void SetMasterVolume(_In_ uint16_t value) noexcept;

        // Fourteen bit, 8192 is centered, full range is plus or minus 100 cents.
        void SetMasterFineTuning(_In_ uint16_t value) noexcept;

        // Semitones, 0x40 is centered. Applied as pitch, never as a note offset: shifting notes
        // would select a different sound on a drum kit.
        void SetMasterCoarseTuning(_In_ uint8_t msb) noexcept;

        void Render(_Out_writes_(frameCount * 2) float* interleavedStereo, _In_ uint32_t frameCount) noexcept;

        uint32_t ActiveVoiceCount() const noexcept;
        uint64_t StolenVoiceCount() const noexcept { return m_stolenVoiceCount; }
        uint64_t DroppedNoteCount() const noexcept { return m_droppedNoteCount; }

        // Samples that reached full scale after limiting. Should stay zero.
        uint64_t ClippedSampleCount() const noexcept { return m_clippedSampleCount; }

        // Most the limiter has had to pull the mix down, in dB. Zero means it never engaged.
        double PeakGainReductionDb() const noexcept;

        // Loudest output sample seen, in dBFS. Shows how much headroom a session actually used,
        // which the limiter reduction alone does not: it stays at zero right up to the threshold.
        double PeakOutputDbfs() const noexcept;

        const SynthConfig& Config() const noexcept { return m_config; }

    private:
        _Ret_maybenull_ const DlsRegion* SelectRegion(
            _In_ const DlsInstrument& instrument,
            _In_ uint8_t note,
            _In_ uint8_t velocity7) const noexcept;

        void ResolveInstrument(_In_ uint8_t channel) noexcept;
        _Ret_maybenull_ SynthVoice* AllocateVoice(_In_ uint8_t channel) noexcept;
        void StartVoice(
            _In_ SynthVoice& voice,
            _In_ const DlsRegion& region,
            _In_ const DlsWave& wave,
            _In_ const DlsInstrument& instrument,
            _In_ uint8_t channel,
            _In_ uint8_t note,
            _In_ uint16_t velocity) noexcept;

        void ReleaseVoice(_In_ SynthVoice& voice) noexcept;

        // Ends a voice with a short fade rather than an instant cut, which would leave a step in
        // the waveform and be heard as a click.
        void KillVoice(_In_ SynthVoice& voice) noexcept;

        void UpdateVoiceControl(_In_ SynthVoice& voice, _In_ uint32_t frames) noexcept;
        void RenderVoiceBlock(
            _In_ SynthVoice& voice,
            _Inout_updates_(frames * 2) float* output,
            _In_ uint32_t frames) noexcept;

        const DlsCollection* m_collection{ nullptr };
        SynthConfig m_config;
        double m_renderSampleRate{ 48000.0 };

        std::vector<SynthVoice> m_voices;
        std::array<SynthChannelState, MidiChannelCount> m_channels;

        uint64_t m_nextStartOrder{ 1 };
        uint64_t m_stolenVoiceCount{ 0 };
        uint64_t m_droppedNoteCount{ 0 };
        uint64_t m_clippedSampleCount{ 0 };

        double m_limiterGain{ 1.0 };

        // GM2 device controls, applied on top of the configured master gain.
        double m_masterVolumeDb{ 0.0 };
        double m_masterFineCents{ 0.0 };
        double m_masterCoarseCents{ 0.0 };
        double m_masterTuningCents{ 0.0 };

        ReverbEffect m_reverb;
        ChorusEffect m_chorus;

        std::vector<float> m_reverbSendBuffer;
        std::vector<float> m_chorusSendBuffer;

        // Where the current Render call is writing, so a voice can find its offset into the sends.
        float* m_renderCursor{ nullptr };

        bool m_activeSensingSeen{ false };
        double m_framesSinceActiveSensing{ 0.0 };
        double m_lowestLimiterGain{ 1.0 };
        double m_peakOutput{ 0.0 };
    };
}
