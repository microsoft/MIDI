// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ProcessingBlockTests.h"
#include "TestMessages.h"

#include "ProcessingBlock.h"

#include <set>
#include <string>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOff = 0x8;
    constexpr uint8_t NoteOn = 0x9;
    constexpr uint8_t PolyPressure = 0xA;
    constexpr uint8_t ControlChange = 0xB;
    constexpr uint8_t ProgramChange = 0xC;
    constexpr uint8_t ChannelPressure = 0xD;
    constexpr uint8_t PitchBend = 0xE;

    bool Run(BlockKind kind, BlockSettings const& settings, Message& message)
    {
        return ProcessBlock(kind, settings, message.Words.data(), message.Count);
    }

    bool Passes(BlockKind kind, BlockSettings const& settings, Message message)
    {
        return Run(kind, settings, message);
    }

    std::vector<Message> SampleMessages()
    {
        return {
            Midi1(0, NoteOn, 0, 60, 100),
            Midi1(1, NoteOff, 3, 61, 0),
            Midi1(0, NoteOn, 9, 36, 0),
            Midi1(2, ControlChange, 15, 7, 90),
            Midi1(0, ControlChange, 0, 0, 3),
            Midi1(0, ControlChange, 0, 32, 1),
            Midi1(0, ProgramChange, 0, 5, 0),
            Midi1(0, PolyPressure, 1, 60, 50),
            Midi1(0, ChannelPressure, 1, 70, 0),
            Midi1(0, PitchBend, 0, 0, 64),
            Midi2(0, NoteOn, 0, 60, 0, 0x80000000u),
            Midi2(3, NoteOn, 2, 64, 3, (0xFFFFu << 16) | (64u * 512u)),
            Midi2(0, ControlChange, 4, 74, 0, 0x12345678u),
            Midi2(0, ProgramChange, 0, 0, 1, (12u << 24) | (1u << 8) | 2u),
            Midi2(0, ChannelPressure, 0, 0, 0, 0x40000000u),
            System(0, 0xF8),
            System(5, 0xFE),
            System(0, 0xF2, 10, 20),
            Utility(),
            SysEx7(1),
            Stream(),
        };
    }
}

void ProcessingBlockTests::EveryKindHasItsOwnKey()
{
    std::set<std::wstring> keys{};

    for (auto const kind : AllBlockKinds)
    {
        auto const key = BlockKindKey(kind);

        VERIFY_IS_FALSE(key.empty());
        VERIFY_IS_TRUE(keys.insert(std::wstring{ key }).second);

        auto const back = BlockKindFromKey(key);

        VERIFY_IS_TRUE(back.has_value());
        VERIFY_IS_TRUE(back.value() == kind);
    }

    VERIFY_ARE_EQUAL(BlockKindCount, keys.size());
    VERIFY_IS_FALSE(BlockKindFromKey(L"NoteFilter").has_value());
    VERIFY_IS_FALSE(BlockKindFromKey(L"").has_value());
}

void ProcessingBlockTests::ANewBlockChangesNothing()
{
    for (auto const kind : AllBlockKinds)
    {
        // A generator takes nothing in, so there is nothing for it to leave alone.
        if (IsGenerator(kind))
        {
            continue;
        }

        auto const settings = DefaultBlockSettings(kind);

        // A throttle with no limit, or a clock divider dividing by one, would be pointless to
        // add, so a new one starts out doing something.
        auto const startsDoingSomething = kind == BlockKind::Throttle || kind == BlockKind::ClockDivider;

        VERIFY_ARE_EQUAL(!startsDoingSomething, BlockChangesNothing(kind, settings));

        for (auto const& original : SampleMessages())
        {
            auto message = original;

            VERIFY_IS_TRUE(Run(kind, settings, message));
            VERIFY_IS_TRUE(message == original);
        }
    }
}

void ProcessingBlockTests::NoteFilterPicksNotesOnly()
{
    auto settings = DefaultBlockSettings(BlockKind::NoteFilter);
    settings.Values.Mode = ValueSetMode::Range;
    settings.Values.Lowest = 48;
    settings.Values.Highest = 72;

    VERIFY_IS_FALSE(BlockChangesNothing(BlockKind::NoteFilter, settings));

    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 0, 47, 100)));
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 0, 48, 100)));
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOff, 0, 72, 0)));
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOff, 0, 73, 0)));
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi1(0, PolyPressure, 0, 30, 10)));
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi2(0, NoteOn, 0, 40, 0, 0x80000000u)));

    // Per-note controllers carry a note in MIDI 2.0.
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi2(0, 0x0, 0, 40, 7, 0)));

    // Everything without a note goes through untouched.
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, ControlChange, 0, 20, 10)));
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, ProgramChange, 0, 20, 0)));
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, System(0, 0xF8)));
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, SysEx7(0)));

    settings.Values.Action = FilterAction::KeepOut;

    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 0, 47, 100)));
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));

    settings.Values.Action = FilterAction::LetThrough;
    settings.Values.Mode = ValueSetMode::One;
    settings.Values.One = 60;

    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 0, 61, 100)));

    settings.Values.Mode = ValueSetMode::List;
    settings.Values.List[36] = true;
    settings.Values.List[38] = true;

    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 9, 36, 100)));
    VERIFY_IS_FALSE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 9, 37, 100)));
    VERIFY_IS_TRUE(Passes(BlockKind::NoteFilter, settings, Midi1(0, NoteOn, 9, 38, 100)));

    // Keeping out an empty list keeps out nothing.
    settings.Values.List.fill(false);
    settings.Values.Action = FilterAction::KeepOut;

    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::NoteFilter, settings));
}

