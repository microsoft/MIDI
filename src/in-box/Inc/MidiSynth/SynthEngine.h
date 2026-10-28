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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace MidiSynth
{
    constexpr size_t MidiChannelCount = 16;

    // Registered Per-Note Controller numbers this engine acts on. The numbering is shared with the
    // MIDI 1.0 control change assignments, which is why these look familiar.
    constexpr uint8_t PerNoteControllerPitch = 3;
    constexpr uint8_t PerNoteControllerVolume = 7;
    constexpr uint8_t PerNoteControllerPan = 10;

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

        // Started from a drum kit. General MIDI 2 recommends that drums ignore modulation, pitch bend,
        // the damper pedal, and channel and master tuning, and the engine applies that per note: a
        // note keeps this when its channel changes program while it sounds.
        bool DrumKit{ false };

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

        // MIDI 2.0 per-note state. Every one of these is per sounding note rather than per channel,
        // which is the point of them: two notes on one channel can be bent, panned and balanced
        // independently, and MIDI 1.0 has no way to express that.
        //
        // The note number the voice is tuned to, which is fractional when a Pitch attribute or a
        // per-note pitch controller asked for a pitch between the keys. Kept so an absolute pitch
        // arriving later can be turned into an offset from where the voice started.
        double PitchNoteNumber{ 0.0 };

        // Held apart because they arrive on different messages and neither should erase the other.
        double PerNoteTuningCents{ 0.0 };
        double PerNoteBendCents{ 0.0 };

        double PerNoteGain{ 1.0 };
        double PerNotePan{ 0.0 };

        // A detached note keeps sounding under its own per-note control even when the same note
        // number is played again on the same channel, so a held note is not stolen by the retrigger.
        bool Detached{ false };

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

        // A reset fades the voice out rather than cutting it off, and in the same moment puts the
        // channel and master controls back to their defaults: volume up to 100, pan centered,
        // reverb send up to 40. These are what the voice had just before the reset, so the fade
        // carries on at the level, place, pitch and sends the note had instead of jumping to the
        // new ones. Without them a quiet note comes back loud for the length of the fade.
        bool ControlsHeldFromReset{ false };
        double HeldChannelGainDb{ 0.0 };
        double HeldMasterVolumeDb{ 0.0 };
        double HeldModulation{ 0.0 };
        double HeldPanOffset{ 0.0 };
        double HeldBendCents{ 0.0 };
        double HeldTuningCents{ 0.0 };
        float HeldReverbSend{ 0.0f };
        float HeldChorusSend{ 0.0f };
    };

    struct SynthChannelState
    {
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t Program{ 0 };
        bool IsDrumChannel{ false };

        // The channel plays a drum kit: it is a drum channel, or a bank select chose a kit under XG
        // or GM2. Under GM2 a melodic bank also turns a drum channel back into a melodic one.
        // General MIDI 2 calls this a Rhythm Channel. Set when the program is resolved, so a bank
        // select changes nothing until the program change that follows it.
        bool PlaysDrumKit{ false };

        // The bank the program was looked up in once the addressing convention was applied: the
        // variation for a melodic program, zero for a kit. Set with PlaysDrumKit, so MIDI-CI can
        // name what plays even before the engine has a sound set.
        uint8_t VariationBank{ 0 };

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

        // Registered parameter 0,0 as it arrived: whole semitones and cents. The range the bend uses
        // is the two together.
        uint8_t PitchBendRangeMsb{ 2 };
        uint8_t PitchBendRangeLsb{ 0 };
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
        // The channel map is at General MIDI power-up defaults from here, because the transport
        // answers MIDI-CI out of the engine long before any audio device is opened.
        SynthEngine() noexcept;

        // The collection must outlive the engine.
        bool Initialize(_In_ const DlsCollection* collection, _In_ const SynthConfig& config);

        void NoteOn(_In_ uint8_t channel, _In_ uint8_t note, _In_ uint16_t velocity);
        void NoteOff(_In_ uint8_t channel, _In_ uint8_t note);

        // MIDI 2.0 Note On carrying the Pitch 7.9 attribute. The note number is then only an index,
        // which Note Off and the per-note messages address, and a sender may rotate it through all
        // 128 values. So on a melodic instrument the pitch chooses the region, the same one a plain
        // note on that key gets; on a drum kit the note number still picks the drum. Either way the
        // sample plays at pitchNoteNumber, which can fall between keys.
        void NoteOnWithPitch(
            _In_ uint8_t channel,
            _In_ uint8_t note,
            _In_ uint16_t velocity,
            _In_ double pitchNoteNumber);

        // Per-note pitch bend, as a normalized -1 to +1 across the channel's bend range. Applies to
        // every sounding note with this channel and note number.
        void PerNotePitchBend(_In_ uint8_t channel, _In_ uint8_t note, _In_ double normalized);

        // Registered Per-Note Controller. Only the controllers this engine can act on are applied;
        // the rest are ignored rather than approximated.
        void PerNoteController(
            _In_ uint8_t channel,
            _In_ uint8_t note,
            _In_ uint8_t controller,
            _In_ uint32_t value);

        // Per-Note Management. Detach keeps a sounding note under its own control when the same
        // note is retriggered; reset returns its per-note controllers to their default.
        void PerNoteManagement(
            _In_ uint8_t channel,
            _In_ uint8_t note,
            _In_ bool detach,
            _In_ bool resetPerNoteControllers);

        void ControlChange(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint8_t value);
        void ProgramChange(_In_ uint8_t channel, _In_ uint8_t program);

        // MIDI 2.0 entry points. The value keeps its full width rather than being reduced to
        // 7 bits, which is the whole reason for using them.
        void ControlChange32(_In_ uint8_t channel, _In_ uint8_t controller, _In_ uint32_t value);
        void PitchBend32(_In_ uint8_t channel, _In_ uint32_t value);

        // MIDI 2.0 Registered Controller 0,1, channel fine tuning, at full resolution. The value is
        // centered on 0x80000000, and the ends are 100 cents flat and 100 cents sharp.
        //
        // Like the MIDI 1.0 data entry for both, this and coarse tuning are ignored while the
        // channel plays a drum kit, and never move a note from one, as General MIDI 2 recommends.
        void SetChannelFineTuning32(_In_ uint8_t channel, _In_ uint32_t value) noexcept;

        // Registered Controller 0,2, channel coarse tuning: semitones, 64 is no change. Applied as
        // pitch rather than as a note offset, like the master tuning.
        void SetChannelCoarseTuning(_In_ uint8_t channel, _In_ uint8_t msb) noexcept;

        // Registered Controller 0,0, pitch bend sensitivity: whole semitones and cents, each seven
        // bits. Ignored while the channel plays a drum kit, as General MIDI 2 recommends.
        void SetPitchBendRange(_In_ uint8_t channel, _In_ uint8_t semitones, _In_ uint8_t cents) noexcept;

        // The instrument a program plays in a bank the addressing convention already chose, falling
        // back the General MIDI way: a missing variation plays the capital tone, and a missing kit
        // the standard kit. MIDI-CI names programs with this, so it names what really plays.
        static _Ret_maybenull_ const DlsInstrument* FindPlayableInstrument(
            _In_ const DlsCollection& collection,
            _In_ uint32_t variationBank,
            _In_ uint8_t program,
            _In_ bool isDrumKit) noexcept;

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

        // Any channel can be a rhythm part. GS says so through a SysEx and nothing else can, so
        // without this the eight kits other than the one on channel 10 are unreachable no matter
        // what a file sends. Resetting the synthesizer puts this back to channel 10 only.
        void SetDrumChannel(_In_ uint8_t channel, _In_ bool isDrumChannel);
        bool IsDrumChannel(_In_ uint8_t channel) const noexcept
        {
            return m_channels[channel & 0x0F].IsDrumChannel;
        }

        // Changing this re-resolves every channel, so a mode chosen mid-song takes effect at once
        // rather than at the next program change.
        void SetBankSelectMode(_In_ BankSelectMode mode);
        BankSelectMode EffectiveBankSelectMode() const noexcept { return m_detectedBankSelectMode; }

        // Reported by the dispatcher when a System On or reset identifies the convention in use.
        // Ignored unless the configured mode is Automatic.
        void NotifyAddressingConvention(_In_ BankSelectMode convention);

        // The customer's own volume trim, in dB, deliberately separate from the GM2 master volume
        // above. That one belongs to the content and is cleared by a System Reset, so sharing it
        // would let any file silently discard what the customer chose. Applied before the limiter,
        // so turning it down buys headroom rather than only scaling what the limiter already did.
        //
        // This is the synthesizer's only volume control: midisrv runs in session 0, so it gets no
        // slider of its own in the Windows Volume Mixer, and exclusive and ASIO output have no
        // Windows mixer in the path at all. The transport sets it from its configuration thread
        // while the audio thread renders, which is why it is atomic.
        void SetUserVolumeDb(_In_ double decibels) noexcept
        {
            m_userVolumeDb.store(
                (std::clamp)(decibels, MinimumUserVolumeDb, MaximumUserVolumeDb), std::memory_order_relaxed);
        }

        double UserVolumeDb() const noexcept { return m_userVolumeDb.load(std::memory_order_relaxed); }

        // Quiet enough to be inaudible at the bottom, and a little make-up gain at the top for a
        // customer whose other applications are much louder.
        static constexpr double MinimumUserVolumeDb{ -60.0 };
        static constexpr double MaximumUserVolumeDb{ 12.0 };

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

        // Fourteen bit, 8192 is centered, full range is plus or minus 100 cents. Neither master
        // tuning moves a note from a drum kit, as General MIDI 2 recommends (sections 4.2 and 4.3).
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

        // Bank and program currently selected on a channel. Read by a worker thread building a
        // property exchange channel list, which tolerates a torn read of an in-flight change.
        SynthChannelState ChannelState(_In_ uint8_t channel) const noexcept
        {
            return m_channels[channel & 0x0F];
        }

    private:
        // Puts every channel back to its power-up default. Separate from SystemReset because the
        // constructor needs it before there are any voices to clear.
        void ResetChannels() noexcept;

        // Translates an incoming bank select into the addressing this sound set actually uses.
        void ResolveBankAddressing(
            _In_ SynthChannelState const& state,
            _Out_ uint32_t& variationBank,
            _Out_ bool& isDrumKit) const noexcept;

        _Ret_maybenull_ const DlsRegion* SelectRegion(
            _In_ const DlsInstrument& instrument,
            _In_ uint8_t note,
            _In_ uint8_t velocity7) const noexcept;

        void ResolveInstrument(_In_ uint8_t channel) noexcept;
        _Ret_maybenull_ SynthVoice* AllocateVoice(_In_ uint8_t channel) noexcept;
        void StartVoice(
            _Inout_ SynthVoice& voice,
            _In_ const DlsRegion& region,
            _In_ const DlsWave& wave,
            _In_ const DlsInstrument& instrument,
            _In_ uint8_t channel,
            _In_ uint8_t note,
            _In_ uint8_t regionKey,
            _In_ uint16_t velocity,
            _In_ double pitchNoteNumber) noexcept;

        void ReleaseVoice(_Inout_ SynthVoice& voice) noexcept;

        // Ends a voice with a short fade rather than an instant cut, which would leave a step in
        // the waveform and be heard as a click.
        void KillVoice(_Inout_ SynthVoice& voice) noexcept;

        // Copies into the voice what its channel and the master controls are now, for a reset
        // about to change them. See SynthVoice::ControlsHeldFromReset.
        void HoldControlsForReset(_Inout_ SynthVoice& voice) const noexcept;

        void UpdateVoiceControl(_Inout_ SynthVoice& voice, _In_ uint32_t frames) noexcept;
        void RenderVoiceBlock(
            _Inout_ SynthVoice& voice,
            _Inout_updates_(frames * 2) float* output,
            _In_ uint32_t frames) noexcept;

        const DlsCollection* m_collection{ nullptr };
        SynthConfig m_config;
        double m_renderSampleRate{ 48000.0 };

        std::vector<SynthVoice> m_voices;
        std::array<SynthChannelState, MidiChannelCount> m_channels;

        uint64_t m_nextStartOrder{ 1 };

        // What Automatic has settled on. Equal to the configured mode when it is not Automatic.
        BankSelectMode m_detectedBankSelectMode{ BankSelectMode::RolandGS };
        uint64_t m_stolenVoiceCount{ 0 };
        uint64_t m_droppedNoteCount{ 0 };
        uint64_t m_clippedSampleCount{ 0 };

        double m_limiterGain{ 1.0 };

        // GM2 device controls, applied on top of the configured master gain.
        double m_masterVolumeDb{ 0.0 };

        // Survives a System Reset on purpose: it is the customer's, not the content's.
        std::atomic<double> m_userVolumeDb{ 0.0 };
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
