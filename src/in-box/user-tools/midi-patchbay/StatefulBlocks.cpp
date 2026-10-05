// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "StatefulBlocks.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t TypeSystem = 0x1;
        constexpr uint32_t TypeMidi1 = 0x2;
        constexpr uint32_t TypeMidi2 = 0x4;

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        // MIDI 2.0 only.
        constexpr uint8_t StatusRegisteredPerNote = 0x0;
        constexpr uint8_t StatusAssignablePerNote = 0x1;
        constexpr uint8_t StatusRegistered = 0x2;
        constexpr uint8_t StatusAssignable = 0x3;
        constexpr uint8_t StatusRelativeRegistered = 0x4;
        constexpr uint8_t StatusRelativeAssignable = 0x5;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;
        constexpr uint8_t StatusPerNoteManagement = 0xF;

        // MIDI 1.0 controllers that select and set a parameter.
        constexpr uint8_t ControllerDataMsb = 6;
        constexpr uint8_t ControllerDataLsb = 38;
        constexpr uint8_t ControllerIncrement = 96;
        constexpr uint8_t ControllerDecrement = 97;
        constexpr uint8_t ControllerAssignableLsb = 98;
        constexpr uint8_t ControllerAssignableMsb = 99;
        constexpr uint8_t ControllerRegisteredLsb = 100;
        constexpr uint8_t ControllerRegisteredMsb = 101;

        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerAllNotesOff = 123;

        // How a selected parameter is packed into one atomic.
        constexpr uint32_t SelectionMsbKnown = 1u << 7;
        constexpr uint32_t SelectionLsbKnown = 1u << 15;
        constexpr uint32_t SelectionAssignable = 1u << 16;
        constexpr uint32_t SelectionDataShift = 17;
        constexpr uint32_t SelectionDataKnown = 1u << 24;

        uint32_t MessageType(_In_ uint32_t word) noexcept { return word >> 28; }
        uint8_t GroupOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 24) & 0x0F); }
        uint8_t StatusOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 20) & 0x0F); }
        uint8_t ChannelOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 16) & 0x0F); }
        uint8_t NoteOf(_In_ uint32_t word) noexcept { return static_cast<uint8_t>((word >> 8) & 0x7F); }

        bool IsChannelVoice(_In_ uint32_t word) noexcept
        {
            return MessageType(word) == TypeMidi1 || MessageType(word) == TypeMidi2;
        }

        // A MIDI 1.0 note on at velocity zero is a note off. A MIDI 2.0 one is a quiet note.
        bool IsNoteOff(_In_ uint32_t word) noexcept
        {
            return IsChannelVoice(word) &&
                (StatusOf(word) == StatusNoteOff ||
                 (MessageType(word) == TypeMidi1 && StatusOf(word) == StatusNoteOn && (word & 0x7F) == 0));
        }

        bool IsNoteOn(_In_ uint32_t word) noexcept
        {
            return IsChannelVoice(word) && StatusOf(word) == StatusNoteOn && !IsNoteOff(word);
        }

        // A control change value on the MIDI 1.0 scale: a MIDI 2.0 one by its top seven bits.
        uint8_t ControlValue7(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept
        {
            if (MessageType(words[0]) == TypeMidi1)
            {
                return static_cast<uint8_t>(words[0] & 0x7F);
            }

            return wordCount >= 2 ? static_cast<uint8_t>(words[1] >> 25) : 0;
        }

        uint8_t SystemStatusOf(_In_ uint32_t word) noexcept
        {
            return static_cast<uint8_t>((word >> 16) & 0xFF);
        }

        // A MIDI 1.0 control change on the same group and channel as the one given.
        uint32_t ControlChange(_In_ uint32_t like, _In_ uint8_t controller, _In_ uint8_t value) noexcept
        {
            return (TypeMidi1 << 28) | (static_cast<uint32_t>(GroupOf(like)) << 24) |
                (static_cast<uint32_t>(StatusControlChange) << 20) | (static_cast<uint32_t>(ChannelOf(like)) << 16) |
                (static_cast<uint32_t>(controller & 0x7F) << 8) | (value & 0x7Fu);
        }

        // ------------------------------------------------------------------------- gate

        bool RunGate(
            _In_ GateSettings const& gate,
            _Inout_ BlockState& state,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept
        {
            auto const opens = gate.Open.Matches(words, wordCount);
            auto const closes = gate.Close.Matches(words, wordCount);

            if (opens || closes)
            {
                if (opens && closes)
                {
                    state.Flags.fetch_xor(1u, std::memory_order_relaxed);
                }
                else
                {
                    // Set means it's the other way from how it starts.
                    state.Flags.store(opens != gate.StartsOpen ? 1u : 0u, std::memory_order_relaxed);
                }

                return gate.PassesTriggers;
            }

            // So nothing is left sounding when the gate closes in the middle of a note.
            if (IsNoteOff(words[0]))
            {
                return true;
            }

            auto const flipped = (state.Flags.load(std::memory_order_relaxed) & 1u) != 0;

            return gate.StartsOpen != flipped;
        }

        // ------------------------------------------------------------- (N)RPN selection

        // The parameter a MIDI 1.0 select controller picks, merged into what was selected before.
        // A change between RPN and NRPN forgets the other half, which belonged to the other kind.
        uint32_t Select(_In_ uint32_t selection, _In_ uint8_t controller, _In_ uint8_t value) noexcept
        {
            auto const assignable = controller == ControllerAssignableMsb || controller == ControllerAssignableLsb;
            auto const isMsb = controller == ControllerAssignableMsb || controller == ControllerRegisteredMsb;

            if (((selection & SelectionAssignable) != 0) != assignable)
            {
                selection = assignable ? SelectionAssignable : 0;
            }

            // A new selection has no data yet.
            selection &= ~(SelectionDataKnown | (0x7Fu << SelectionDataShift));

            if (isMsb)
            {
                selection = (selection & ~0xFFu) | SelectionMsbKnown | (value & 0x7Fu);
            }
            else
            {
                selection = (selection & ~0xFF00u) | SelectionLsbKnown | (static_cast<uint32_t>(value & 0x7F) << 8);
            }

            return selection;
        }

        bool SelectionComplete(_In_ uint32_t selection) noexcept
        {
            return (selection & SelectionMsbKnown) != 0 && (selection & SelectionLsbKnown) != 0;
        }

        uint8_t SelectedBank(_In_ uint32_t selection) noexcept { return static_cast<uint8_t>(selection & 0x7F); }
        uint8_t SelectedIndex(_In_ uint32_t selection) noexcept { return static_cast<uint8_t>((selection >> 8) & 0x7F); }
        bool SelectedAssignable(_In_ uint32_t selection) noexcept { return (selection & SelectionAssignable) != 0; }

        // 127 and 127 is the null parameter, which turns data entry off on the receiver.
        bool SelectionIsNull(_In_ uint32_t selection) noexcept
        {
            return SelectedBank(selection) == 0x7F && SelectedIndex(selection) == 0x7F;
        }

        bool IsSelectController(_In_ uint8_t controller) noexcept
        {
            return controller == ControllerRegisteredMsb || controller == ControllerRegisteredLsb ||
                controller == ControllerAssignableMsb || controller == ControllerAssignableLsb;
        }

        bool IsDataController(_In_ uint8_t controller) noexcept
        {
            return controller == ControllerDataMsb || controller == ControllerDataLsb ||
                controller == ControllerIncrement || controller == ControllerDecrement;
        }

        std::atomic<uint32_t>& SelectionSlot(_Inout_ BlockState& state, _In_ uint32_t word) noexcept
        {
            return state.Parameters[static_cast<size_t>(GroupOf(word)) * 16 + ChannelOf(word)];
        }

        bool IsNativeParameter(_In_ uint32_t word) noexcept
        {
            auto const status = StatusOf(word);

            return MessageType(word) == TypeMidi2 && status >= StatusRegistered && status <= StatusRelativeAssignable;
        }

        // ---------------------------------------------------------------- (N)RPN filter

        bool RunParameterFilter(
            _In_ ParameterFilterSettings const& filter,
            _Inout_ BlockState& state,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept
        {
            if (filter.Parameters.empty())
            {
                return true;
            }

            auto const passes = [&filter](bool assignable, uint8_t bank, uint8_t index)
                {
                    auto const listed = std::any_of(filter.Parameters.begin(), filter.Parameters.end(),
                        [&](ParameterMatch const& match) { return match.Matches(assignable, bank, index); });

                    return listed == (filter.Action == FilterAction::LetThrough);
                };

            auto const word = words[0];

            if (IsNativeParameter(word))
            {
                auto const status = StatusOf(word);
                auto const assignable = status == StatusAssignable || status == StatusRelativeAssignable;

                return passes(assignable, static_cast<uint8_t>((word >> 8) & 0x7F), static_cast<uint8_t>(word & 0x7F));
            }

            if (!IsChannelVoice(word) || StatusOf(word) != StatusControlChange)
            {
                return true;
            }

            auto const controller = NoteOf(word);
            auto& slot = SelectionSlot(state, word);

            // Selecting does nothing on its own, so it always goes through and the data decides.
            if (IsSelectController(controller))
            {
                slot.store(Select(slot.load(std::memory_order_relaxed), controller, ControlValue7(words, wordCount)),
                    std::memory_order_relaxed);
                return true;
            }

            if (!IsDataController(controller))
            {
                return true;
            }

            auto const selection = slot.load(std::memory_order_relaxed);

            // Data for a parameter this hasn't seen selected can't be judged.
            if (!SelectionComplete(selection) || SelectionIsNull(selection))
            {
                return true;
            }

            return passes(SelectedAssignable(selection), SelectedBank(selection), SelectedIndex(selection));
        }

        // ------------------------------------------------------------- (N)RPN transform

        ParameterMapRow const* FindRow(
            _In_ ParameterTransformSettings const& transform,
            _In_ bool assignable,
            _In_ uint8_t bank,
            _In_ uint8_t index) noexcept
        {
            for (auto const& row : transform.Rows)
            {
                if (row.From.Matches(assignable, bank, index))
                {
                    return &row;
                }
            }

            return nullptr;
        }

        struct ParameterAddress
        {
            bool Assignable{ false };
            uint8_t Bank{ 0 };
            uint8_t Index{ 0 };

            bool operator==(ParameterAddress const&) const = default;
        };

        ParameterAddress Moved(_In_ ParameterMapRow const& row, _In_ ParameterAddress const& from) noexcept
        {
            ParameterAddress to{ from };

            if (row.ToKind != ParameterKind::Either)
            {
                to.Assignable = row.ToKind == ParameterKind::Assignable;
            }

            if (row.ToBank >= 0)
            {
                to.Bank = static_cast<uint8_t>(row.ToBank & 0x7F);
            }

            if (row.ToIndex >= 0)
            {
                to.Index = static_cast<uint8_t>(row.ToIndex & 0x7F);
            }

            return to;
        }

        uint32_t Shape14(_In_ ValueShape const& shape, _In_ uint32_t value) noexcept
        {
            auto const shaped = shape.ShapeUnit(static_cast<double>(value & 0x3FFF) / 16383.0);

            return static_cast<uint32_t>(std::clamp(std::lround(shaped * 16383.0), 0L, 16383L));
        }

        bool RunParameterTransform(
            _In_ ParameterTransformSettings const& transform,
            _Inout_ BlockState& state,
            _Inout_updates_(wordCount) uint32_t* words,
            _In_ uint8_t wordCount,
            _Inout_ StageOutput& output) noexcept
        {
            if (transform.Rows.empty())
            {
                return true;
            }

            auto const word = words[0];

            // MIDI 2.0 carries the whole address and value in one message.
            if (IsNativeParameter(word))
            {
                auto const status = StatusOf(word);
                auto const relative = status >= StatusRelativeRegistered;

                ParameterAddress const from{
                    status == StatusAssignable || status == StatusRelativeAssignable,
                    static_cast<uint8_t>((word >> 8) & 0x7F),
                    static_cast<uint8_t>(word & 0x7F) };

                auto const* row = FindRow(transform, from.Assignable, from.Bank, from.Index);

                if (row == nullptr)
                {
                    return true;
                }

                auto const to = Moved(*row, from);

                auto const newStatus = relative
                    ? (to.Assignable ? StatusRelativeAssignable : StatusRelativeRegistered)
                    : (to.Assignable ? StatusAssignable : StatusRegistered);

                words[0] = (word & 0xFF0F0000u) | (static_cast<uint32_t>(newStatus) << 20) |
                    (static_cast<uint32_t>(to.Bank) << 8) | to.Index;

                // A relative value is a step up or down, not a place on a scale.
                if (!relative && wordCount >= 2 && !row->Shape.ChangesNothing())
                {
                    words[1] = row->Shape.Shape32(words[1]);
                }

                return true;
            }

            // MIDI 1.0 spreads the same thing over several control changes. A MIDI 2.0 control
            // change on these numbers is left alone: it has no MIDI 1.0 sequence to follow.
            if (MessageType(word) != TypeMidi1 || StatusOf(word) != StatusControlChange)
            {
                return true;
            }

            auto const controller = NoteOf(word);
            auto const value = static_cast<uint8_t>(word & 0x7F);
            auto& slot = SelectionSlot(state, word);

            if (IsSelectController(controller))
            {
                auto const selection = Select(slot.load(std::memory_order_relaxed), controller, value);
                slot.store(selection, std::memory_order_relaxed);

                // Sent as it is until the second half arrives, then put right with both halves.
                auto const isLsb = controller == ControllerRegisteredLsb || controller == ControllerAssignableLsb;

                if (!isLsb || !SelectionComplete(selection) || SelectionIsNull(selection))
                {
                    return true;
                }

                ParameterAddress const from{ SelectedAssignable(selection), SelectedBank(selection), SelectedIndex(selection) };
                auto const* row = FindRow(transform, from.Assignable, from.Bank, from.Index);

                if (row == nullptr)
                {
                    return true;
                }

                auto const to = Moved(*row, from);

                if (to == from)
                {
                    return true;
                }

                uint32_t const msb = ControlChange(word,
                    to.Assignable ? ControllerAssignableMsb : ControllerRegisteredMsb, to.Bank);
                uint32_t const lsb = ControlChange(word,
                    to.Assignable ? ControllerAssignableLsb : ControllerRegisteredLsb, to.Index);

                output.Add(&msb, 1, EveryEdge);
                output.Add(&lsb, 1, EveryEdge);
                return true;
            }

            if (controller != ControllerDataMsb && controller != ControllerDataLsb)
            {
                return true;
            }

            auto selection = slot.load(std::memory_order_relaxed);

            if (!SelectionComplete(selection) || SelectionIsNull(selection))
            {
                return true;
            }

            auto const* row = FindRow(transform, SelectedAssignable(selection), SelectedBank(selection), SelectedIndex(selection));

            if (row == nullptr || row->Shape.ChangesNothing())
            {
                return true;
            }

            if (controller == ControllerDataMsb)
            {
                // Remembered, so a fine adjustment that follows is shaped with it.
                selection = (selection & ~(0x7Fu << SelectionDataShift)) | SelectionDataKnown |
                    (static_cast<uint32_t>(value) << SelectionDataShift);
                slot.store(selection, std::memory_order_relaxed);

                words[0] = (word & ~0x7Fu) | (Shape14(row->Shape, static_cast<uint32_t>(value) << 7) >> 7);
                return true;
            }

            if ((selection & SelectionDataKnown) == 0)
            {
                return true;
            }

            // The coarse half goes again, so the receiver has both halves of the shaped value.
            auto const coarse = (selection >> SelectionDataShift) & 0x7Fu;
            auto const shaped = Shape14(row->Shape, (coarse << 7) | value);

            uint32_t const msb = ControlChange(word, ControllerDataMsb, static_cast<uint8_t>(shaped >> 7));
            uint32_t const lsb = ControlChange(word, ControllerDataLsb, static_cast<uint8_t>(shaped & 0x7F));

            output.Add(&msb, 1, EveryEdge);
            output.Add(&lsb, 1, EveryEdge);
            return true;
        }

        // ------------------------------------------------------------ note distributor

        class VoiceLockGuard
        {
        public:
            explicit VoiceLockGuard(_Inout_ std::atomic_flag& flag) noexcept : m_flag(flag)
            {
                while (m_flag.test_and_set(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
            }

            ~VoiceLockGuard() noexcept
            {
                m_flag.clear(std::memory_order_release);
            }

            VoiceLockGuard(VoiceLockGuard const&) = delete;
            VoiceLockGuard& operator=(VoiceLockGuard const&) = delete;

        private:
            std::atomic_flag& m_flag;
        };

        int32_t FindVoice(_In_ BlockState const& state, _In_ uint8_t group, _In_ uint8_t channel, _In_ uint8_t note) noexcept
        {
            for (uint32_t i = 0; i < state.VoiceCount; i++)
            {
                auto const& voice = state.Voices[i];

                if (voice.Sounding && voice.Group == group && voice.Channel == channel && voice.Note == note)
                {
                    return static_cast<int32_t>(i);
                }
            }

            return -1;
        }

        int32_t FreeVoice(_In_ BlockState const& state, _In_ uint32_t from) noexcept
        {
            for (uint32_t i = 0; i < state.VoiceCount; i++)
            {
                auto const candidate = (from + i) % state.VoiceCount;

                if (!state.Voices[candidate].Sounding)
                {
                    return static_cast<int32_t>(candidate);
                }
            }

            return -1;
        }

        // A note off for what a voice is playing, in the protocol it was played in.
        void AddNoteOff(_Inout_ StageOutput& output, _In_ Voice const& voice, _In_ int32_t edge) noexcept
        {
            uint32_t words[2]{};

            words[0] = ((voice.Midi2 ? TypeMidi2 : TypeMidi1) << 28) | (static_cast<uint32_t>(voice.Group) << 24) |
                (static_cast<uint32_t>(StatusNoteOff) << 20) | (static_cast<uint32_t>(voice.Channel) << 16) |
                (static_cast<uint32_t>(voice.Note) << 8);

            output.Add(words, voice.Midi2 ? 2 : 1, edge);
        }

        // Which voice a new note goes to, or -1 to leave it out. Called with the lock held.
        int32_t ChooseVoice(_In_ DistributionMode mode, _In_ BlockState const& state, _In_ uint8_t note) noexcept
        {
            auto const free = FreeVoice(state, mode == DistributionMode::TakeTurns ? state.NextVoice : 0);

            if (free >= 0)
            {
                return free;
            }

            switch (mode)
            {
            case DistributionMode::TakeTurns:
                return static_cast<int32_t>(state.NextVoice % state.VoiceCount);

            case DistributionMode::FirstFree:
            {
                uint32_t oldest{ 0 };

                for (uint32_t i = 1; i < state.VoiceCount; i++)
                {
                    if (state.Voices[i].Age < state.Voices[oldest].Age)
                    {
                        oldest = i;
                    }
                }

                return static_cast<int32_t>(oldest);
            }

            case DistributionMode::HighestNotes:
            case DistributionMode::LowestNotes:
            {
                auto const keepHigh = mode == DistributionMode::HighestNotes;
                uint32_t weakest{ 0 };

                for (uint32_t i = 1; i < state.VoiceCount; i++)
                {
                    auto const playing = state.Voices[i].Note;

                    if (keepHigh ? playing < state.Voices[weakest].Note : playing > state.Voices[weakest].Note)
                    {
                        weakest = i;
                    }
                }

                auto const replaces = keepHigh ? note > state.Voices[weakest].Note : note < state.Voices[weakest].Note;

                return replaces ? static_cast<int32_t>(weakest) : -1;
            }

            default:
                return -1;
            }
        }

        bool RunNoteDistributor(
            _In_ NoteDistributorSettings const& settings,
            _Inout_ BlockState& state,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount,
            _In_ uint32_t edgeCount,
            _Inout_ StageOutput& output) noexcept
        {
            auto const voices = (std::min)(edgeCount, static_cast<uint32_t>(MaximumVoices));

            if (voices == 0)
            {
                return false;
            }

            auto const word = words[0];

            // Everything that isn't a channel voice message is for every voice.
            if (!IsChannelVoice(word))
            {
                return true;
            }

            auto const isMidi2 = MessageType(word) == TypeMidi2;
            auto const status = StatusOf(word);
            auto const group = GroupOf(word);
            auto const channel = ChannelOf(word);
            auto const note = NoteOf(word);

            VoiceLockGuard const guard{ state.VoiceLock };

            // A connection added or taken away: the voices are counted again from nothing.
            if (state.VoiceCount != voices)
            {
                state.Voices.fill(Voice{});
                state.VoiceCount = voices;
                state.NextVoice = 0;
                state.LatestVoice = -1;
            }

            if (IsNoteOn(word))
            {
                auto target = FindVoice(state, group, channel, note);

                if (target < 0)
                {
                    target = ChooseVoice(settings.Mode, state, note);

                    if (target < 0)
                    {
                        return false;
                    }

                    auto const& taken = state.Voices[static_cast<size_t>(target)];

                    if (taken.Sounding)
                    {
                        AddNoteOff(output, taken, target);
                    }

                    if (settings.Mode == DistributionMode::TakeTurns)
                    {
                        state.NextVoice = (static_cast<uint32_t>(target) + 1) % voices;
                    }
                }

                state.Voices[static_cast<size_t>(target)] = Voice{ true, isMidi2, group, channel, note, ++state.VoiceClock };
                state.LatestVoice = target;

                output.Add(words, wordCount, target);
                return true;
            }

            if (IsNoteOff(word))
            {
                auto const target = FindVoice(state, group, channel, note);

                // A note that was left out, or one already cut short by a note that took its voice.
                if (target < 0)
                {
                    return false;
                }

                state.Voices[static_cast<size_t>(target)].Sounding = false;

                output.Add(words, wordCount, target);
                return true;
            }

            auto const perNote = status == StatusPolyPressure ||
                (isMidi2 && (status == StatusRegisteredPerNote || status == StatusAssignablePerNote ||
                             status == StatusPerNotePitchBend || status == StatusPerNoteManagement));

            if (perNote)
            {
                auto const target = FindVoice(state, group, channel, note);

                if (target < 0)
                {
                    return false;
                }

                output.Add(words, wordCount, target);
                return true;
            }

            bool toEveryVoice{ true };

            if (status == StatusControlChange)
            {
                // Channel mode messages always reach every voice, and all notes off frees them.
                if (note >= ControllerAllSoundOff)
                {
                    if (note == ControllerAllSoundOff || note == ControllerAllNotesOff)
                    {
                        for (uint32_t i = 0; i < voices; i++)
                        {
                            auto& voice = state.Voices[i];

                            if (voice.Group == group && voice.Channel == channel)
                            {
                                voice.Sounding = false;
                            }
                        }
                    }

                    return true;
                }

                toEveryVoice = settings.ControlChangesToEveryVoice;
            }
            else if (isMidi2 && status >= StatusRegistered && status <= StatusRelativeAssignable)
            {
                toEveryVoice = settings.ControlChangesToEveryVoice;
            }
            else if (status == StatusChannelPressure)
            {
                toEveryVoice = settings.ChannelPressureToEveryVoice;
            }
            else if (status == StatusPitchBend)
            {
                toEveryVoice = settings.PitchBendToEveryVoice;
            }

            if (!toEveryVoice && state.LatestVoice >= 0)
            {
                output.Add(words, wordCount, state.LatestVoice);
            }

            return true;
        }
    }

    _Use_decl_annotations_
    bool ParameterMatch::Matches(bool assignable, uint8_t bank, uint8_t index) const noexcept
    {
        if (Kind != ParameterKind::Either && (Kind == ParameterKind::Assignable) != assignable)
        {
            return false;
        }

        return (Bank < 0 || Bank == bank) && (Index < 0 || Index == index);
    }

    _Use_decl_annotations_
    bool GateTrigger::Matches(uint32_t const* words, uint8_t wordCount) const noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        auto const word = words[0];
        auto const type = MessageType(word);

        // Utility and stream messages have no group.
        auto const hasGroup = type != 0x0 && type != 0xF;

        if (Group >= 0 && (!hasGroup || GroupOf(word) != static_cast<uint8_t>(Group)))
        {
            return false;
        }

        switch (Kind)
        {
        case GateTriggerKind::Words:
        {
            if (WordCount == 0 || wordCount < WordCount)
            {
                return false;
            }

            for (uint8_t i = 0; i < WordCount && i < MaximumUmpWords; i++)
            {
                // The group is the Group setting's to decide, not the words'.
                auto const mask = i == 0 && hasGroup ? 0xF0FFFFFFu : 0xFFFFFFFFu;

                if (((words[i] ^ Words[i]) & mask) != 0)
                {
                    return false;
                }
            }

            return true;
        }

        case GateTriggerKind::Start:
            return type == TypeSystem && SystemStatusOf(word) == 0xFA;

        case GateTriggerKind::Continue:
            return type == TypeSystem && SystemStatusOf(word) == 0xFB;

        case GateTriggerKind::Stop:
            return type == TypeSystem && SystemStatusOf(word) == 0xFC;

        default:
            break;
        }

        if (!IsChannelVoice(word) || (Channel >= 0 && ChannelOf(word) != static_cast<uint8_t>(Channel)))
        {
            return false;
        }

        auto const status = StatusOf(word);
        int32_t number{ NoteOf(word) };

        switch (Kind)
        {
        case GateTriggerKind::NoteOn:
            if (!IsNoteOn(word))
            {
                return false;
            }
            break;

        case GateTriggerKind::NoteOff:
            if (!IsNoteOff(word))
            {
                return false;
            }
            break;

        case GateTriggerKind::ControlChange:
        {
            if (status != StatusControlChange)
            {
                return false;
            }

            auto const value = ControlValue7(words, wordCount);

            if ((Test == GateValueTest::AtLeast && value < Value) || (Test == GateValueTest::Below && value >= Value))
            {
                return false;
            }
            break;
        }

        case GateTriggerKind::ProgramChange:
            if (status != StatusProgramChange)
            {
                return false;
            }

            // MIDI 2.0 carries the program in the second word.
            if (MessageType(word) == TypeMidi2)
            {
                number = wordCount >= 2 ? static_cast<int32_t>((words[1] >> 24) & 0x7F) : -1;
            }
            break;

        default:
            return false;
        }

        return Number < 0 || Number == number;
    }

    _Use_decl_annotations_
    void StageOutput::Add(uint32_t const* words, uint8_t wordCount, int32_t edge) noexcept
    {
        if (words == nullptr || wordCount == 0 || wordCount > MaximumStageWords || Count >= Messages.size())
        {
            return;
        }

        auto& message = Messages[Count++];

        std::copy_n(words, wordCount, message.Words.begin());
        message.Count = wordCount;
        message.Edge = edge;
    }

    _Use_decl_annotations_
    bool IsStatefulBlock(BlockKind kind) noexcept
    {
        switch (kind)
        {
        case BlockKind::ClockDivider:
        case BlockKind::ParameterFilter:
        case BlockKind::ParameterTransform:
        case BlockKind::NoteDistributor:
        case BlockKind::Gate:
        case BlockKind::CiResponder:
        case BlockKind::CiFilter:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    void PrepareBlockState(BlockKind kind, BlockState& state)
    {
        if (kind == BlockKind::CiResponder && state.Ci == nullptr)
        {
            state.Ci = std::make_unique<CiResponderState>();
        }
    }

    _Use_decl_annotations_
    bool RunStatefulBlock(
        BlockKind kind,
        BlockSettings const& settings,
        BlockState& state,
        uint32_t* words,
        uint8_t wordCount,
        uint32_t edgeCount,
        StageOutput& output) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        switch (kind)
        {
        case BlockKind::ClockDivider:
            return DivideClock(settings.ClockDivision, state.Count, words, wordCount);

        case BlockKind::ParameterFilter:
            return RunParameterFilter(settings.ParameterFilter, state, words, wordCount);

        case BlockKind::ParameterTransform:
            return RunParameterTransform(settings.ParameterTransform, state, words, wordCount, output);

        case BlockKind::NoteDistributor:
            return RunNoteDistributor(settings.Distributor, state, words, wordCount, edgeCount, output);

        case BlockKind::Gate:
            return RunGate(settings.Gate, state, words, wordCount);

        default:
            return ProcessBlock(kind, settings, words, wordCount);
        }
    }
}