void ProcessingBlockTests::ControlChangeFilterPicksControllersOnly()
{
    auto settings = DefaultBlockSettings(BlockKind::ControlChangeFilter);

    // The mod wheel is what a new control change filter offers first.
    VERIFY_ARE_EQUAL(1, static_cast<int>(settings.Values.One));

    settings.Values.Mode = ValueSetMode::One;

    VERIFY_IS_TRUE(Passes(BlockKind::ControlChangeFilter, settings, Midi1(0, ControlChange, 0, 1, 64)));
    VERIFY_IS_FALSE(Passes(BlockKind::ControlChangeFilter, settings, Midi1(0, ControlChange, 0, 7, 64)));
    VERIFY_IS_TRUE(Passes(BlockKind::ControlChangeFilter, settings, Midi2(0, ControlChange, 0, 1, 0, 0x80000000u)));
    VERIFY_IS_FALSE(Passes(BlockKind::ControlChangeFilter, settings, Midi2(0, ControlChange, 0, 2, 0, 0x80000000u)));

    // A note with the same number as the controller is not a controller.
    VERIFY_IS_TRUE(Passes(BlockKind::ControlChangeFilter, settings, Midi1(0, NoteOn, 0, 7, 64)));
    VERIFY_IS_TRUE(Passes(BlockKind::ControlChangeFilter, settings, System(0, 0xFA)));

    settings.Values.Action = FilterAction::KeepOut;
    settings.Values.Mode = ValueSetMode::Range;
    settings.Values.Lowest = 100;
    settings.Values.Highest = 127;

    VERIFY_IS_TRUE(Passes(BlockKind::ControlChangeFilter, settings, Midi1(0, ControlChange, 0, 99, 0)));
    VERIFY_IS_FALSE(Passes(BlockKind::ControlChangeFilter, settings, Midi1(0, ControlChange, 0, 121, 0)));
}

void ProcessingBlockTests::VelocityFilterNeverKeepsOutANoteOff()
{
    auto settings = DefaultBlockSettings(BlockKind::VelocityFilter);
    settings.Velocities.LowestHundredths = 0;
    settings.Velocities.HighestHundredths = 5000;

    VERIFY_IS_FALSE(BlockChangesNothing(BlockKind::VelocityFilter, settings));

    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOn, 0, 60, 40)));
    VERIFY_IS_FALSE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));

    // A MIDI 1.0 note on at velocity 0 is a note off, and so is a note off at any velocity.
    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOn, 0, 60, 0)));
    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOff, 0, 60, 127)));
    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi2(0, NoteOff, 0, 60, 0, 0xFFFF0000u)));

    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi2(0, NoteOn, 0, 60, 0, 0x40000000u)));
    VERIFY_IS_FALSE(Passes(BlockKind::VelocityFilter, settings, Midi2(0, NoteOn, 0, 60, 0, 0xFFFF0000u)));

    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, ControlChange, 0, 1, 127)));

    settings.Velocities.Action = FilterAction::KeepOut;

    VERIFY_IS_FALSE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOn, 0, 60, 40)));
    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));
    VERIFY_IS_TRUE(Passes(BlockKind::VelocityFilter, settings, Midi1(0, NoteOn, 0, 60, 0)));
}

void ProcessingBlockTests::GroupFilterLetsGrouplessMessagesThrough()
{
    auto settings = DefaultBlockSettings(BlockKind::GroupFilter);
    settings.Groups.fill(false);
    settings.Groups[1] = true;

    VERIFY_IS_FALSE(BlockChangesNothing(BlockKind::GroupFilter, settings));

    VERIFY_IS_TRUE(Passes(BlockKind::GroupFilter, settings, Midi1(1, NoteOn, 0, 60, 100)));
    VERIFY_IS_FALSE(Passes(BlockKind::GroupFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));
    VERIFY_IS_FALSE(Passes(BlockKind::GroupFilter, settings, System(2, 0xF8)));
    VERIFY_IS_TRUE(Passes(BlockKind::GroupFilter, settings, SysEx7(1)));

    VERIFY_IS_TRUE(Passes(BlockKind::GroupFilter, settings, Utility()));
    VERIFY_IS_TRUE(Passes(BlockKind::GroupFilter, settings, Stream()));
}

