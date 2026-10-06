// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "ProcessingBlock.h"

// Shared with MIDI Glass, in midi-app-shared. Header only and pure.
#include "FontNames.h"

#include <algorithm>
#include <cmath>

// Uses PVOID and UNREFERENCED_PARAMETER, so it comes after windows.h.
#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        struct KindInfo
        {
            BlockKind Kind;
            BlockCategory Category;
            wchar_t const* Key;
        };

        constexpr KindInfo Kinds[] =
        {
            { BlockKind::MessageTypeFilter, BlockCategory::Filter, L"messageTypeFilter" },
            { BlockKind::GroupFilter, BlockCategory::Filter, L"groupFilter" },
            { BlockKind::ChannelFilter, BlockCategory::Filter, L"channelFilter" },
            { BlockKind::NoteFilter, BlockCategory::Filter, L"noteFilter" },
            { BlockKind::ControlChangeFilter, BlockCategory::Filter, L"controlChangeFilter" },
            { BlockKind::VelocityFilter, BlockCategory::Filter, L"velocityFilter" },
            { BlockKind::MessageMaskFilter, BlockCategory::Filter, L"messageMaskFilter" },
            { BlockKind::ChannelMap, BlockCategory::Transform, L"channelMap" },
            { BlockKind::GroupMap, BlockCategory::Transform, L"groupMap" },
            { BlockKind::NoteMap, BlockCategory::Transform, L"noteMap" },
            { BlockKind::Transpose, BlockCategory::Transform, L"transpose" },
            { BlockKind::Velocity, BlockCategory::Transform, L"velocity" },
            { BlockKind::Aftertouch, BlockCategory::Transform, L"aftertouch" },
            { BlockKind::ControlChangeMap, BlockCategory::Transform, L"controlChangeMap" },
            { BlockKind::ControlChangeValue, BlockCategory::Transform, L"controlChangeValue" },
            { BlockKind::ProgramMap, BlockCategory::Transform, L"programMap" },
            { BlockKind::Throttle, BlockCategory::Sending, L"throttle" },
            { BlockKind::ClockGenerator, BlockCategory::Generator, L"clockGenerator" },
            { BlockKind::TimeCodeGenerator, BlockCategory::Generator, L"timeCodeGenerator" },
            { BlockKind::LfoGenerator, BlockCategory::Generator, L"lfoGenerator" },
            { BlockKind::ClockDivider, BlockCategory::Transform, L"clockDivider" },
            { BlockKind::Annotation, BlockCategory::Annotation, L"annotation" },
            { BlockKind::ParameterFilter, BlockCategory::Filter, L"rpnFilter" },
            { BlockKind::ParameterTransform, BlockCategory::Transform, L"rpnTransform" },
            { BlockKind::NoteDistributor, BlockCategory::Distribution, L"noteDistributor" },
            { BlockKind::Gate, BlockCategory::Distribution, L"gate" },
            { BlockKind::CiResponder, BlockCategory::CapabilityInquiry, L"ciResponder" },
            { BlockKind::CiFilter, BlockCategory::CapabilityInquiry, L"ciFilter" },
        };

        static_assert(std::size(Kinds) == BlockKindCount);

        // Filter keys, shared with a whole Preview 10 filter.
        constexpr wchar_t KeyMessageTypes[] = L"messageTypes";
        constexpr wchar_t KeyChannelVoice[] = L"channelVoiceStatuses";
        constexpr wchar_t KeySystem[] = L"systemMessages";
        constexpr wchar_t KeyChannels[] = L"channels";

        // Transform keys, shared with a whole Preview 10 transform so TransformFromJson reads a
        // block's part exactly the way it reads the whole thing. The round trip tests hold these
        // to what MessageTransform.cpp writes.
        constexpr wchar_t KeyScale[] = L"valueScale";
        constexpr wchar_t KeyTranspose[] = L"transposeSemitones";
        constexpr wchar_t KeyNoteMap[] = L"noteMap";
        constexpr wchar_t KeyIgnoreExactPitch[] = L"ignoreExactPitchNotes";
        constexpr wchar_t KeyControlMap[] = L"controlMap";
        constexpr wchar_t KeyChannelMap[] = L"channelMap";
        constexpr wchar_t KeyProgramMap[] = L"programMap";
        constexpr wchar_t KeyBankMsbMap[] = L"bankMsbMap";
        constexpr wchar_t KeyBankLsbMap[] = L"bankLsbMap";
        constexpr wchar_t KeyCurve[] = L"velocityCurve";
        constexpr wchar_t KeyFixedVelocityPercent[] = L"fixedVelocityPercent";
        constexpr wchar_t KeyRescale[] = L"rescaleVelocity";
        constexpr wchar_t KeyMinimumVelocityPercent[] = L"minimumVelocityPercent";
        constexpr wchar_t KeyMaximumVelocityPercent[] = L"maximumVelocityPercent";
        constexpr wchar_t KeyControlValueShapes[] = L"controlValueShapes";
        constexpr wchar_t KeyAftertouchShape[] = L"aftertouchShape";

        // Keys for the kinds Preview 10 did not have.
        constexpr wchar_t KeyGroups[] = L"groups";
        constexpr wchar_t KeyGroupMap[] = L"groupMap";
        constexpr wchar_t KeyMode[] = L"mode";
        constexpr wchar_t KeyAction[] = L"action";
        constexpr wchar_t KeyLowest[] = L"lowest";
        constexpr wchar_t KeyHighest[] = L"highest";
        constexpr wchar_t KeyNote[] = L"note";
        constexpr wchar_t KeyNotes[] = L"notes";
        constexpr wchar_t KeyController[] = L"controller";
        constexpr wchar_t KeyControllers[] = L"controllers";
        constexpr wchar_t KeyLowestPercent[] = L"lowestPercent";
        constexpr wchar_t KeyHighestPercent[] = L"highestPercent";
        constexpr wchar_t KeyWords[] = L"words";
        constexpr wchar_t KeyHex[] = L"hex";
        constexpr wchar_t KeyConditions[] = L"conditions";
        constexpr wchar_t KeyWord[] = L"word";
        constexpr wchar_t KeyHighBit[] = L"highBit";
        constexpr wchar_t KeyLowBit[] = L"lowBit";
        constexpr wchar_t KeyMatch[] = L"match";
        constexpr wchar_t KeyValue[] = L"value";
        constexpr wchar_t KeyValues[] = L"values";
        constexpr wchar_t KeySpeed[] = L"speed";

        // Generators and the clock divider.
        constexpr wchar_t KeyBeatsPerMinute[] = L"beatsPerMinute";
        constexpr wchar_t KeySendStartStop[] = L"sendStartStop";
        constexpr wchar_t KeySwingPercent[] = L"swingPercent";
        constexpr wchar_t KeySwingSubdivision[] = L"swingSubdivision";
        constexpr wchar_t KeyGroup[] = L"group";
        constexpr wchar_t KeyFrameRate[] = L"frameRate";
        constexpr wchar_t KeyStartTime[] = L"startTime";
        constexpr wchar_t KeySendFullFrame[] = L"sendFullFrame";
        constexpr wchar_t KeyWave[] = L"wave";
        constexpr wchar_t KeyBeatsPerCycle[] = L"beatsPerCycle";
        constexpr wchar_t KeyIntervalMilliseconds[] = L"intervalMilliseconds";
        constexpr wchar_t KeyMessage[] = L"message";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeyNumber[] = L"number";
        constexpr wchar_t KeyMidi1[] = L"midi1";
        constexpr wchar_t KeyReturnToMiddle[] = L"returnToMiddle";
        constexpr wchar_t KeyStartStopWithClock[] = L"startStopWithClock";
        constexpr wchar_t KeyDivideBy[] = L"divideBy";

        // Annotations.
        constexpr wchar_t KeyText[] = L"text";
        constexpr wchar_t KeyFontFamily[] = L"fontFamily";
        constexpr wchar_t KeyFontSize[] = L"fontSize";
        constexpr wchar_t KeyBold[] = L"bold";
        constexpr wchar_t KeyItalic[] = L"italic";
        constexpr wchar_t KeyUnderline[] = L"underline";
        constexpr wchar_t KeyColor[] = L"color";

        // (N)RPN filter and transform, note distributor and gate.
        constexpr wchar_t KeyParameters[] = L"parameters";
        constexpr wchar_t KeyType[] = L"type";
        constexpr wchar_t KeyBank[] = L"bank";
        constexpr wchar_t KeyIndex[] = L"index";
        constexpr wchar_t KeyRows[] = L"rows";
        constexpr wchar_t KeyFrom[] = L"from";
        constexpr wchar_t KeyTo[] = L"to";
        constexpr wchar_t KeyShape[] = L"shape";
        constexpr wchar_t KeyControlChangesToAll[] = L"controlChangesToAll";
        constexpr wchar_t KeyChannelPressureToAll[] = L"channelPressureToAll";
        constexpr wchar_t KeyPitchBendToAll[] = L"pitchBendToAll";
        constexpr wchar_t KeyOpen[] = L"open";
        constexpr wchar_t KeyClose[] = L"close";
        constexpr wchar_t KeyStartsOpen[] = L"startsOpen";
        constexpr wchar_t KeyPassTriggers[] = L"passTriggers";
        constexpr wchar_t KeyTest[] = L"test";
        constexpr wchar_t KeyMessageWords[] = L"messageWords";

        // MIDI-CI responder and filter.
        constexpr wchar_t KeyManufacturer[] = L"manufacturer";
        constexpr wchar_t KeyFamily[] = L"family";
        constexpr wchar_t KeyModel[] = L"model";
        constexpr wchar_t KeyVersion[] = L"version";
        constexpr wchar_t KeyProductInstanceId[] = L"productInstanceId";
        constexpr wchar_t KeyProcessInquiry[] = L"processInquiry";
        constexpr wchar_t KeyPassMidiCi[] = L"passMidiCi";
        constexpr wchar_t KeyFile[] = L"file";
        constexpr wchar_t KeyCategories[] = L"categories";

        // In the order of the CiCategory bits.
        constexpr wchar_t const* CiCategoryNames[]{ L"management", L"profiles", L"propertyExchange", L"processInquiry" };

        constexpr size_t MaximumCiFileNameLength = 200;
        constexpr size_t MaximumProductInstanceIdLength = 42;

        constexpr wchar_t const* ParameterKindNames[]{ L"rpn", L"nrpn", L"either" };
        constexpr wchar_t const* DistributionModeNames[]{ L"takeTurns", L"firstFree", L"highestNotes", L"lowestNotes" };
        constexpr wchar_t const* GateTriggerNames[]{
            L"noteOn", L"noteOff", L"controlChange", L"programChange", L"start", L"continue", L"stop", L"words" };
        constexpr wchar_t const* GateTestNames[]{ L"any", L"atLeast", L"below" };

        constexpr wchar_t ModeRange[] = L"range";
        constexpr wchar_t ModeOne[] = L"one";
        constexpr wchar_t ModeList[] = L"list";

        constexpr wchar_t ActionLetThrough[] = L"letThrough";
        constexpr wchar_t ActionKeepOut[] = L"keepOut";

        constexpr wchar_t MatchExactly[] = L"exactly";
        constexpr wchar_t MatchAnyOf[] = L"anyOf";
        constexpr wchar_t MatchBetween[] = L"between";

        constexpr wchar_t ScaleSevenBit[] = L"sevenBit";
        constexpr wchar_t ScalePercent[] = L"percent";

        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusControlChange = 0xB;

        // System common and real time, whole status bytes.
        constexpr uint8_t StatusSongPosition = 0xF2;
        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusStart = 0xFA;

        // A song position counts sixteenth notes, which is six clocks.
        constexpr uint32_t ClocksPerSongPositionStep = 6;

        constexpr uint32_t MaximumSendSpeed = 32;

        json::JsonValue GetValue(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (object != nullptr && object.HasKey(key))
                {
                    return object.GetNamedValue(key);
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        double ReadNumber(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ double lowest,
            _In_ double highest,
            _In_ double fallback) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Number)
                {
                    auto const number = value.GetNumber();

                    if (std::isfinite(number) && number >= lowest && number <= highest)
                    {
                        return number;
                    }
                }
            }
            catch (...)
            {
            }

            return fallback;
        }

        bool ReadBool(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ bool fallback) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Boolean)
                {
                    return value.GetBoolean();
                }
            }
            catch (...)
            {
            }

            return fallback;
        }

        std::wstring ReadString(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::String)
                {
                    return std::wstring{ value.GetString() };
                }
            }
            catch (...)
            {
            }

            return {};
        }

        // Whole numbers only, each in range, at most a list's worth.
        std::vector<uint32_t> ReadNumberList(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ uint32_t highest,
            _In_ size_t limit) noexcept
        {
            std::vector<uint32_t> result{};

            try
            {
                auto const value = GetValue(object, key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
                {
                    return result;
                }

                for (auto const& item : value.GetArray())
                {
                    if (result.size() >= limit)
                    {
                        break;
                    }

                    if (item == nullptr || item.ValueType() != json::JsonValueType::Number)
                    {
                        continue;
                    }

                    auto const number = item.GetNumber();

                    if (!std::isfinite(number) || number < 0 || number > highest || std::floor(number) != number)
                    {
                        continue;
                    }

                    auto const whole = static_cast<uint32_t>(number);

                    if (std::find(result.begin(), result.end(), whole) == result.end())
                    {
                        result.push_back(whole);
                    }
                }
            }
            catch (...)
            {
            }

            return result;
        }

        json::JsonArray NumberListToJson(_In_ std::vector<uint32_t> const& values) noexcept
        {
            json::JsonArray array{};

            try
            {
                for (auto const value : values)
                {
                    array.Append(json::JsonValue::CreateNumberValue(static_cast<double>(value)));
                }
            }
            catch (...)
            {
            }

            return array;
        }

        FilterAction ReadAction(_In_ json::JsonObject const& object, _In_ FilterAction fallback) noexcept
        {
            auto const text = ReadString(object, KeyAction);

            if (text == ActionLetThrough)
            {
                return FilterAction::LetThrough;
            }

            if (text == ActionKeepOut)
            {
                return FilterAction::KeepOut;
            }

            return fallback;
        }

        wchar_t const* ActionName(_In_ FilterAction action) noexcept
        {
            return action == FilterAction::KeepOut ? ActionKeepOut : ActionLetThrough;
        }

        ValueScale ReadScale(_In_ json::JsonObject const& object) noexcept
        {
            return ReadString(object, KeyScale) == ScaleSevenBit ? ValueScale::SevenBit : ValueScale::Percent;
        }

        wchar_t const* ScaleName(_In_ ValueScale scale) noexcept
        {
            return scale == ValueScale::SevenBit ? ScaleSevenBit : ScalePercent;
        }

        void SetNumber(_Inout_ json::JsonObject& object, _In_ std::wstring_view key, _In_ double value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateNumberValue(value));
        }

        void SetBool(_Inout_ json::JsonObject& object, _In_ std::wstring_view key, _In_ bool value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateBooleanValue(value));
        }

        void SetString(_Inout_ json::JsonObject& object, _In_ std::wstring_view key, _In_ std::wstring_view value)
        {
            object.SetNamedValue(key, json::JsonValue::CreateStringValue(value));
        }

        // A value set is written whole, so switching modes in the dialog and saving keeps what
        // the other modes held.
        void ValueSetToJson(
            _Inout_ json::JsonObject& object,
            _In_ ValueSetFilter const& values,
            _In_ std::wstring_view oneKey,
            _In_ std::wstring_view listKey)
        {
            SetString(object, KeyMode,
                values.Mode == ValueSetMode::One ? ModeOne : values.Mode == ValueSetMode::List ? ModeList : ModeRange);
            SetString(object, KeyAction, ActionName(values.Action));
            SetNumber(object, KeyLowest, values.Lowest);
            SetNumber(object, KeyHighest, values.Highest);
            SetNumber(object, oneKey, values.One);

            std::vector<uint32_t> listed{};

            for (uint32_t i = 0; i < SevenBitValueCount; i++)
            {
                if (values.List[i])
                {
                    listed.push_back(i);
                }
            }

            object.SetNamedValue(listKey, NumberListToJson(listed));
        }

        ValueSetFilter ValueSetFromJson(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view oneKey,
            _In_ std::wstring_view listKey,
            _In_ uint8_t defaultOne) noexcept
        {
            ValueSetFilter values{};

            auto const mode = ReadString(object, KeyMode);

            values.Mode = mode == ModeOne ? ValueSetMode::One : mode == ModeList ? ValueSetMode::List : ValueSetMode::Range;
            values.Action = ReadAction(object, FilterAction::LetThrough);
            values.Lowest = static_cast<uint8_t>(ReadNumber(object, KeyLowest, 0, 127, 0));
            values.Highest = static_cast<uint8_t>(ReadNumber(object, KeyHighest, 0, 127, 127));
            values.One = static_cast<uint8_t>(ReadNumber(object, oneKey, 0, 127, defaultOne));

            // A hand edited file could put the ends the wrong way round, which would silently
            // match nothing.
            if (values.Lowest > values.Highest)
            {
                std::swap(values.Lowest, values.Highest);
            }

            for (auto const value : ReadNumberList(object, listKey, 127, SevenBitValueCount))
            {
                values.List[value] = true;
            }

            return values;
        }

        uint32_t GroupFlags(_In_ std::array<bool, 16> const& groups) noexcept
        {
            return PackFlags(groups.data(), groups.size());
        }

        MaskMatch ReadMatch(_In_ json::JsonObject const& object) noexcept
        {
            auto const text = ReadString(object, KeyMatch);

            return text == MatchAnyOf ? MaskMatch::AnyOf : text == MatchBetween ? MaskMatch::Between : MaskMatch::Exactly;
        }

        wchar_t const* MatchName(_In_ MaskMatch match) noexcept
        {
            switch (match)
            {
            case MaskMatch::AnyOf:      return MatchAnyOf;
            case MaskMatch::Between:    return MatchBetween;
            default:                    return MatchExactly;
            }
        }

        bool IsChannelVoice(_In_ uint32_t word0, _Out_ bool& isMidi2) noexcept
        {
            auto const messageType = static_cast<uint8_t>((word0 >> 28) & 0x0F);

            isMidi2 = messageType == static_cast<uint8_t>(UmpMessageType::Midi2ChannelVoice);

            return isMidi2 || messageType == static_cast<uint8_t>(UmpMessageType::Midi1ChannelVoice);
        }

        // Keeps only the part of a transform this kind is named for, so a hand edited block that
        // carries other keys does exactly what its kind says and nothing else.
        void KeepTransformPart(_In_ BlockKind kind, _Inout_ MessageTransform& transform) noexcept
        {
            MessageTransform kept{};

            kept.IsActive = true;

            // Only the kinds that show a value care how it is shown. The others never write a
            // scale, so they hold the same one in memory as they read back.
            kept.Scale = kind == BlockKind::Velocity || kind == BlockKind::Aftertouch || kind == BlockKind::ControlChangeValue
                ? transform.Scale
                : ValueScale::Percent;

            switch (kind)
            {
            case BlockKind::ChannelMap:
                kept.ChannelMap = transform.ChannelMap;
                break;

            case BlockKind::NoteMap:
                kept.NoteMap = transform.NoteMap;
                kept.IgnoreExactPitchNotes = transform.IgnoreExactPitchNotes;
                break;

            case BlockKind::Transpose:
                kept.TransposeSemitones = transform.TransposeSemitones;
                kept.IgnoreExactPitchNotes = transform.IgnoreExactPitchNotes;
                break;

            case BlockKind::Velocity:
                kept.Curve = transform.Curve;
                kept.FixedVelocityHundredths = transform.FixedVelocityHundredths;
                kept.RescaleVelocity = transform.RescaleVelocity;
                kept.MinimumVelocityHundredths = transform.MinimumVelocityHundredths;
                kept.MaximumVelocityHundredths = transform.MaximumVelocityHundredths;
                break;

            case BlockKind::Aftertouch:
                kept.AftertouchShape = transform.AftertouchShape;
                break;

            case BlockKind::ControlChangeMap:
                kept.ControlMap = transform.ControlMap;
                break;

            case BlockKind::ControlChangeValue:
                kept.ControlValueShapes = transform.ControlValueShapes;
                break;

            case BlockKind::ProgramMap:
                kept.ProgramMap = transform.ProgramMap;
                kept.BankMsbMap = transform.BankMsbMap;
                kept.BankLsbMap = transform.BankLsbMap;
                break;

            default:
                break;
            }

            transform = kept;
        }

        void KeepFilterPart(_In_ BlockKind kind, _Inout_ MessageFilter& filter) noexcept
        {
            MessageFilter kept{};

            kept.IsActive = true;

            if (kind == BlockKind::MessageTypeFilter)
            {
                kept.MessageTypes = filter.MessageTypes;
                kept.ChannelVoiceStatuses = filter.ChannelVoiceStatuses;
                kept.SystemMessages = filter.SystemMessages;
            }
            else if (kind == BlockKind::ChannelFilter)
            {
                kept.Channels = filter.Channels;
            }

            filter = kept;
        }

        json::JsonArray GroupMapToJson(_In_ GroupMapTable const& map) noexcept
        {
            std::array<int16_t, 16> wide{};

            for (size_t i = 0; i < wide.size(); i++)
            {
                wide[i] = map[i];
            }

            return MapToJson(wide.data(), wide.size());
        }

        std::wstring PackedList(_In_ std::array<bool, SevenBitValueCount> const& list)
        {
            std::wstring text{};

            for (size_t chunk = 0; chunk < SevenBitValueCount; chunk += 32)
            {
                text += std::to_wstring(PackFlags(list.data() + chunk, 32)) + L'.';
            }

            return text;
        }

        uint8_t ReadGroup(_In_ json::JsonObject const& object) noexcept
        {
            return static_cast<uint8_t>(std::floor(ReadNumber(object, KeyGroup, 0, 15, 0)));
        }

        // The file says 24, 25, 29.97 or 30, the way a frame rate is written everywhere else.
        // 29.97 is always drop frame: it is the only 29.97 MIDI Time Code has.
        double FrameRateNumber(_In_ midiapp::MidiTimeCodeFrameRate rate) noexcept
        {
            switch (rate)
            {
            case midiapp::MidiTimeCodeFrameRate::Frames24:       return 24.0;
            case midiapp::MidiTimeCodeFrameRate::Frames25:       return 25.0;
            case midiapp::MidiTimeCodeFrameRate::Frames2997Drop: return 29.97;
            default:                                             return 30.0;
            }
        }

        midiapp::MidiTimeCodeFrameRate ReadFrameRate(_In_ json::JsonObject const& object) noexcept
        {
            auto const number = ReadNumber(object, KeyFrameRate, 0, 1000, 30.0);

            for (auto const rate : { midiapp::MidiTimeCodeFrameRate::Frames24, midiapp::MidiTimeCodeFrameRate::Frames25,
                                     midiapp::MidiTimeCodeFrameRate::Frames2997Drop, midiapp::MidiTimeCodeFrameRate::Frames30 })
            {
                if (std::abs(number - FrameRateNumber(rate)) < 0.01)
                {
                    return rate;
                }
            }

            return midiapp::MidiTimeCodeFrameRate::Frames30;
        }

        std::wstring NumberText(_In_ double value)
        {
            return std::to_wstring(value);
        }

        // The index of a name in a list, or the fallback.
        template <typename T, size_t N>
        T ReadName(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ wchar_t const* const (&names)[N], _In_ T fallback)
        {
            auto const text = ReadString(object, key);

            for (size_t i = 0; i < N; i++)
            {
                if (text == names[i])
                {
                    return static_cast<T>(i);
                }
            }

            return fallback;
        }

        json::JsonObject ReadObject(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                {
                    return value.GetObject();
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        json::JsonArray ReadArray(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(object, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
                {
                    return value.GetArray();
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // MIDI-CI numbers, 0 to 127 each.
        template <size_t N>
        json::JsonArray SevenBitBytesToJson(_In_ std::array<uint8_t, N> const& bytes)
        {
            json::JsonArray array{};

            for (auto const value : bytes)
            {
                array.Append(json::JsonValue::CreateNumberValue(value & 0x7F));
            }

            return array;
        }

        // All of them or none: a list of the wrong length, or with anything out of range, is left
        // as it was.
        template <size_t N>
        void SevenBitBytesFromJson(
            _In_ json::JsonObject const& object,
            _In_ std::wstring_view key,
            _Inout_ std::array<uint8_t, N>& bytes) noexcept
        {
            try
            {
                auto const list = ReadArray(object, key);

                if (list == nullptr || list.Size() != N)
                {
                    return;
                }

                std::array<uint8_t, N> read{};

                for (uint32_t i = 0; i < N; i++)
                {
                    auto const value = list.GetAt(i);

                    if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                    {
                        return;
                    }

                    auto const number = value.GetNumber();

                    if (!std::isfinite(number) || number < 0 || number > 127 || std::floor(number) != number)
                    {
                        return;
                    }

                    read[i] = static_cast<uint8_t>(number);
                }

                bytes = read;
            }
            catch (...)
            {
            }
        }

        // Left out of the file when it is any, and read back as any when it is missing.
        void SetOptional(_Inout_ json::JsonObject& object, _In_ std::wstring_view key, _In_ int32_t value)
        {
            if (value >= 0)
            {
                SetNumber(object, key, value);
            }
        }

        int16_t ReadOptional(_In_ json::JsonObject const& object, _In_ std::wstring_view key, _In_ int32_t highest) noexcept
        {
            return static_cast<int16_t>(std::floor(ReadNumber(object, key, 0, highest, -1)));
        }

        json::JsonObject ParameterMatchToJson(_In_ ParameterKind kind, _In_ int32_t bank, _In_ int32_t index)
        {
            json::JsonObject item{};

            if (kind != ParameterKind::Either || bank >= 0 || index >= 0)
            {
                SetString(item, KeyType, ParameterKindNames[static_cast<size_t>(kind)]);
            }

            SetOptional(item, KeyBank, bank);
            SetOptional(item, KeyIndex, index);

            return item;
        }

        ParameterMatch ParameterMatchFromJson(_In_ json::JsonObject const& item, _In_ ParameterKind fallback) noexcept
        {
            ParameterMatch match{};

            match.Kind = ReadName(item, KeyType, ParameterKindNames, fallback);
            match.Bank = ReadOptional(item, KeyBank, 127);
            match.Index = ReadOptional(item, KeyIndex, 127);

            return match;
        }

        json::JsonObject GateTriggerToJson(_In_ GateTrigger const& trigger)
        {
            json::JsonObject item{};

            SetString(item, KeyMessage, GateTriggerNames[static_cast<size_t>(trigger.Kind)]);
            SetOptional(item, KeyGroup, trigger.Group);

            switch (trigger.Kind)
            {
            case GateTriggerKind::NoteOn:
            case GateTriggerKind::NoteOff:
            case GateTriggerKind::ProgramChange:
                SetOptional(item, KeyChannel, trigger.Channel);
                SetOptional(item, KeyNumber, trigger.Number);
                break;

            case GateTriggerKind::ControlChange:
                SetOptional(item, KeyChannel, trigger.Channel);
                SetOptional(item, KeyNumber, trigger.Number);
                SetString(item, KeyTest, GateTestNames[static_cast<size_t>(trigger.Test)]);
                SetNumber(item, KeyValue, trigger.Value);
                break;

            case GateTriggerKind::Words:
            {
                json::JsonArray words{};

                for (uint8_t i = 0; i < trigger.WordCount && i < MaximumUmpWords; i++)
                {
                    words.Append(json::JsonValue::CreateNumberValue(trigger.Words[i]));
                }

                item.SetNamedValue(KeyMessageWords, words);
                break;
            }

            default:
                break;
            }

            return item;
        }

        GateTrigger GateTriggerFromJson(_In_ json::JsonObject const& item, _In_ GateTrigger const& fallback) noexcept
        {
            if (item == nullptr)
            {
                return fallback;
            }

            GateTrigger trigger{};

            trigger.Kind = ReadName(item, KeyMessage, GateTriggerNames, fallback.Kind);
            trigger.Group = static_cast<int8_t>(ReadOptional(item, KeyGroup, 15));
            trigger.Channel = static_cast<int8_t>(ReadOptional(item, KeyChannel, 15));
            trigger.Number = ReadOptional(item, KeyNumber, 127);
            trigger.Test = ReadName(item, KeyTest, GateTestNames, GateValueTest::Any);
            trigger.Value = static_cast<uint8_t>(std::floor(ReadNumber(item, KeyValue, 0, 127, 64)));

            try
            {
                if (auto const words = ReadArray(item, KeyMessageWords))
                {
                    uint8_t count{ 0 };

                    for (auto const& value : words)
                    {
                        if (count >= MaximumUmpWords || value.ValueType() != json::JsonValueType::Number)
                        {
                            break;
                        }

                        auto const number = value.GetNumber();

                        if (!std::isfinite(number) || number < 0 || number > 0xFFFFFFFF)
                        {
                            break;
                        }

                        trigger.Words[count++] = static_cast<uint32_t>(number);
                    }

                    trigger.WordCount = (std::max)(count, uint8_t{ 1 });
                }
            }
            catch (...)
            {
            }

            return trigger;
        }

        std::wstring GateTriggerSignature(_In_ GateTrigger const& trigger)
        {
            auto signature = std::to_wstring(static_cast<int32_t>(trigger.Kind)) + L'g' + std::to_wstring(trigger.Group) +
                L'c' + std::to_wstring(trigger.Channel) + L'n' + std::to_wstring(trigger.Number) + L't' +
                std::to_wstring(static_cast<int32_t>(trigger.Test)) + L'v' + std::to_wstring(trigger.Value) + L'w';

            for (uint8_t i = 0; i < trigger.WordCount && i < MaximumUmpWords; i++)
            {
                signature += std::to_wstring(trigger.Words[i]) + L',';
            }

            return signature;
        }
    }

    _Use_decl_annotations_
    BlockCategory CategoryOf(BlockKind kind) noexcept
    {
        for (auto const& info : Kinds)
        {
            if (info.Kind == kind)
            {
                return info.Category;
            }
        }

        return BlockCategory::Filter;
    }

    _Use_decl_annotations_
    uint32_t DefaultLfoNumber(midiapp::ValueMessageKind kind) noexcept
    {
        switch (kind)
        {
        case midiapp::ValueMessageKind::ControlChange:  return 1;
        case midiapp::ValueMessageKind::PolyPressure:   return 60;
        default:                                        return 0;
        }
    }

    _Use_decl_annotations_
    bool IsGenerator(BlockKind kind) noexcept
    {
        return CategoryOf(kind) == BlockCategory::Generator;
    }

    _Use_decl_annotations_
    bool IsAnnotation(BlockKind kind) noexcept
    {
        return kind == BlockKind::Annotation;
    }

    _Use_decl_annotations_
    bool HasInput(BlockKind kind) noexcept
    {
        return kind != BlockKind::ClockGenerator && kind != BlockKind::TimeCodeGenerator && kind != BlockKind::Annotation;
    }

    _Use_decl_annotations_
    bool HasOutput(BlockKind kind) noexcept
    {
        return kind != BlockKind::Annotation;
    }

    _Use_decl_annotations_
    bool CanGoIntoConnection(BlockKind kind) noexcept
    {
        // A generator passes on what it makes, never what comes in, so a connection through one
        // would quietly stop carrying its messages.
        return HasInput(kind) && HasOutput(kind) && !IsGenerator(kind);
    }

    _Use_decl_annotations_
    std::wstring AnnotationTextFrom(std::wstring_view text)
    {
        std::wstring kept{};
        kept.reserve((std::min)(text.size(), MaximumAnnotationLength));

        for (size_t i = 0; i < text.size() && kept.size() < MaximumAnnotationLength; i++)
        {
            auto const ch = text[i];

            // A text box ends a line with a carriage return, a file with a line feed, and text
            // pasted from Windows with both. Every one of them is kept as one line feed.
            if (ch == L'\r' || ch == L'\n' || ch == 0x2028 || ch == 0x2029)
            {
                if (ch == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
                {
                    i++;
                }

                kept.push_back(L'\n');
                continue;
            }

            // A tab or any other control character would draw as a box, or as nothing.
            auto const control = ch < L' ' || (ch >= 0x7F && ch <= 0x9F);

            kept.push_back(control ? L' ' : ch);
        }

        // Cut between the halves of a character outside the basic plane, which is not a character.
        if (!kept.empty() && kept.back() >= 0xD800 && kept.back() <= 0xDBFF)
        {
            kept.pop_back();
        }

        // A line break at the very end only leaves an empty line under the text.
        while (!kept.empty() && kept.back() == L'\n')
        {
            kept.pop_back();
        }

        return kept;
    }

    _Use_decl_annotations_
    std::wstring AnnotationColorFrom(std::wstring_view text)
    {
        if (text.size() != 7 || text[0] != L'#')
        {
            return {};
        }

        std::wstring color{ L"#" };

        for (auto const ch : text.substr(1))
        {
            if (ch >= L'0' && ch <= L'9')
            {
                color.push_back(ch);
            }
            else if (ch >= L'a' && ch <= L'f')
            {
                color.push_back(static_cast<wchar_t>(ch - L'a' + L'A'));
            }
            else if (ch >= L'A' && ch <= L'F')
            {
                color.push_back(ch);
            }
            else
            {
                return {};
            }
        }

        return color;
    }

    _Use_decl_annotations_
    std::wstring_view BlockKindKey(BlockKind kind) noexcept
    {
        for (auto const& info : Kinds)
        {
            if (info.Kind == kind)
            {
                return info.Key;
            }
        }

        return {};
    }

    _Use_decl_annotations_
    std::optional<BlockKind> BlockKindFromKey(std::wstring_view key) noexcept
    {
        for (auto const& info : Kinds)
        {
            if (key == info.Key)
            {
                return info.Kind;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool ValueSetFilter::Contains(uint8_t value) const noexcept
    {
        switch (Mode)
        {
        case ValueSetMode::One:
            return value == One;

        case ValueSetMode::List:
            return value < SevenBitValueCount && List[value];

        default:
            return value >= (std::min)(Lowest, Highest) && value <= (std::max)(Lowest, Highest);
        }
    }

    _Use_decl_annotations_
    bool ValueSetFilter::Passes(uint8_t value) const noexcept
    {
        return Contains(value) == (Action == FilterAction::LetThrough);
    }

    bool ValueSetFilter::PassesEverything() const noexcept
    {
        if (Action == FilterAction::LetThrough)
        {
            if (Mode == ValueSetMode::Range)
            {
                return (std::min)(Lowest, Highest) == 0 && (std::max)(Lowest, Highest) == 127;
            }

            if (Mode == ValueSetMode::List)
            {
                return AllTrue(List.data(), List.size());
            }

            return false;
        }

        // Keeping out an empty list keeps out nothing.
        return Mode == ValueSetMode::List && std::none_of(List.begin(), List.end(), [](bool b) { return b; });
    }

    _Use_decl_annotations_
    bool VelocityRange::Contains(int32_t hundredths) const noexcept
    {
        return hundredths >= (std::min)(LowestHundredths, HighestHundredths) &&
            hundredths <= (std::max)(LowestHundredths, HighestHundredths);
    }

    bool VelocityRange::PassesEverything() const noexcept
    {
        return Action == FilterAction::LetThrough &&
            (std::min)(LowestHundredths, HighestHundredths) <= 0 &&
            (std::max)(LowestHundredths, HighestHundredths) >= FullScaleHundredths;
    }

    uint8_t MaskCondition::BitCount() const noexcept
    {
        if (HighBit > 31 || LowBit > HighBit)
        {
            return 0;
        }

        return static_cast<uint8_t>(HighBit - LowBit + 1);
    }

    uint32_t MaskCondition::FieldMaximum() const noexcept
    {
        auto const bits = BitCount();

        if (bits == 0)
        {
            return 0;
        }

        return bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    }

    _Use_decl_annotations_
    uint32_t MaskCondition::FieldOf(uint32_t const* words, uint8_t wordCount) const noexcept
    {
        if (words == nullptr || Word >= wordCount || BitCount() == 0)
        {
            return 0;
        }

        return (words[Word] >> LowBit) & FieldMaximum();
    }

    _Use_decl_annotations_
    bool MaskCondition::Matches(uint32_t const* words, uint8_t wordCount) const noexcept
    {
        if (words == nullptr || Word >= wordCount || BitCount() == 0)
        {
            return false;
        }

        auto const field = FieldOf(words, wordCount);

        switch (Match)
        {
        case MaskMatch::AnyOf:
            return std::find(Values.begin(), Values.end(), field) != Values.end();

        case MaskMatch::Between:
            return field >= (std::min)(Lowest, Highest) && field <= (std::max)(Lowest, Highest);

        default:
            return field == Value;
        }
    }

    _Use_decl_annotations_
    bool MessageMask::Passes(uint32_t const* words, uint8_t wordCount) const noexcept
    {
        // Other sizes are not this filter's business, and with nothing to look at it does nothing.
        if (wordCount != WordCount || Conditions.empty())
        {
            return true;
        }

        auto const matches = std::all_of(Conditions.begin(), Conditions.end(),
            [words, wordCount](MaskCondition const& condition) { return condition.Matches(words, wordCount); });

        return matches == (Action == FilterAction::LetThrough);
    }

    _Use_decl_annotations_
    int32_t HundredthsFromVelocity16(uint16_t velocity) noexcept
    {
        return static_cast<int32_t>((static_cast<uint32_t>(velocity) * FullScaleHundredths + 32767u) / 65535u);
    }

    _Use_decl_annotations_
    BlockSettings DefaultBlockSettings(BlockKind kind) noexcept
    {
        BlockSettings settings{};

        settings.Filter.Reset();
        settings.Filter.IsActive = true;

        settings.Transform.Reset();
        settings.Transform.IsActive = true;
        settings.Transform.Scale = ValueScale::Percent;

        settings.Groups.fill(true);
        settings.GroupMap.fill(-1);

        // The mod wheel is the controller people reach for first.
        settings.Values.One = kind == BlockKind::ControlChangeFilter ? 1 : 60;

        return settings;
    }

    _Use_decl_annotations_
    bool ProcessBlock(BlockKind kind, BlockSettings const& settings, uint32_t* words, uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        switch (kind)
        {
        case BlockKind::MessageTypeFilter:
        case BlockKind::ChannelFilter:
            return settings.Filter.Allows(words, wordCount);

        case BlockKind::NoteFilter:
        {
            bool isMidi2{ false };

            if (!IsChannelVoice(words[0], isMidi2))
            {
                return true;
            }

            auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);

            if (!StatusCarriesNote(status, isMidi2))
            {
                return true;
            }

            return settings.Values.Passes(static_cast<uint8_t>((words[0] >> 8) & 0x7F));
        }

        case BlockKind::ControlChangeFilter:
        {
            bool isMidi2{ false };

            if (!IsChannelVoice(words[0], isMidi2) ||
                static_cast<uint8_t>((words[0] >> 20) & 0x0F) != StatusControlChange)
            {
                return true;
            }

            return settings.Values.Passes(static_cast<uint8_t>((words[0] >> 8) & 0x7F));
        }

        case BlockKind::VelocityFilter:
        {
            bool isMidi2{ false };

            if (!IsChannelVoice(words[0], isMidi2) ||
                static_cast<uint8_t>((words[0] >> 20) & 0x0F) != StatusNoteOn)
            {
                return true;
            }

            int32_t hundredths{ 0 };

            if (isMidi2)
            {
                if (wordCount < 2)
                {
                    return true;
                }

                hundredths = HundredthsFromVelocity16(static_cast<uint16_t>((words[1] >> 16) & 0xFFFF));
            }
            else
            {
                auto const velocity = static_cast<int32_t>(words[0] & 0x7F);

                // A MIDI 1.0 note on at velocity zero is a note off, and a note off always goes
                // through so nothing is left sounding.
                if (velocity == 0)
                {
                    return true;
                }

                hundredths = HundredthsFromSevenBit(velocity);
            }

            return settings.Velocities.Contains(hundredths) == (settings.Velocities.Action == FilterAction::LetThrough);
        }

        case BlockKind::GroupFilter:
            if (!internal::MessageHasGroupField(words[0]))
            {
                return true;
            }

            return settings.Groups[internal::GetGroupIndexFromFirstWord(words[0]) & 0x0F];

        case BlockKind::MessageMaskFilter:
            return settings.Mask.Passes(words, wordCount);

        case BlockKind::GroupMap:
            if (internal::MessageHasGroupField(words[0]))
            {
                auto const mapped = settings.GroupMap[internal::GetGroupIndexFromFirstWord(words[0]) & 0x0F];

                if (mapped >= 0 && mapped < 16)
                {
                    words[0] = internal::GetFirstWordWithNewGroup(words[0], static_cast<uint8_t>(mapped));
                }
            }

            return true;

        case BlockKind::ChannelMap:
        case BlockKind::NoteMap:
        case BlockKind::Transpose:
        case BlockKind::Velocity:
        case BlockKind::Aftertouch:
        case BlockKind::ControlChangeMap:
        case BlockKind::ControlChangeValue:
        case BlockKind::ProgramMap:
            settings.Transform.Apply(words, wordCount);
            return true;

        // Nothing comes into a generator. The route graph never sends it anything; this is for
        // anything else that asks.
        case BlockKind::ClockGenerator:
        case BlockKind::TimeCodeGenerator:
        case BlockKind::LfoGenerator:
            return false;

        // The clock divider needs its count, which the route graph keeps: see DivideClock.
        case BlockKind::ClockDivider:
        case BlockKind::Throttle:
        default:
            // The engine paces a throttle; nothing about the message changes.
            return true;
        }
    }

    _Use_decl_annotations_
    bool BlockChangesNothing(BlockKind kind, BlockSettings const& settings) noexcept
    {
        switch (kind)
        {
        case BlockKind::MessageTypeFilter:
        case BlockKind::ChannelFilter:
            return settings.Filter.PassesEverything();

        case BlockKind::NoteFilter:
        case BlockKind::ControlChangeFilter:
            return settings.Values.PassesEverything();

        case BlockKind::VelocityFilter:
            return settings.Velocities.PassesEverything();

        case BlockKind::GroupFilter:
            return AllTrue(settings.Groups.data(), settings.Groups.size());

        case BlockKind::MessageMaskFilter:
            return settings.Mask.Conditions.empty();

        case BlockKind::GroupMap:
            for (size_t i = 0; i < settings.GroupMap.size(); i++)
            {
                if (settings.GroupMap[i] >= 0 && settings.GroupMap[i] != static_cast<int8_t>(i))
                {
                    return false;
                }
            }

            return true;

        case BlockKind::Throttle:
            return settings.SendSpeedLimit == 0;

        case BlockKind::ClockDivider:
            return settings.ClockDivision <= 1;

        case BlockKind::ParameterFilter:
            return settings.ParameterFilter.Parameters.empty();

        case BlockKind::ParameterTransform:
            return settings.ParameterTransform.Rows.empty();

        // Both decide where each message goes, so they always run.
        case BlockKind::NoteDistributor:
        case BlockKind::Gate:
            return false;

        // Answers MIDI-CI, whatever else it is set to do.
        case BlockKind::CiResponder:
            return false;

        // Letting only some MIDI-CI through keeps everything else out.
        case BlockKind::CiFilter:
            return settings.CiFilter.Action == FilterAction::KeepOut && settings.CiFilter.Categories == 0;

        // Text on the canvas: messages never reach it.
        case BlockKind::Annotation:
            return true;

        case BlockKind::ClockGenerator:
        case BlockKind::TimeCodeGenerator:
        case BlockKind::LfoGenerator:
            return false;

        default:
            return settings.Transform.ChangesNothing();
        }
    }

    _Use_decl_annotations_
    json::JsonObject BlockSettingsToJson(BlockKind kind, BlockSettings const& settings) noexcept
    {
        json::JsonObject object{};

        try
        {
            auto const& filter = settings.Filter;
            auto const& transform = settings.Transform;

            switch (kind)
            {
            case BlockKind::MessageTypeFilter:
                SetNumber(object, KeyMessageTypes, PackFlags(filter.MessageTypes.data(), filter.MessageTypes.size()));
                SetNumber(object, KeyChannelVoice, PackFlags(filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size()));
                SetNumber(object, KeySystem, PackFlags(filter.SystemMessages.data(), filter.SystemMessages.size()));
                break;

            case BlockKind::ChannelFilter:
                SetNumber(object, KeyChannels, PackFlags(filter.Channels.data(), filter.Channels.size()));
                break;

            case BlockKind::GroupFilter:
                SetNumber(object, KeyGroups, GroupFlags(settings.Groups));
                break;

            case BlockKind::NoteFilter:
                ValueSetToJson(object, settings.Values, KeyNote, KeyNotes);
                break;

            case BlockKind::ControlChangeFilter:
                ValueSetToJson(object, settings.Values, KeyController, KeyControllers);
                break;

            case BlockKind::VelocityFilter:
                SetString(object, KeyAction, ActionName(settings.Velocities.Action));
                SetString(object, KeyScale, ScaleName(settings.Velocities.Scale));
                SetNumber(object, KeyLowestPercent, settings.Velocities.LowestHundredths / 100.0);
                SetNumber(object, KeyHighestPercent, settings.Velocities.HighestHundredths / 100.0);
                break;

            case BlockKind::MessageMaskFilter:
            {
                SetNumber(object, KeyWords, settings.Mask.WordCount);
                SetString(object, KeyAction, ActionName(settings.Mask.Action));
                SetBool(object, KeyHex, settings.Mask.ShowHex);

                json::JsonArray conditions{};

                for (auto const& condition : settings.Mask.Conditions)
                {
                    json::JsonObject item{};

                    SetNumber(item, KeyWord, condition.Word);
                    SetNumber(item, KeyHighBit, condition.HighBit);
                    SetNumber(item, KeyLowBit, condition.LowBit);
                    SetString(item, KeyMatch, MatchName(condition.Match));
                    SetNumber(item, KeyValue, condition.Value);
                    item.SetNamedValue(KeyValues, NumberListToJson(condition.Values));
                    SetNumber(item, KeyLowest, condition.Lowest);
                    SetNumber(item, KeyHighest, condition.Highest);

                    conditions.Append(item);
                }

                object.SetNamedValue(KeyConditions, conditions);
                break;
            }

            case BlockKind::ChannelMap:
                object.SetNamedValue(KeyChannelMap, MapToJson(transform.ChannelMap.data(), transform.ChannelMap.size()));
                break;

            case BlockKind::GroupMap:
                object.SetNamedValue(KeyGroupMap, GroupMapToJson(settings.GroupMap));
                break;

            case BlockKind::NoteMap:
                object.SetNamedValue(KeyNoteMap, MapToJson(transform.NoteMap.data(), transform.NoteMap.size()));
                SetBool(object, KeyIgnoreExactPitch, transform.IgnoreExactPitchNotes);
                break;

            case BlockKind::Transpose:
                SetNumber(object, KeyTranspose, transform.TransposeSemitones);
                SetBool(object, KeyIgnoreExactPitch, transform.IgnoreExactPitchNotes);
                break;

            case BlockKind::Velocity:
                SetString(object, KeyScale, ScaleName(transform.Scale));
                SetNumber(object, KeyCurve, static_cast<int32_t>(transform.Curve));
                SetNumber(object, KeyFixedVelocityPercent, transform.FixedVelocityHundredths / 100.0);
                SetBool(object, KeyRescale, transform.RescaleVelocity);
                SetNumber(object, KeyMinimumVelocityPercent, transform.MinimumVelocityHundredths / 100.0);
                SetNumber(object, KeyMaximumVelocityPercent, transform.MaximumVelocityHundredths / 100.0);
                break;

            case BlockKind::Aftertouch:
                SetString(object, KeyScale, ScaleName(transform.Scale));
                object.SetNamedValue(KeyAftertouchShape, ShapeToJson(transform.AftertouchShape, false));
                break;

            case BlockKind::ControlChangeMap:
                object.SetNamedValue(KeyControlMap, MapToJson(transform.ControlMap.data(), transform.ControlMap.size()));
                break;

            case BlockKind::ControlChangeValue:
                SetString(object, KeyScale, ScaleName(transform.Scale));
                object.SetNamedValue(KeyControlValueShapes, ControlValueShapesToJson(transform.ControlValueShapes));
                break;

            case BlockKind::ProgramMap:
                object.SetNamedValue(KeyProgramMap, MapToJson(transform.ProgramMap.data(), transform.ProgramMap.size()));
                object.SetNamedValue(KeyBankMsbMap, MapToJson(transform.BankMsbMap.data(), transform.BankMsbMap.size()));
                object.SetNamedValue(KeyBankLsbMap, MapToJson(transform.BankLsbMap.data(), transform.BankLsbMap.size()));
                break;

            case BlockKind::Throttle:
                SetNumber(object, KeySpeed, settings.SendSpeedLimit);
                break;

            case BlockKind::ClockGenerator:
                SetNumber(object, KeyBeatsPerMinute, settings.Clock.BeatsPerMinute);
                SetBool(object, KeySendStartStop, settings.Clock.SendStartStop);
                SetNumber(object, KeySwingPercent, settings.Clock.SwingPercent);
                SetNumber(object, KeySwingSubdivision, settings.Clock.SwingSubdivision);
                SetNumber(object, KeyGroup, settings.Clock.Group);
                break;

            case BlockKind::TimeCodeGenerator:
                SetNumber(object, KeyFrameRate, FrameRateNumber(settings.TimeCode.FrameRate));
                SetString(object, KeyStartTime, midiapp::FormatPosition(settings.TimeCode.Start, settings.TimeCode.FrameRate));
                SetBool(object, KeySendFullFrame, settings.TimeCode.SendFullFrame);
                SetNumber(object, KeyGroup, settings.TimeCode.Group);
                break;

            case BlockKind::LfoGenerator:
            {
                auto const& lfo = settings.Lfo;

                SetString(object, KeyWave, midiapp::LfoWaveKey(lfo.Wave));
                SetNumber(object, KeyBeatsPerCycle, lfo.BeatsPerCycle);
                SetNumber(object, KeyBeatsPerMinute, lfo.BeatsPerMinute);
                SetNumber(object, KeyLowestPercent, lfo.LowestHundredths / 100.0);
                SetNumber(object, KeyHighestPercent, lfo.HighestHundredths / 100.0);
                SetNumber(object, KeyIntervalMilliseconds, lfo.IntervalMilliseconds);
                SetString(object, KeyMessage, midiapp::ValueMessageKindKey(lfo.Target.Kind));
                SetNumber(object, KeyChannel, lfo.Target.Channel);
                SetNumber(object, KeyNumber, lfo.Target.Number);
                SetNumber(object, KeyGroup, lfo.Target.Group);
                SetBool(object, KeyMidi1, lfo.Target.Midi1Protocol);
                SetBool(object, KeyReturnToMiddle, lfo.ReturnsToMiddle);
                SetBool(object, KeyStartStopWithClock, lfo.KeepsToStartAndStop);
                break;
            }

            case BlockKind::ClockDivider:
                SetNumber(object, KeyDivideBy, settings.ClockDivision);
                break;

            case BlockKind::ParameterFilter:
            {
                SetString(object, KeyAction, ActionName(settings.ParameterFilter.Action));

                json::JsonArray parameters{};

                for (auto const& match : settings.ParameterFilter.Parameters)
                {
                    parameters.Append(ParameterMatchToJson(match.Kind, match.Bank, match.Index));
                }

                object.SetNamedValue(KeyParameters, parameters);
                break;
            }

            case BlockKind::ParameterTransform:
            {
                json::JsonArray rows{};

                for (auto const& row : settings.ParameterTransform.Rows)
                {
                    json::JsonObject item{};

                    item.SetNamedValue(KeyFrom, ParameterMatchToJson(row.From.Kind, row.From.Bank, row.From.Index));
                    item.SetNamedValue(KeyTo, ParameterMatchToJson(row.ToKind, row.ToBank, row.ToIndex));
                    item.SetNamedValue(KeyShape, ShapeToJson(row.Shape, true));

                    rows.Append(item);
                }

                object.SetNamedValue(KeyRows, rows);
                break;
            }

            case BlockKind::NoteDistributor:
            {
                auto const& distributor = settings.Distributor;

                SetString(object, KeyMode, DistributionModeNames[static_cast<size_t>(distributor.Mode)]);
                SetBool(object, KeyControlChangesToAll, distributor.ControlChangesToEveryVoice);
                SetBool(object, KeyChannelPressureToAll, distributor.ChannelPressureToEveryVoice);
                SetBool(object, KeyPitchBendToAll, distributor.PitchBendToEveryVoice);
                break;
            }

            case BlockKind::Gate:
                object.SetNamedValue(KeyOpen, GateTriggerToJson(settings.Gate.Open));
                object.SetNamedValue(KeyClose, GateTriggerToJson(settings.Gate.Close));
                SetBool(object, KeyStartsOpen, settings.Gate.StartsOpen);
                SetBool(object, KeyPassTriggers, settings.Gate.PassesTriggers);
                break;

            case BlockKind::CiResponder:
            {
                auto const& responder = settings.CiResponder;

                object.SetNamedValue(KeyManufacturer, SevenBitBytesToJson(responder.Manufacturer));
                SetNumber(object, KeyFamily, responder.Family);
                SetNumber(object, KeyModel, responder.Model);
                object.SetNamedValue(KeyVersion, SevenBitBytesToJson(responder.Version));
                SetString(object, KeyProductInstanceId, responder.ProductInstanceId);
                SetBool(object, KeyProcessInquiry, responder.ProcessInquiry);
                SetBool(object, KeyPassMidiCi, responder.PassMidiCi);
                SetString(object, KeyFile, responder.FileName);
                break;
            }

            case BlockKind::CiFilter:
            {
                SetString(object, KeyAction, ActionName(settings.CiFilter.Action));

                json::JsonArray categories{};

                for (size_t i = 0; i < std::size(CiCategoryNames); i++)
                {
                    if ((settings.CiFilter.Categories & (1u << i)) != 0)
                    {
                        categories.Append(json::JsonValue::CreateStringValue(CiCategoryNames[i]));
                    }
                }

                object.SetNamedValue(KeyCategories, categories);
                break;
            }

            case BlockKind::Annotation:
            {
                auto const& note = settings.Annotation;

                SetString(object, KeyText, note.Text);
                SetString(object, KeyFontFamily, note.FontFamily);
                SetNumber(object, KeyFontSize, note.FontSize);
                SetBool(object, KeyBold, note.Bold);
                SetBool(object, KeyItalic, note.Italic);
                SetBool(object, KeyUnderline, note.Underline);
                SetString(object, KeyColor, note.Color);
                break;
            }

            default:
                break;
            }
        }
        catch (...)
        {
        }

        return object;
    }

    _Use_decl_annotations_
    BlockSettings BlockSettingsFromJson(BlockKind kind, json::JsonObject const& object) noexcept
    {
        auto settings = DefaultBlockSettings(kind);

        if (object == nullptr)
        {
            return settings;
        }

        try
        {
            switch (kind)
            {
            case BlockKind::MessageTypeFilter:
            case BlockKind::ChannelFilter:
                settings.Filter = FilterFromJson(object);
                KeepFilterPart(kind, settings.Filter);
                break;

            case BlockKind::GroupFilter:
                UnpackFlags(static_cast<uint32_t>(ReadNumber(object, KeyGroups, 0, 65535, 65535)),
                    settings.Groups.data(), settings.Groups.size());
                break;

            case BlockKind::NoteFilter:
                settings.Values = ValueSetFromJson(object, KeyNote, KeyNotes, 60);
                break;

            case BlockKind::ControlChangeFilter:
                settings.Values = ValueSetFromJson(object, KeyController, KeyControllers, 1);
                break;

            case BlockKind::VelocityFilter:
                settings.Velocities.Action = ReadAction(object, FilterAction::LetThrough);
                settings.Velocities.Scale = ReadScale(object);
                settings.Velocities.LowestHundredths = static_cast<int32_t>(
                    std::lround(ReadNumber(object, KeyLowestPercent, 0, 100, 0) * 100.0));
                settings.Velocities.HighestHundredths = static_cast<int32_t>(
                    std::lround(ReadNumber(object, KeyHighestPercent, 0, 100, 100) * 100.0));

                if (settings.Velocities.LowestHundredths > settings.Velocities.HighestHundredths)
                {
                    std::swap(settings.Velocities.LowestHundredths, settings.Velocities.HighestHundredths);
                }
                break;

            case BlockKind::MessageMaskFilter:
            {
                auto& mask = settings.Mask;

                mask.WordCount = static_cast<uint8_t>(ReadNumber(object, KeyWords, 1, MaximumUmpWords, 2));
                mask.Action = ReadAction(object, FilterAction::KeepOut);
                mask.ShowHex = ReadBool(object, KeyHex, false);

                auto const list = GetValue(object, KeyConditions);

                if (list != nullptr && list.ValueType() == json::JsonValueType::Array)
                {
                    for (auto const& entry : list.GetArray())
                    {
                        if (mask.Conditions.size() >= MaximumMaskConditions)
                        {
                            break;
                        }

                        if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const item = entry.GetObject();

                        MaskCondition condition{};

                        condition.Word = static_cast<uint8_t>(ReadNumber(item, KeyWord, 0, MaximumUmpWords - 1, 255));
                        condition.HighBit = static_cast<uint8_t>(ReadNumber(item, KeyHighBit, 0, 31, 255));
                        condition.LowBit = static_cast<uint8_t>(ReadNumber(item, KeyLowBit, 0, 31, 255));

                        // A place that is not in a message of this size, or bits the wrong way
                        // round, could never match; leaving it out is clearer than keeping it.
                        if (condition.Word >= mask.WordCount || condition.HighBit > 31 ||
                            condition.LowBit > condition.HighBit)
                        {
                            continue;
                        }

                        auto const maximum = static_cast<double>(condition.FieldMaximum());

                        condition.Match = ReadMatch(item);
                        condition.Value = static_cast<uint32_t>(ReadNumber(item, KeyValue, 0, maximum, 0));
                        condition.Values = ReadNumberList(item, KeyValues, condition.FieldMaximum(), MaximumMaskValues);
                        condition.Lowest = static_cast<uint32_t>(ReadNumber(item, KeyLowest, 0, maximum, 0));
                        condition.Highest = static_cast<uint32_t>(ReadNumber(item, KeyHighest, 0, maximum, maximum));

                        if (condition.Lowest > condition.Highest)
                        {
                            std::swap(condition.Lowest, condition.Highest);
                        }

                        mask.Conditions.push_back(std::move(condition));
                    }
                }
                break;
            }

            case BlockKind::GroupMap:
            {
                std::array<int16_t, 16> wide{};

                MapFromJson(object, KeyGroupMap, wide.data(), wide.size());

                for (size_t i = 0; i < wide.size(); i++)
                {
                    settings.GroupMap[i] = static_cast<int8_t>(wide[i]);
                }
                break;
            }

            case BlockKind::ChannelMap:
            case BlockKind::NoteMap:
            case BlockKind::Transpose:
            case BlockKind::Velocity:
            case BlockKind::Aftertouch:
            case BlockKind::ControlChangeMap:
            case BlockKind::ControlChangeValue:
            case BlockKind::ProgramMap:
                settings.Transform = TransformFromJson(object);
                KeepTransformPart(kind, settings.Transform);

                // Blocks always write their scale, so a missing one means percent here rather
                // than the 0 to 127 a Preview 10 transform without one meant.
                if (!object.HasKey(KeyScale))
                {
                    settings.Transform.Scale = ValueScale::Percent;
                }
                break;

            case BlockKind::Throttle:
            {
                auto const speed = ReadNumber(object, KeySpeed, 0, MaximumSendSpeed, DefaultThrottleSpeed);

                settings.SendSpeedLimit = speed < 1.0 ? 0 : static_cast<uint32_t>(speed);
                break;
            }

            case BlockKind::ClockGenerator:
            {
                auto& clock = settings.Clock;

                clock.BeatsPerMinute = ReadNumber(object, KeyBeatsPerMinute,
                    MinimumGeneratorBeatsPerMinute, MaximumGeneratorBeatsPerMinute, DefaultGeneratorBeatsPerMinute);
                clock.SendStartStop = ReadBool(object, KeySendStartStop, true);
                clock.SwingPercent = ReadNumber(object, KeySwingPercent, 50.0, 75.0, 50.0);
                clock.SwingSubdivision = ReadNumber(object, KeySwingSubdivision, 2, 4, 2) == 4.0 ? 4 : 2;
                clock.Group = ReadGroup(object);
                break;
            }

            case BlockKind::TimeCodeGenerator:
            {
                auto& timeCode = settings.TimeCode;

                timeCode.FrameRate = ReadFrameRate(object);

                midiapp::MidiTimeCodePosition start{};
                auto const text = ReadString(object, KeyStartTime);

                if (!text.empty() && midiapp::TryParsePosition(text, timeCode.FrameRate, start))
                {
                    timeCode.Start = midiapp::ClampPosition(start, timeCode.FrameRate);
                }

                timeCode.SendFullFrame = ReadBool(object, KeySendFullFrame, true);
                timeCode.Group = ReadGroup(object);
                break;
            }

            case BlockKind::LfoGenerator:
            {
                auto& lfo = settings.Lfo;

                lfo.Wave = midiapp::LfoWaveFromKey(ReadString(object, KeyWave)).value_or(midiapp::LfoWave::Sine);
                lfo.BeatsPerCycle = ReadNumber(object, KeyBeatsPerCycle,
                    midiapp::MinimumLfoBeatsPerCycle, midiapp::MaximumLfoBeatsPerCycle, 4.0);
                lfo.BeatsPerMinute = ReadNumber(object, KeyBeatsPerMinute,
                    MinimumGeneratorBeatsPerMinute, MaximumGeneratorBeatsPerMinute, DefaultGeneratorBeatsPerMinute);

                // Not put in order: lowest above highest is how a sweep is turned upside down.
                lfo.LowestHundredths = static_cast<int32_t>(
                    std::lround(ReadNumber(object, KeyLowestPercent, 0, 100, 0) * 100.0));
                lfo.HighestHundredths = static_cast<int32_t>(
                    std::lround(ReadNumber(object, KeyHighestPercent, 0, 100, 100) * 100.0));

                lfo.IntervalMilliseconds = static_cast<int32_t>(std::lround(ReadNumber(object, KeyIntervalMilliseconds,
                    midiapp::MinimumLfoIntervalMilliseconds, midiapp::MaximumLfoIntervalMilliseconds,
                    midiapp::DefaultLfoIntervalMilliseconds)));

                auto& target = lfo.Target;

                target.Kind = midiapp::ValueMessageKindFromKey(ReadString(object, KeyMessage))
                    .value_or(midiapp::ValueMessageKind::ControlChange);
                target.Channel = static_cast<uint8_t>(std::floor(ReadNumber(object, KeyChannel, 0, 15, 0)));

                auto const numberMaximum = midiapp::ValueMessageNumberMaximum(target.Kind);

                target.Number = numberMaximum == 0
                    ? 0
                    : static_cast<uint32_t>(std::floor(ReadNumber(object, KeyNumber, 0, numberMaximum,
                        DefaultLfoNumber(target.Kind))));

                target.Group = ReadGroup(object);
                target.Midi1Protocol = ReadBool(object, KeyMidi1, false);

                lfo.ReturnsToMiddle = ReadBool(object, KeyReturnToMiddle, true);
                lfo.KeepsToStartAndStop = ReadBool(object, KeyStartStopWithClock, false);
                break;
            }

            case BlockKind::ClockDivider:
                settings.ClockDivision = static_cast<uint32_t>(
                    std::floor(ReadNumber(object, KeyDivideBy, 1, MaximumClockDivision, DefaultClockDivision)));
                break;

            case BlockKind::ParameterFilter:
            {
                auto& filter = settings.ParameterFilter;

                filter.Action = ReadAction(object, FilterAction::KeepOut);

                if (auto const parameters = ReadArray(object, KeyParameters))
                {
                    for (auto const& value : parameters)
                    {
                        if (filter.Parameters.size() >= MaximumParameterRows)
                        {
                            break;
                        }

                        if (value.ValueType() == json::JsonValueType::Object)
                        {
                            filter.Parameters.push_back(ParameterMatchFromJson(value.GetObject(), ParameterKind::Registered));
                        }
                    }
                }
                break;
            }

            case BlockKind::ParameterTransform:
            {
                if (auto const rows = ReadArray(object, KeyRows))
                {
                    for (auto const& value : rows)
                    {
                        if (settings.ParameterTransform.Rows.size() >= MaximumParameterRows)
                        {
                            break;
                        }

                        if (value.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const item = value.GetObject();
                        ParameterMapRow row{};

                        if (auto const from = ReadObject(item, KeyFrom))
                        {
                            row.From = ParameterMatchFromJson(from, ParameterKind::Registered);
                        }

                        if (auto const to = ReadObject(item, KeyTo))
                        {
                            auto const target = ParameterMatchFromJson(to, ParameterKind::Either);

                            row.ToKind = target.Kind;
                            row.ToBank = target.Bank;
                            row.ToIndex = target.Index;
                        }

                        row.Shape = ShapeFromJson(ReadObject(item, KeyShape), true);

                        settings.ParameterTransform.Rows.push_back(std::move(row));
                    }
                }
                break;
            }

            case BlockKind::NoteDistributor:
            {
                auto& distributor = settings.Distributor;

                distributor.Mode = ReadName(object, KeyMode, DistributionModeNames, DistributionMode::TakeTurns);
                distributor.ControlChangesToEveryVoice = ReadBool(object, KeyControlChangesToAll, true);
                distributor.ChannelPressureToEveryVoice = ReadBool(object, KeyChannelPressureToAll, true);
                distributor.PitchBendToEveryVoice = ReadBool(object, KeyPitchBendToAll, true);
                break;
            }

            case BlockKind::Gate:
            {
                auto& gate = settings.Gate;

                gate.Open = GateTriggerFromJson(ReadObject(object, KeyOpen), gate.Open);
                gate.Close = GateTriggerFromJson(ReadObject(object, KeyClose), gate.Close);
                gate.StartsOpen = ReadBool(object, KeyStartsOpen, true);
                gate.PassesTriggers = ReadBool(object, KeyPassTriggers, true);
                break;
            }

            case BlockKind::CiResponder:
            {
                auto& responder = settings.CiResponder;

                SevenBitBytesFromJson(object, KeyManufacturer, responder.Manufacturer);
                responder.Family = static_cast<uint16_t>(std::floor(ReadNumber(object, KeyFamily, 0, 16383, 0)));
                responder.Model = static_cast<uint16_t>(std::floor(ReadNumber(object, KeyModel, 0, 16383, 0)));
                SevenBitBytesFromJson(object, KeyVersion, responder.Version);
                responder.ProductInstanceId = CiProductInstanceIdFrom(ReadString(object, KeyProductInstanceId));
                responder.ProcessInquiry = ReadBool(object, KeyProcessInquiry, true);
                responder.PassMidiCi = ReadBool(object, KeyPassMidiCi, false);

                // A path, or anything else that isn't a plain file name, is never opened.
                auto file = ReadString(object, KeyFile);
                responder.FileName = IsCiFileName(file) ? std::move(file) : std::wstring{};
                break;
            }

            case BlockKind::CiFilter:
            {
                auto& filter = settings.CiFilter;

                filter.Action = ReadAction(object, FilterAction::KeepOut);

                if (auto const categories = ReadArray(object, KeyCategories))
                {
                    uint8_t bits{ 0 };

                    for (auto const& value : categories)
                    {
                        if (value == nullptr || value.ValueType() != json::JsonValueType::String)
                        {
                            continue;
                        }

                        auto const name = value.GetString();

                        for (size_t i = 0; i < std::size(CiCategoryNames); i++)
                        {
                            if (name == CiCategoryNames[i])
                            {
                                bits = static_cast<uint8_t>(bits | (1u << i));
                            }
                        }
                    }

                    filter.Categories = bits;
                }
                break;
            }

            case BlockKind::Annotation:
            {
                auto& note = settings.Annotation;

                note.Text = AnnotationTextFrom(ReadString(object, KeyText));

                // A family that could not be handed to XAML as it is goes back to the default.
                auto family = ReadString(object, KeyFontFamily);
                note.FontFamily = midiapp::IsSafeFontFamilyName(family) ? std::move(family) : std::wstring{};

                note.FontSize = ReadNumber(object, KeyFontSize,
                    MinimumAnnotationFontSize, MaximumAnnotationFontSize, DefaultAnnotationFontSize);
                note.Bold = ReadBool(object, KeyBold, false);
                note.Italic = ReadBool(object, KeyItalic, false);
                note.Underline = ReadBool(object, KeyUnderline, false);
                note.Color = AnnotationColorFrom(ReadString(object, KeyColor));
                break;
            }

            default:
                break;
            }
        }
        catch (...)
        {
        }

        return settings;
    }

    _Use_decl_annotations_
    std::wstring BlockSettingsSignature(BlockKind kind, BlockSettings const& settings) noexcept
    {
        try
        {
            std::wstring signature{ BlockKindKey(kind) };
            signature += L':';

            switch (kind)
            {
            case BlockKind::MessageTypeFilter:
            case BlockKind::ChannelFilter:
                signature += FilterSignature(settings.Filter);
                break;

            case BlockKind::NoteFilter:
            case BlockKind::ControlChangeFilter:
            {
                auto const& values = settings.Values;

                signature += std::to_wstring(static_cast<int32_t>(values.Mode)) + L'.' +
                    std::to_wstring(static_cast<int32_t>(values.Action)) + L'.' +
                    std::to_wstring(values.Lowest) + L'-' + std::to_wstring(values.Highest) + L'.' +
                    std::to_wstring(values.One) + L'.' + PackedList(values.List);
                break;
            }

            case BlockKind::VelocityFilter:
                signature += std::to_wstring(static_cast<int32_t>(settings.Velocities.Action)) + L'.' +
                    std::to_wstring(settings.Velocities.LowestHundredths) + L'-' +
                    std::to_wstring(settings.Velocities.HighestHundredths);
                break;

            case BlockKind::GroupFilter:
                signature += std::to_wstring(GroupFlags(settings.Groups));
                break;

            case BlockKind::GroupMap:
                for (auto const entry : settings.GroupMap)
                {
                    signature += std::to_wstring(entry) + L',';
                }
                break;

            case BlockKind::MessageMaskFilter:
                signature += std::to_wstring(settings.Mask.WordCount) + L'.' +
                    std::to_wstring(static_cast<int32_t>(settings.Mask.Action));

                for (auto const& condition : settings.Mask.Conditions)
                {
                    signature += L"|w" + std::to_wstring(condition.Word) + L'b' +
                        std::to_wstring(condition.HighBit) + L'-' + std::to_wstring(condition.LowBit) + L'm' +
                        std::to_wstring(static_cast<int32_t>(condition.Match)) + L'=' +
                        std::to_wstring(condition.Value) + L'/' + std::to_wstring(condition.Lowest) + L'-' +
                        std::to_wstring(condition.Highest) + L'/';

                    for (auto const value : condition.Values)
                    {
                        signature += std::to_wstring(value) + L',';
                    }
                }
                break;

            case BlockKind::Throttle:
                signature += std::to_wstring(settings.SendSpeedLimit);
                break;

            case BlockKind::ClockGenerator:
            {
                auto const& clock = settings.Clock;

                signature += NumberText(clock.BeatsPerMinute) + (clock.SendStartStop ? L".ss." : L".--.") +
                    NumberText(clock.SwingPercent) + L'/' + std::to_wstring(clock.SwingSubdivision) +
                    L".g" + std::to_wstring(clock.Group);
                break;
            }

            case BlockKind::TimeCodeGenerator:
            {
                auto const& timeCode = settings.TimeCode;

                signature += std::to_wstring(static_cast<int32_t>(timeCode.FrameRate)) + L'.' +
                    midiapp::FormatPosition(timeCode.Start, timeCode.FrameRate) +
                    (timeCode.SendFullFrame ? L".ff" : L".--") + L".g" + std::to_wstring(timeCode.Group);
                break;
            }

            case BlockKind::LfoGenerator:
            {
                auto const& lfo = settings.Lfo;

                signature += std::wstring{ midiapp::LfoWaveKey(lfo.Wave) } + L'.' +
                    NumberText(lfo.BeatsPerCycle) + L'@' + NumberText(lfo.BeatsPerMinute) + L'.' +
                    std::to_wstring(lfo.LowestHundredths) + L'-' + std::to_wstring(lfo.HighestHundredths) + L'.' +
                    std::to_wstring(lfo.IntervalMilliseconds) + L'.' +
                    std::wstring{ midiapp::ValueMessageKindKey(lfo.Target.Kind) } + L'.' +
                    std::to_wstring(lfo.Target.Channel) + L'.' + std::to_wstring(lfo.Target.Number) +
                    L".g" + std::to_wstring(lfo.Target.Group) +
                    (lfo.Target.Midi1Protocol ? L".m1" : L".m2") + (lfo.ReturnsToMiddle ? L".r" : L".-") + (lfo.KeepsToStartAndStop ? L".s" : L".-");
                break;
            }

            case BlockKind::ClockDivider:
                signature += std::to_wstring(settings.ClockDivision);
                break;

            case BlockKind::ParameterFilter:
                signature += std::to_wstring(static_cast<int32_t>(settings.ParameterFilter.Action));

                for (auto const& match : settings.ParameterFilter.Parameters)
                {
                    signature += L'|' + std::to_wstring(static_cast<int32_t>(match.Kind)) + L'.' +
                        std::to_wstring(match.Bank) + L'.' + std::to_wstring(match.Index);
                }
                break;

            case BlockKind::ParameterTransform:
                for (auto const& row : settings.ParameterTransform.Rows)
                {
                    signature += L'|' + std::to_wstring(static_cast<int32_t>(row.From.Kind)) + L'.' +
                        std::to_wstring(row.From.Bank) + L'.' + std::to_wstring(row.From.Index) + L'>' +
                        std::to_wstring(static_cast<int32_t>(row.ToKind)) + L'.' +
                        std::to_wstring(row.ToBank) + L'.' + std::to_wstring(row.ToIndex) + L'~' +
                        ShapeSignature(row.Shape);
                }
                break;

            case BlockKind::NoteDistributor:
            {
                auto const& distributor = settings.Distributor;

                signature += std::to_wstring(static_cast<int32_t>(distributor.Mode)) +
                    (distributor.ControlChangesToEveryVoice ? L".c" : L".-") +
                    (distributor.ChannelPressureToEveryVoice ? L"p" : L"-") +
                    (distributor.PitchBendToEveryVoice ? L"b" : L"-");
                break;
            }

            case BlockKind::Gate:
                signature += GateTriggerSignature(settings.Gate.Open) + L'/' + GateTriggerSignature(settings.Gate.Close) +
                    (settings.Gate.StartsOpen ? L".o" : L".-") + (settings.Gate.PassesTriggers ? L"p" : L"-");
                break;

            // What the file says is added where the patch is routed, because it is read then.
            case BlockKind::CiResponder:
            {
                auto const& responder = settings.CiResponder;

                for (auto const value : responder.Manufacturer)
                {
                    signature += std::to_wstring(value) + L'.';
                }

                signature += std::to_wstring(responder.Family) + L'.' + std::to_wstring(responder.Model) + L'.';

                for (auto const value : responder.Version)
                {
                    signature += std::to_wstring(value) + L'.';
                }

                signature += responder.ProcessInquiry ? L"pi" : L"--";
                signature += responder.PassMidiCi ? L".pass|" : L".keep|";
                signature += responder.ProductInstanceId + L'|' + responder.FileName;
                break;
            }

            case BlockKind::CiFilter:
                signature += std::to_wstring(static_cast<int32_t>(settings.CiFilter.Action)) + L'.' +
                    std::to_wstring(settings.CiFilter.Categories);
                break;

            case BlockKind::Annotation:
            {
                auto const& note = settings.Annotation;

                signature += NumberText(note.FontSize) + (note.Bold ? L".b" : L".-") +
                    (note.Italic ? L"i" : L"-") + (note.Underline ? L"u" : L"-") + L'.' +
                    note.Color + L'.' + note.FontFamily + L'|' + note.Text;
                break;
            }

            default:
                signature += TransformSignature(settings.Transform);
                break;
            }

            return signature;
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    bool IsCiFileName(std::wstring_view name) noexcept
    {
        if (name.empty() || name.size() > MaximumCiFileNameLength || name == L"." || name == L"..")
        {
            return false;
        }

        // Windows drops a trailing dot or space, so the file opened would not be the one named.
        if (name.front() == L' ' || name.back() == L' ' || name.back() == L'.')
        {
            return false;
        }

        for (auto const c : name)
        {
            // 0x5C is the backslash.
            if (c < 0x20 || c == 0x5C || c == L'/' || c == L':' || c == L'*' || c == L'?' ||
                c == L'"' || c == L'<' || c == L'>' || c == L'|')
            {
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    std::wstring CiProductInstanceIdFrom(std::wstring_view text)
    {
        std::wstring kept{};

        for (auto const c : text)
        {
            if (c >= 0x20 && c <= 0x7E && kept.size() < MaximumProductInstanceIdLength)
            {
                kept += c;
            }
        }

        return kept;
    }

    _Use_decl_annotations_
    bool DivideClock(uint32_t divideBy, std::atomic<uint32_t>& counter, uint32_t* words, uint8_t wordCount) noexcept
    {
        if (words == nullptr || wordCount == 0 || divideBy <= 1)
        {
            return true;
        }

        // Clock, start and song position are all system messages; nothing else is counted.
        if (((words[0] >> 28) & 0x0F) != static_cast<uint32_t>(UmpMessageType::System))
        {
            return true;
        }

        switch (static_cast<uint8_t>((words[0] >> 16) & 0xFF))
        {
        case StatusTimingClock:
        {
            // Kept below divideBy, so the count never wraps in a way that breaks the pattern.
            auto current = counter.load(std::memory_order_relaxed);
            uint32_t next{ 0 };

            do
            {
                next = (current % divideBy + 1) % divideBy;
            }
            while (!counter.compare_exchange_weak(current, next, std::memory_order_relaxed));

            return current % divideBy == 0;
        }

        case StatusStart:
            counter.store(0, std::memory_order_relaxed);
            return true;

        case StatusSongPosition:
        {
            // Sixteenth notes since the top, low seven bits first.
            auto const position = ((words[0] >> 8) & 0x7Fu) | ((words[0] & 0x7Fu) << 7);
            auto const clocks = position * ClocksPerSongPositionStep;

            // The next clock is the one at this position, so the count picks up where it would
            // have been had the clocks been played from the top.
            counter.store(clocks % divideBy, std::memory_order_relaxed);

            // Where the slower device is: the clocks it would have been sent, in sixteenths.
            auto const sent = (clocks + divideBy - 1) / divideBy;
            auto const divided = (std::min)(sent / ClocksPerSongPositionStep, 0x3FFFu);

            words[0] = (words[0] & 0xFFFF0000u) | ((divided & 0x7Fu) << 8) | ((divided >> 7) & 0x7Fu);
            return true;
        }

        default:
            return true;
        }
    }

    _Use_decl_annotations_
    std::wstring GeneratorRestartSignature(BlockKind kind, BlockSettings const& settings) noexcept
    {
        try
        {
            switch (kind)
            {
            case BlockKind::ClockGenerator:
                // The tempo and the amount of swing change on the fly.
                return std::wstring{ L"clock." } + (settings.Clock.SendStartStop ? L"ss." : L"--.") +
                    std::to_wstring(settings.Clock.SwingSubdivision) + L".g" + std::to_wstring(settings.Clock.Group);

            case BlockKind::LfoGenerator:
                // Everything about a sweep changes on the fly, and the phase carries on.
                return L"lfo";

            default:
                // A new frame rate or start time means starting the time code again.
                return BlockSettingsSignature(kind, settings);
            }
        }
        catch (...)
        {
        }

        return {};
    }
}