void ProcessingBlockTests::GroupMapMovesOnlyTheGroupsItNames()
{
    auto settings = DefaultBlockSettings(BlockKind::GroupMap);
    settings.GroupMap[0] = 3;

    VERIFY_IS_FALSE(BlockChangesNothing(BlockKind::GroupMap, settings));

    auto moved = Midi1(0, NoteOn, 5, 60, 100);
    VERIFY_IS_TRUE(Run(BlockKind::GroupMap, settings, moved));
    VERIFY_IS_TRUE(moved == Midi1(3, NoteOn, 5, 60, 100));

    auto untouched = Midi1(2, NoteOn, 5, 60, 100);
    VERIFY_IS_TRUE(Run(BlockKind::GroupMap, settings, untouched));
    VERIFY_IS_TRUE(untouched == Midi1(2, NoteOn, 5, 60, 100));

    auto utility = Utility();
    VERIFY_IS_TRUE(Run(BlockKind::GroupMap, settings, utility));
    VERIFY_IS_TRUE(utility == Utility());

    // Mapping a group to itself is the same as not mapping it.
    settings.GroupMap[0] = 0;
    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::GroupMap, settings));
}

void ProcessingBlockTests::MessageMaskLooksOnlyAtItsOwnSize()
{
    auto settings = DefaultBlockSettings(BlockKind::MessageMaskFilter);

    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::MessageMaskFilter, settings));

    // Note ons: bits 23 to 20 of the first word are 9.
    MaskCondition condition{};
    condition.Word = 0;
    condition.HighBit = 23;
    condition.LowBit = 20;
    condition.Match = MaskMatch::Exactly;
    condition.Value = 0x9;

    settings.Mask.WordCount = 1;
    settings.Mask.Action = FilterAction::KeepOut;
    settings.Mask.Conditions.push_back(condition);

    VERIFY_IS_FALSE(BlockChangesNothing(BlockKind::MessageMaskFilter, settings));

    VERIFY_IS_FALSE(Passes(BlockKind::MessageMaskFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));
    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi1(0, NoteOff, 0, 60, 0)));

    // A two-word note on is not this filter's business.
    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi2(0, NoteOn, 0, 60, 0, 0x80000000u)));

    settings.Mask.Action = FilterAction::LetThrough;

    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi1(0, NoteOn, 0, 60, 100)));
    VERIFY_IS_FALSE(Passes(BlockKind::MessageMaskFilter, settings, Midi1(0, NoteOff, 0, 60, 0)));
    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi2(0, NoteOff, 0, 60, 0, 0)));
}

void ProcessingBlockTests::MessageMaskNeedsEveryPlaceToMatch()
{
    auto settings = DefaultBlockSettings(BlockKind::MessageMaskFilter);
    settings.Mask.WordCount = 2;
    settings.Mask.Action = FilterAction::KeepOut;

    // MIDI 2.0 control changes...
    MaskCondition status{};
    status.Word = 0;
    status.HighBit = 23;
    status.LowBit = 20;
    status.Match = MaskMatch::Exactly;
    status.Value = 0xB;

    // ...on controllers 70 to 79...
    MaskCondition controller{};
    controller.Word = 0;
    controller.HighBit = 14;
    controller.LowBit = 8;
    controller.Match = MaskMatch::Between;
    controller.Lowest = 70;
    controller.Highest = 79;

    // ...whose value has its top bit set.
    MaskCondition top{};
    top.Word = 1;
    top.HighBit = 31;
    top.LowBit = 31;
    top.Match = MaskMatch::AnyOf;
    top.Values = { 1 };

    settings.Mask.Conditions = { status, controller, top };

    VERIFY_IS_FALSE(Passes(BlockKind::MessageMaskFilter, settings, Midi2(0, ControlChange, 0, 74, 0, 0x80000000u)));
    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi2(0, ControlChange, 0, 74, 0, 0x7FFFFFFFu)));
    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi2(0, ControlChange, 0, 80, 0, 0x80000000u)));
    VERIFY_IS_TRUE(Passes(BlockKind::MessageMaskFilter, settings, Midi2(0, NoteOn, 0, 74, 0, 0x80000000u)));
}

void ProcessingBlockTests::ATransformBlockUsesOnlyItsOwnPart()
{
    // A hand edited transpose block that also carries a channel map.
    json::JsonObject object{};
    object.SetNamedValue(L"transposeSemitones", json::JsonValue::CreateNumberValue(12));

    json::JsonArray channelMap{};
    json::JsonObject entry{};
    entry.SetNamedValue(L"from", json::JsonValue::CreateNumberValue(0));
    entry.SetNamedValue(L"to", json::JsonValue::CreateNumberValue(5));
    channelMap.Append(entry);
    object.SetNamedValue(L"channelMap", channelMap);

    auto const settings = BlockSettingsFromJson(BlockKind::Transpose, object);

    auto message = Midi1(0, NoteOn, 0, 60, 100);
    VERIFY_IS_TRUE(Run(BlockKind::Transpose, settings, message));
    VERIFY_IS_TRUE(message == Midi1(0, NoteOn, 0, 72, 100));

    // The channel map is ignored, and the block does not even remember it.
    VERIFY_ARE_EQUAL(0u, CountMapEntries(settings.Transform.ChannelMap.data(), settings.Transform.ChannelMap.size()));

    // Read as a channel mapper instead, the same object moves the channel and leaves the note.
    auto const channel = BlockSettingsFromJson(BlockKind::ChannelMap, object);

    auto other = Midi1(0, NoteOn, 0, 60, 100);
    VERIFY_IS_TRUE(Run(BlockKind::ChannelMap, channel, other));
    VERIFY_IS_TRUE(other == Midi1(0, NoteOn, 5, 60, 100));
}

void ProcessingBlockTests::SettingsSurviveTheFile()
{
    for (auto const kind : AllBlockKinds)
    {
        auto settings = DefaultBlockSettings(kind);

        switch (kind)
        {
        case BlockKind::MessageTypeFilter:
            settings.Filter.SystemMessages[4] = false;
            settings.Filter.MessageTypes[3] = false;
            settings.Filter.ChannelVoiceStatuses[0xE] = false;
            break;

        case BlockKind::ChannelFilter:
            settings.Filter.Channels.fill(false);
            settings.Filter.Channels[9] = true;
            break;

        case BlockKind::GroupFilter:
            settings.Groups.fill(false);
            settings.Groups[15] = true;
            break;

        case BlockKind::NoteFilter:
        case BlockKind::ControlChangeFilter:
            settings.Values.Mode = ValueSetMode::List;
            settings.Values.Action = FilterAction::KeepOut;
            settings.Values.Lowest = 10;
            settings.Values.Highest = 20;
            settings.Values.One = 99;
            settings.Values.List[0] = true;
            settings.Values.List[127] = true;
            break;

        case BlockKind::VelocityFilter:
            settings.Velocities.Action = FilterAction::KeepOut;
            settings.Velocities.Scale = ValueScale::SevenBit;
            settings.Velocities.LowestHundredths = 1234;
            settings.Velocities.HighestHundredths = 8765;
            break;

        case BlockKind::MessageMaskFilter:
        {
            MaskCondition condition{};
            condition.Word = 1;
            condition.HighBit = 15;
            condition.LowBit = 0;
            condition.Match = MaskMatch::AnyOf;
            condition.Values = { 1, 2, 65535 };
            condition.Lowest = 5;
            condition.Highest = 9;

            settings.Mask.WordCount = 2;
            settings.Mask.Action = FilterAction::LetThrough;
            settings.Mask.ShowHex = true;
            settings.Mask.Conditions.push_back(condition);
            break;
        }

        case BlockKind::ChannelMap:
            settings.Transform.ChannelMap[0] = 15;
            break;

        case BlockKind::GroupMap:
            settings.GroupMap[2] = 7;
            break;

        case BlockKind::NoteMap:
            settings.Transform.NoteMap[36] = 38;
            settings.Transform.IgnoreExactPitchNotes = true;
            break;

        case BlockKind::Transpose:
            settings.Transform.TransposeSemitones = -7;
            break;

        case BlockKind::Velocity:
            settings.Transform.Scale = ValueScale::SevenBit;
            settings.Transform.Curve = VelocityCurve::Fixed;
            settings.Transform.FixedVelocityHundredths = 4567;
            settings.Transform.RescaleVelocity = true;
            settings.Transform.MinimumVelocityHundredths = 1000;
            settings.Transform.MaximumVelocityHundredths = 9000;
            break;

        case BlockKind::Aftertouch:
            settings.Transform.AftertouchShape.Curve = ValueCurve::SlowRise;
            settings.Transform.AftertouchShape.OutputMaximumHundredths = 8000;
            break;

        case BlockKind::ControlChangeMap:
            settings.Transform.ControlMap[1] = 11;
            break;

        case BlockKind::ControlChangeValue:
            settings.Transform.ControlValueShapes[64].Invert = true;
            settings.Transform.ControlValueShapes[7].InputMinimumHundredths = 2000;
            break;

        case BlockKind::ProgramMap:
            settings.Transform.ProgramMap[0] = 10;
            settings.Transform.BankMsbMap[1] = 2;
            settings.Transform.BankLsbMap[3] = 4;
            break;

        case BlockKind::Throttle:
            settings.SendSpeedLimit = 4;
            break;

        case BlockKind::ClockDivider:
            settings.ClockDivision = 3;
            break;

        case BlockKind::ClockGenerator:
            settings.Clock.BeatsPerMinute = 97.5;
            settings.Clock.SwingPercent = 62;
            break;

        case BlockKind::TimeCodeGenerator:
            settings.TimeCode.FrameRate = midiapp::MidiTimeCodeFrameRate::Frames25;
            settings.TimeCode.Start.Hours = 1;
            break;

        case BlockKind::LfoGenerator:
            settings.Lfo.Wave = midiapp::LfoWave::RampDown;
            settings.Lfo.Target.Kind = midiapp::ValueMessageKind::PitchBend;
            settings.Lfo.Target.Number = 0;
            break;
        }

        VERIFY_IS_FALSE(BlockChangesNothing(kind, settings));

        auto const object = BlockSettingsToJson(kind, settings);
        auto const back = BlockSettingsFromJson(kind, json::JsonObject::Parse(object.Stringify()));

        auto const before = BlockSettingsSignature(kind, settings);
        auto const after = BlockSettingsSignature(kind, back);

        VERIFY_IS_FALSE(before.empty());
        VERIFY_ARE_EQUAL(before, after);
    }

    // A throttle can be turned off without being removed.
    auto off = DefaultBlockSettings(BlockKind::Throttle);
    off.SendSpeedLimit = 0;

    auto const backOff = BlockSettingsFromJson(BlockKind::Throttle, BlockSettingsToJson(BlockKind::Throttle, off));
    VERIFY_ARE_EQUAL(0u, backOff.SendSpeedLimit);
}

void ProcessingBlockTests::BadValuesInTheFileTakeTheDefault()
{
    auto const noteFilter = BlockSettingsFromJson(BlockKind::NoteFilter, json::JsonObject::Parse(
        LR"({"mode":"sideways","action":"maybe","lowest":300,"highest":-4,"note":1.5e9,"notes":[1,1,2,"x",128,-1,3.5]})"));

    VERIFY_IS_TRUE(noteFilter.Values.Mode == ValueSetMode::Range);
    VERIFY_IS_TRUE(noteFilter.Values.Action == FilterAction::LetThrough);
    VERIFY_ARE_EQUAL(0, static_cast<int>(noteFilter.Values.Lowest));
    VERIFY_ARE_EQUAL(127, static_cast<int>(noteFilter.Values.Highest));
    VERIFY_ARE_EQUAL(60, static_cast<int>(noteFilter.Values.One));
    VERIFY_IS_TRUE(noteFilter.Values.List[1]);
    VERIFY_IS_TRUE(noteFilter.Values.List[2]);
    VERIFY_IS_FALSE(noteFilter.Values.List[3]);
    VERIFY_IS_FALSE(noteFilter.Values.List[127]);

    // Ends written the wrong way round still mean the same range.
    auto const swapped = BlockSettingsFromJson(BlockKind::ControlChangeFilter, json::JsonObject::Parse(
        LR"({"mode":"range","lowest":90,"highest":10})"));

    VERIFY_ARE_EQUAL(10, static_cast<int>(swapped.Values.Lowest));
    VERIFY_ARE_EQUAL(90, static_cast<int>(swapped.Values.Highest));

    // Places that cannot exist in a message of the size are left out, and so is anything past
    // the fourth.
    auto const mask = BlockSettingsFromJson(BlockKind::MessageMaskFilter, json::JsonObject::Parse(
        LR"({"words":1,"conditions":[
            {"word":1,"highBit":31,"lowBit":0},
            {"word":0,"highBit":4,"lowBit":9},
            {"word":0,"highBit":40,"lowBit":0},
            "nonsense",
            {"word":0,"highBit":7,"lowBit":0,"match":"exactly","value":999},
            {"word":0,"highBit":7,"lowBit":0},
            {"word":0,"highBit":7,"lowBit":0},
            {"word":0,"highBit":7,"lowBit":0},
            {"word":0,"highBit":7,"lowBit":0}]})"));

    VERIFY_ARE_EQUAL(1, static_cast<int>(mask.Mask.WordCount));
    VERIFY_ARE_EQUAL(MaximumMaskConditions, mask.Mask.Conditions.size());
    VERIFY_ARE_EQUAL(0u, mask.Mask.Conditions[0].Value);

    auto const words = BlockSettingsFromJson(BlockKind::MessageMaskFilter, json::JsonObject::Parse(LR"({"words":9})"));
    VERIFY_ARE_EQUAL(2, static_cast<int>(words.Mask.WordCount));

    auto const throttle = BlockSettingsFromJson(BlockKind::Throttle, json::JsonObject::Parse(LR"({"speed":1000})"));
    VERIFY_ARE_EQUAL(DefaultThrottleSpeed, throttle.SendSpeedLimit);

    auto const missing = BlockSettingsFromJson(BlockKind::GroupFilter, nullptr);
    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::GroupFilter, missing));
}

void ProcessingBlockTests::GeneratorSettingsReadBackExactly()
{
    auto const roundTrip = [](BlockKind kind, BlockSettings const& settings)
        {
            return BlockSettingsFromJson(kind, json::JsonObject::Parse(BlockSettingsToJson(kind, settings).Stringify()));
        };

    auto clock = DefaultBlockSettings(BlockKind::ClockGenerator);
    clock.Clock.BeatsPerMinute = 97.5;
    clock.Clock.SendStartStop = false;
    clock.Clock.SwingPercent = 62;
    clock.Clock.SwingSubdivision = 4;
    clock.Clock.Group = 3;

    auto const clockBack = roundTrip(BlockKind::ClockGenerator, clock).Clock;

    VERIFY_ARE_EQUAL(97.5, clockBack.BeatsPerMinute);
    VERIFY_IS_FALSE(clockBack.SendStartStop);
    VERIFY_ARE_EQUAL(62.0, clockBack.SwingPercent);
    VERIFY_ARE_EQUAL(4, clockBack.SwingSubdivision);
    VERIFY_ARE_EQUAL(3, static_cast<int>(clockBack.Group));

    auto timeCode = DefaultBlockSettings(BlockKind::TimeCodeGenerator);
    timeCode.TimeCode.FrameRate = midiapp::MidiTimeCodeFrameRate::Frames2997Drop;
    timeCode.TimeCode.Start = midiapp::MidiTimeCodePosition{ 1, 2, 3, 4 };
    timeCode.TimeCode.SendFullFrame = false;
    timeCode.TimeCode.Group = 15;

    // Written the way people write them, so a file made by hand reads the same.
    auto const timeCodeObject = BlockSettingsToJson(BlockKind::TimeCodeGenerator, timeCode);

    VERIFY_ARE_EQUAL(29.97, timeCodeObject.GetNamedNumber(L"frameRate"));
    VERIFY_ARE_EQUAL(std::wstring{ L"01:02:03;04" }, std::wstring{ timeCodeObject.GetNamedString(L"startTime") });

    auto const timeCodeBack = roundTrip(BlockKind::TimeCodeGenerator, timeCode).TimeCode;

    VERIFY_IS_TRUE(timeCodeBack.FrameRate == midiapp::MidiTimeCodeFrameRate::Frames2997Drop);
    VERIFY_ARE_EQUAL(1, static_cast<int>(timeCodeBack.Start.Hours));
    VERIFY_ARE_EQUAL(2, static_cast<int>(timeCodeBack.Start.Minutes));
    VERIFY_ARE_EQUAL(3, static_cast<int>(timeCodeBack.Start.Seconds));
    VERIFY_ARE_EQUAL(4, static_cast<int>(timeCodeBack.Start.Frames));
    VERIFY_IS_FALSE(timeCodeBack.SendFullFrame);
    VERIFY_ARE_EQUAL(15, static_cast<int>(timeCodeBack.Group));

    auto lfo = DefaultBlockSettings(BlockKind::LfoGenerator);
    lfo.Lfo.Wave = midiapp::LfoWave::PinkNoise;
    lfo.Lfo.BeatsPerCycle = 1.5;
    lfo.Lfo.BeatsPerMinute = 90;
    lfo.Lfo.LowestHundredths = 8000;
    lfo.Lfo.HighestHundredths = 2000;
    lfo.Lfo.IntervalMilliseconds = 40;
    lfo.Lfo.Target.Kind = midiapp::ValueMessageKind::AssignableController;
    lfo.Lfo.Target.Number = 300;
    lfo.Lfo.Target.Channel = 5;
    lfo.Lfo.Target.Group = 7;
    lfo.Lfo.Target.Midi1Protocol = true;
    lfo.Lfo.ReturnsToMiddle = false;

    auto const lfoBack = roundTrip(BlockKind::LfoGenerator, lfo).Lfo;

    VERIFY_IS_TRUE(lfoBack.Wave == midiapp::LfoWave::PinkNoise);
    VERIFY_ARE_EQUAL(1.5, lfoBack.BeatsPerCycle);
    VERIFY_ARE_EQUAL(90.0, lfoBack.BeatsPerMinute);

    // Lowest above highest stays that way round: it is how a sweep is turned upside down.
    VERIFY_ARE_EQUAL(8000, lfoBack.LowestHundredths);
    VERIFY_ARE_EQUAL(2000, lfoBack.HighestHundredths);
    VERIFY_ARE_EQUAL(40, lfoBack.IntervalMilliseconds);
    VERIFY_IS_TRUE(lfoBack.Target.Kind == midiapp::ValueMessageKind::AssignableController);
    VERIFY_ARE_EQUAL(300u, lfoBack.Target.Number);
    VERIFY_ARE_EQUAL(5, static_cast<int>(lfoBack.Target.Channel));
    VERIFY_ARE_EQUAL(7, static_cast<int>(lfoBack.Target.Group));
    VERIFY_IS_TRUE(lfoBack.Target.Midi1Protocol);
    VERIFY_IS_FALSE(lfoBack.ReturnsToMiddle);

    auto divider = DefaultBlockSettings(BlockKind::ClockDivider);
    divider.ClockDivision = 96;

    VERIFY_ARE_EQUAL(96u, roundTrip(BlockKind::ClockDivider, divider).ClockDivision);
}

void ProcessingBlockTests::BadGeneratorValuesTakeTheDefault()
{
    auto const clock = BlockSettingsFromJson(BlockKind::ClockGenerator, json::JsonObject::Parse(
        LR"({"beatsPerMinute":9999,"sendStartStop":"yes","swingPercent":10,"swingSubdivision":3,"group":16})")).Clock;

    VERIFY_ARE_EQUAL(DefaultGeneratorBeatsPerMinute, clock.BeatsPerMinute);
    VERIFY_IS_TRUE(clock.SendStartStop);
    VERIFY_ARE_EQUAL(50.0, clock.SwingPercent);
    VERIFY_ARE_EQUAL(2, clock.SwingSubdivision);
    VERIFY_ARE_EQUAL(0, static_cast<int>(clock.Group));

    // Only the four rates MIDI Time Code has. 29.97 is always drop frame.
    auto const timeCode = BlockSettingsFromJson(BlockKind::TimeCodeGenerator, json::JsonObject::Parse(
        LR"({"frameRate":29,"startTime":"half past nine"})")).TimeCode;

    VERIFY_IS_TRUE(timeCode.FrameRate == midiapp::MidiTimeCodeFrameRate::Frames30);
    VERIFY_ARE_EQUAL(0, static_cast<int>(timeCode.Start.Hours));
    VERIFY_ARE_EQUAL(0, static_cast<int>(timeCode.Start.Frames));

    auto const film = BlockSettingsFromJson(BlockKind::TimeCodeGenerator, json::JsonObject::Parse(
        LR"({"frameRate":24,"startTime":"1:30"})")).TimeCode;

    VERIFY_IS_TRUE(film.FrameRate == midiapp::MidiTimeCodeFrameRate::Frames24);

    auto const lfo = BlockSettingsFromJson(BlockKind::LfoGenerator, json::JsonObject::Parse(
        LR"({"wave":"sawtooth","beatsPerCycle":0,"intervalMilliseconds":1,"lowestPercent":-5,"highestPercent":150,
            "message":"sysex","channel":20,"number":99999,"group":-1})")).Lfo;

    VERIFY_IS_TRUE(lfo.Wave == midiapp::LfoWave::Sine);
    VERIFY_ARE_EQUAL(4.0, lfo.BeatsPerCycle);
    VERIFY_ARE_EQUAL(midiapp::DefaultLfoIntervalMilliseconds, lfo.IntervalMilliseconds);
    VERIFY_ARE_EQUAL(0, lfo.LowestHundredths);
    VERIFY_ARE_EQUAL(FullScaleHundredths, lfo.HighestHundredths);
    VERIFY_IS_TRUE(lfo.Target.Kind == midiapp::ValueMessageKind::ControlChange);
    VERIFY_ARE_EQUAL(0, static_cast<int>(lfo.Target.Channel));
    VERIFY_ARE_EQUAL(1u, lfo.Target.Number);
    VERIFY_ARE_EQUAL(0, static_cast<int>(lfo.Target.Group));

    // A number is read against the kind it is for: 300 is an NRPN, and too big for a controller.
    auto const nrpn = BlockSettingsFromJson(BlockKind::LfoGenerator, json::JsonObject::Parse(
        LR"({"message":"nrpn","number":300})")).Lfo;

    VERIFY_ARE_EQUAL(300u, nrpn.Target.Number);

    auto const controller = BlockSettingsFromJson(BlockKind::LfoGenerator, json::JsonObject::Parse(
        LR"({"message":"controlChange","number":300})")).Lfo;

    VERIFY_ARE_EQUAL(1u, controller.Target.Number);

    VERIFY_ARE_EQUAL(DefaultClockDivision, BlockSettingsFromJson(BlockKind::ClockDivider,
        json::JsonObject::Parse(LR"({"divideBy":0})")).ClockDivision);
    VERIFY_ARE_EQUAL(DefaultClockDivision, BlockSettingsFromJson(BlockKind::ClockDivider,
        json::JsonObject::Parse(LR"({"divideBy":97})")).ClockDivision);
    VERIFY_ARE_EQUAL(3u, BlockSettingsFromJson(BlockKind::ClockDivider,
        json::JsonObject::Parse(LR"({"divideBy":3.7})")).ClockDivision);
}

void ProcessingBlockTests::TheClockDividerLetsOneInSoManyThrough()
{
    std::atomic<uint32_t> count{ 0 };

    auto const pulse = [&count](uint32_t divideBy)
        {
            auto message = System(0, 0xF8);
            return DivideClock(divideBy, count, message.Words.data(), message.Count);
        };

    std::vector<bool> passed{};

    for (int i = 0; i < 7; i++)
    {
        passed.push_back(pulse(3));
    }

    VERIFY_IS_TRUE((passed == std::vector<bool>{ true, false, false, true, false, false, true }));

    // Start puts the count back, so the first pulse after it always goes through.
    auto start = System(0, 0xFA);

    VERIFY_IS_TRUE(DivideClock(3, count, start.Words.data(), start.Count));
    VERIFY_IS_TRUE(pulse(3));
    VERIFY_IS_FALSE(pulse(3));

    // Stop, continue and everything else go through as they are, and leave the count alone.
    for (auto const& original : { System(0, 0xFC), System(0, 0xFB), System(0, 0xFE),
                                  Midi1(0, NoteOn, 0, 60, 100), Midi2(0, ControlChange, 0, 1, 0, 5u) })
    {
        auto message = original;

        VERIFY_IS_TRUE(DivideClock(3, count, message.Words.data(), message.Count));
        VERIFY_IS_TRUE(message == original);
    }

    VERIFY_IS_FALSE(pulse(3));
    VERIFY_IS_TRUE(pulse(3));

    // Dividing by one lets every pulse through.
    VERIFY_IS_TRUE(pulse(1));
    VERIFY_IS_TRUE(pulse(1));
}

void ProcessingBlockTests::TheClockDividerFollowsSongPosition()
{
    std::atomic<uint32_t> count{ 0 };

    auto const pulse = [&count](uint32_t divideBy)
        {
            auto message = System(0, 0xF8);
            return DivideClock(divideBy, count, message.Words.data(), message.Count);
        };

    // One beat in is 24 clocks. At half speed the device has had 12, which is two sixteenths,
    // and the next clock is one it gets.
    auto position = System(5, 0xF2, 4, 0);

    VERIFY_IS_TRUE(DivideClock(2, count, position.Words.data(), position.Count));
    VERIFY_IS_TRUE(position == System(5, 0xF2, 2, 0));
    VERIFY_IS_TRUE(pulse(2));

    // Three sixteenths in is 18 clocks. Divided by four the device has had five of them (0, 4,
    // 8, 12 and 16), which is not yet a sixteenth, and the next it gets is clock 20.
    position = System(0, 0xF2, 3, 0);

    VERIFY_IS_TRUE(DivideClock(4, count, position.Words.data(), position.Count));
    VERIFY_IS_TRUE(position == System(0, 0xF2, 0, 0));
    VERIFY_IS_FALSE(pulse(4));
    VERIFY_IS_FALSE(pulse(4));
    VERIFY_IS_TRUE(pulse(4));

    // A position that needs both bytes: 1000 sixteenths divided by three is 333.
    position = System(0, 0xF2, 1000 & 0x7F, 1000 >> 7);

    VERIFY_IS_TRUE(DivideClock(3, count, position.Words.data(), position.Count));
    VERIFY_IS_TRUE(position == System(0, 0xF2, 333 & 0x7F, 333 >> 7));
    VERIFY_IS_TRUE(pulse(3));
}

void ProcessingBlockTests::OnlyTheRightChangesRestartAGenerator()
{
    auto const restart = [](BlockKind kind, BlockSettings const& settings)
        {
            return GeneratorRestartSignature(kind, settings);
        };

    // A new tempo or more swing carries on from where the clock is.
    auto const clock = DefaultBlockSettings(BlockKind::ClockGenerator);
    auto faster = clock;
    faster.Clock.BeatsPerMinute = 140;
    faster.Clock.SwingPercent = 60;

    VERIFY_ARE_EQUAL(restart(BlockKind::ClockGenerator, clock), restart(BlockKind::ClockGenerator, faster));
    VERIFY_ARE_NOT_EQUAL(BlockSettingsSignature(BlockKind::ClockGenerator, clock),
        BlockSettingsSignature(BlockKind::ClockGenerator, faster));

    // Another group, or no start and stop, starts it again.
    auto moved = clock;
    moved.Clock.Group = 1;

    auto quiet = clock;
    quiet.Clock.SendStartStop = false;

    VERIFY_ARE_NOT_EQUAL(restart(BlockKind::ClockGenerator, clock), restart(BlockKind::ClockGenerator, moved));
    VERIFY_ARE_NOT_EQUAL(restart(BlockKind::ClockGenerator, clock), restart(BlockKind::ClockGenerator, quiet));

    // An LFO takes every change on the fly.
    auto const lfo = DefaultBlockSettings(BlockKind::LfoGenerator);
    auto other = lfo;
    other.Lfo.Wave = midiapp::LfoWave::Square;
    other.Lfo.BeatsPerMinute = 60;
    other.Lfo.Target.Kind = midiapp::ValueMessageKind::PitchBend;

    VERIFY_ARE_EQUAL(restart(BlockKind::LfoGenerator, lfo), restart(BlockKind::LfoGenerator, other));

    // Time code starts again for any change.
    auto const timeCode = DefaultBlockSettings(BlockKind::TimeCodeGenerator);
    auto later = timeCode;
    later.TimeCode.Start.Hours = 1;

    VERIFY_ARE_NOT_EQUAL(restart(BlockKind::TimeCodeGenerator, timeCode), restart(BlockKind::TimeCodeGenerator, later));
}
