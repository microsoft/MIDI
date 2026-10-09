// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageText.h"
#include "StringResources.h"

namespace midipatchbay
{
    _Use_decl_annotations_
    std::wstring DescribeRuns(bool const* values, size_t count, int offset) noexcept
    {
        std::wstring text{};

        try
        {
            size_t index{ 0 };

            while (index < count)
            {
                if (!values[index])
                {
                    index++;
                    continue;
                }

                auto const start = index;

                while (index + 1 < count && values[index + 1])
                {
                    index++;
                }

                if (!text.empty())
                {
                    text += L", ";
                }

                text += start == index
                    ? std::to_wstring(static_cast<int>(start) + offset)
                    : std::to_wstring(static_cast<int>(start) + offset) + L" - " +
                      std::to_wstring(static_cast<int>(index) + offset);

                index++;
            }
        }
        catch (...)
        {
        }

        return text;
    }

    _Use_decl_annotations_
    winrt::hstring DescribeNote(uint8_t noteIndex) noexcept
    {
        try
        {
            // The shipped helper, so this app names notes exactly like the rest of the tools.
            // Note names are MIDI proper nouns and are not translated.
            auto const name = midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(noteIndex);
            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(noteIndex);

            return winrt::hstring{ std::wstring{ name } + std::to_wstring(octave) };
        }
        catch (...)
        {
        }

        return winrt::to_hstring(static_cast<int>(noteIndex));
    }

    _Use_decl_annotations_
    winrt::hstring DescribeParameter(ParameterKind kind, int32_t bank, int32_t index) noexcept
    {
        try
        {
            auto const part = [](int32_t value)
                {
                    return value < 0 ? resources::GetString(L"ParameterAny") : winrt::to_hstring(value);
                };

            auto const name = resources::GetString(kind == ParameterKind::Assignable
                ? L"ParameterKindAssignable"
                : kind == ParameterKind::Either ? L"ParameterKindEither" : L"ParameterKindRegistered");

            return resources::FormatString(L"ParameterFormat", name, part(bank), part(index));
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeGateTrigger(GateTrigger const& trigger) noexcept
    {
        try
        {
            switch (trigger.Kind)
            {
            case GateTriggerKind::NoteOn:
            case GateTriggerKind::NoteOff:
            {
                auto const on = trigger.Kind == GateTriggerKind::NoteOn;

                return trigger.Number < 0
                    ? resources::GetString(on ? L"GateTriggerNoteOnAny" : L"GateTriggerNoteOffAny")
                    : resources::FormatString(on ? L"GateTriggerNoteOnFormat" : L"GateTriggerNoteOffFormat",
                        DescribeNote(static_cast<uint8_t>(trigger.Number & 0x7F)));
            }

            case GateTriggerKind::ControlChange:
            {
                auto const controller = trigger.Number < 0
                    ? resources::GetString(L"GateTriggerControlAny")
                    : resources::FormatString(L"GateTriggerControlFormat", static_cast<int>(trigger.Number));

                switch (trigger.Test)
                {
                case GateValueTest::AtLeast:
                    return resources::FormatString(L"GateTriggerAtLeastFormat", controller, static_cast<int>(trigger.Value));

                case GateValueTest::Below:
                    return resources::FormatString(L"GateTriggerBelowFormat", controller, static_cast<int>(trigger.Value));

                default:
                    return controller;
                }
            }

            case GateTriggerKind::ProgramChange:
                return trigger.Number < 0
                    ? resources::GetString(L"GateTriggerProgramAny")
                    : resources::FormatString(L"GateTriggerProgramFormat", static_cast<int>(trigger.Number));

            case GateTriggerKind::Start:    return resources::GetString(L"GateTriggerStart");
            case GateTriggerKind::Continue: return resources::GetString(L"GateTriggerContinue");
            case GateTriggerKind::Stop:     return resources::GetString(L"GateTriggerStop");
            default:                        return resources::GetString(L"GateTriggerWords");
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeMessageType(uint8_t messageType) noexcept
    {
        switch (messageType)
        {
        case 0x0: return resources::GetString(L"MessageTypeUtility");
        case 0x1: return resources::GetString(L"MessageTypeSystem");
        case 0x2: return resources::GetString(L"MessageTypeMidi1ChannelVoice");
        case 0x3: return resources::GetString(L"MessageTypeSysEx7");
        case 0x4: return resources::GetString(L"MessageTypeMidi2ChannelVoice");
        case 0x5: return resources::GetString(L"MessageTypeData128");
        case 0xD: return resources::GetString(L"MessageTypeFlexData");
        case 0xF: return resources::GetString(L"MessageTypeStream");
        default: return resources::FormatString(L"MessageTypeReservedFormat", messageType);
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeChannelVoiceStatus(uint8_t status) noexcept
    {
        switch (status)
        {
        case 0x0: return resources::GetString(L"VoiceRegisteredPerNote");
        case 0x1: return resources::GetString(L"VoiceAssignablePerNote");
        case 0x2: return resources::GetString(L"VoiceRegisteredController");
        case 0x3: return resources::GetString(L"VoiceAssignableController");
        case 0x4: return resources::GetString(L"VoiceRelativeRegisteredController");
        case 0x5: return resources::GetString(L"VoiceRelativeAssignableController");
        case 0x6: return resources::GetString(L"VoicePerNotePitchBend");
        case 0x8: return resources::GetString(L"VoiceNoteOff");
        case 0x9: return resources::GetString(L"VoiceNoteOn");
        case 0xA: return resources::GetString(L"VoicePolyPressure");
        case 0xB: return resources::GetString(L"VoiceControlChange");
        case 0xC: return resources::GetString(L"VoiceProgramChange");
        case 0xD: return resources::GetString(L"VoiceChannelPressure");
        case 0xE: return resources::GetString(L"VoicePitchBend");
        case 0xF: return resources::GetString(L"VoicePerNoteManagement");
        default: return resources::FormatString(L"VoiceReservedFormat", status);
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeSystemMessage(uint8_t status) noexcept
    {
        switch (status)
        {
        case 0xF1: return resources::GetString(L"SystemTimeCode");
        case 0xF2: return resources::GetString(L"SystemSongPosition");
        case 0xF3: return resources::GetString(L"SystemSongSelect");
        case 0xF6: return resources::GetString(L"SystemTuneRequest");
        case 0xF8: return resources::GetString(L"SystemTimingClock");
        case 0xFA: return resources::GetString(L"SystemStart");
        case 0xFB: return resources::GetString(L"SystemContinue");
        case 0xFC: return resources::GetString(L"SystemStop");
        case 0xFE: return resources::GetString(L"SystemActiveSensing");
        case 0xFF: return resources::GetString(L"SystemReset");
        default: return resources::FormatString(L"SystemReservedFormat", status);
        }
    }

    namespace
    {
        // Beyond this the list stops being something anyone reads and turns into a wall of text.
        constexpr size_t MaximumNamesInSummary = 4;

        std::wstring JoinNames(_In_ std::vector<std::wstring> const& names)
        {
            std::wstring text{};
            auto const shown = (std::min)(names.size(), MaximumNamesInSummary);

            for (size_t i = 0; i < shown; i++)
            {
                if (!text.empty())
                {
                    text += L", ";
                }

                text += names[i];
            }

            if (names.size() > shown)
            {
                text = std::wstring{ resources::FormatString(L"FilterSummaryMoreFormat",
                    text, static_cast<int>(names.size() - shown)) };
            }

            return text;
        }

        // Saying "no A, B, C, D, E, F, G, H, I, J" when only two things are kept is unreadable, so
        // whichever side is shorter is the one described.
        void DescribeGroup(
            _In_reads_(count) bool const* values,
            _In_ size_t count,
            _In_ std::function<winrt::hstring(size_t)> const& describe,
            _In_ std::wstring_view onlyKey,
            _In_ std::wstring_view noneKey,
            _In_ std::wstring_view nothingKey,
            _Inout_ std::vector<std::wstring>& parts)
        {
            std::vector<std::wstring> kept{};
            std::vector<std::wstring> dropped{};

            for (size_t i = 0; i < count; i++)
            {
                auto const name = describe(i);

                if (values[i])
                {
                    kept.push_back(std::wstring{ name });
                }
                else
                {
                    dropped.push_back(std::wstring{ name });
                }
            }

            if (dropped.empty())
            {
                return;
            }

            if (kept.empty())
            {
                parts.push_back(std::wstring{ resources::GetString(nothingKey) });
            }
            else if (kept.size() <= dropped.size())
            {
                parts.push_back(std::wstring{ resources::FormatString(onlyKey, JoinNames(kept)) });
            }
            else
            {
                parts.push_back(std::wstring{ resources::FormatString(noneKey, JoinNames(dropped)) });
            }
        }

        std::wstring JoinParts(_In_ std::vector<std::wstring> const& parts, _In_ std::wstring_view separator)
        {
            std::wstring text{};

            for (auto const& part : parts)
            {
                if (part.empty())
                {
                    continue;
                }

                if (!text.empty())
                {
                    text += separator;
                }

                text += part;
            }

            return text;
        }

        // "A", "A and B", "A, B and C". The joining word comes from resources.
        std::wstring JoinList(_In_ std::vector<std::wstring> const& items, _In_ std::wstring_view lastFormatKey)
        {
            if (items.empty())
            {
                return {};
            }

            if (items.size() == 1)
            {
                return items[0];
            }

            std::wstring head{};

            for (size_t i = 0; i + 1 < items.size(); i++)
            {
                if (!head.empty())
                {
                    head += L", ";
                }

                head += items[i];
            }

            return std::wstring{ resources::FormatString(lastFormatKey, head, items.back()) };
        }

        std::wstring Text(_In_ winrt::hstring const& value)
        {
            return std::wstring{ value };
        }

        std::wstring SignedSemitones(_In_ int32_t semitones)
        {
            return std::to_wstring(std::abs(semitones));
        }

        // The real-time messages most people want out of a route, in the order the filter lists them.
        bool KeepsOutOnlyRealTime(_In_ MessageFilter const& filter) noexcept
        {
            if (!AllTrue(filter.MessageTypes.data(), filter.MessageTypes.size()) ||
                !AllTrue(filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size()))
            {
                return false;
            }

            for (size_t i = 0; i < SystemMessageCount; i++)
            {
                auto const status = SystemMessageList[i];
                auto const isRealTime = status >= 0xF8;

                if (filter.SystemMessages[i] == isRealTime)
                {
                    return false;
                }
            }

            return true;
        }

        std::vector<std::wstring> ListedNumbers(
            _In_ std::array<bool, SevenBitValueCount> const& list,
            _In_ bool asNotes)
        {
            std::vector<std::wstring> names{};

            for (uint8_t i = 0; i < SevenBitValueCount; i++)
            {
                if (list[i])
                {
                    names.push_back(asNotes ? Text(DescribeNote(i)) : std::to_wstring(i));
                }
            }

            return names;
        }

        std::wstring DescribeValueSet(_In_ ValueSetFilter const& values, _In_ bool asNotes)
        {
            auto const letThrough = values.Action == FilterAction::LetThrough;

            auto const name = [asNotes](uint8_t value)
                {
                    return asNotes ? Text(DescribeNote(value)) : std::to_wstring(value);
                };

            if (values.PassesEverything())
            {
                return Text(resources::GetString(asNotes ? L"BlockDescEveryNote" : L"BlockDescEveryController"));
            }

            switch (values.Mode)
            {
            case ValueSetMode::One:
                return Text(resources::FormatString(asNotes
                    ? (letThrough ? L"BlockDescNoteOneLetFormat" : L"BlockDescNoteOneKeepFormat")
                    : (letThrough ? L"BlockDescControllerOneLetFormat" : L"BlockDescControllerOneKeepFormat"),
                    name(values.One)));

            case ValueSetMode::List:
            {
                auto const listed = ListedNumbers(values.List, asNotes);

                if (listed.empty())
                {
                    return Text(resources::GetString(asNotes ? L"BlockDescNoNotes" : L"BlockDescNoControllers"));
                }

                if (listed.size() > MaximumNamesInSummary)
                {
                    return Text(resources::FormatString(asNotes
                        ? (letThrough ? L"BlockDescNoteCountLetFormat" : L"BlockDescNoteCountKeepFormat")
                        : (letThrough ? L"BlockDescControllerCountLetFormat" : L"BlockDescControllerCountKeepFormat"),
                        static_cast<int>(listed.size())));
                }

                auto const items = JoinList(listed, L"BlockDescListAndFormat");

                if (asNotes)
                {
                    return Text(resources::FormatString(letThrough ? L"BlockDescNoteListLetFormat" : L"BlockDescNoteListKeepFormat", items));
                }

                return Text(resources::FormatString(letThrough
                    ? (listed.size() == 1 ? L"BlockDescControllerOneLetFormat" : L"BlockDescControllerListLetFormat")
                    : (listed.size() == 1 ? L"BlockDescControllerOneKeepFormat" : L"BlockDescControllerListKeepFormat"),
                    items));
            }

            default:
            {
                auto const low = (std::min)(values.Lowest, values.Highest);
                auto const high = (std::max)(values.Lowest, values.Highest);

                return Text(resources::FormatString(asNotes
                    ? (letThrough ? L"BlockDescNoteRangeLetFormat" : L"BlockDescNoteRangeKeepFormat")
                    : (letThrough ? L"BlockDescControllerRangeLetFormat" : L"BlockDescControllerRangeKeepFormat"),
                    name(low), name(high)));
            }
            }
        }

        std::wstring DescribeMapOne(
            _In_reads_(count) int16_t const* map,
            _In_ size_t count,
            _In_ int offset,
            _In_ std::wstring_view oneFormatKey,
            _In_ std::wstring_view manyFormatKey,
            _In_ bool asNotes)
        {
            auto const entries = CountMapEntries(map, count);

            if (entries == 0)
            {
                return {};
            }

            if (entries == 1)
            {
                for (size_t i = 0; i < count; i++)
                {
                    if (map[i] >= 0 && map[i] != static_cast<int16_t>(i))
                    {
                        if (asNotes)
                        {
                            return Text(resources::FormatString(oneFormatKey,
                                DescribeNote(static_cast<uint8_t>(i)), DescribeNote(static_cast<uint8_t>(map[i]))));
                        }

                        return Text(resources::FormatString(oneFormatKey,
                            static_cast<int>(i) + offset, static_cast<int>(map[i]) + offset));
                    }
                }
            }

            return Text(resources::FormatString(manyFormatKey, static_cast<int>(entries)));
        }

        std::wstring DescribeShape(_In_ ValueShape const& shape, _In_ ValueScale scale)
        {
            std::vector<std::wstring> parts{};

            if (shape.Invert)
            {
                parts.push_back(Text(resources::GetString(L"BlockDescInverted")));
            }

            if (shape.Curve == ValueCurve::SlowRise)
            {
                parts.push_back(Text(resources::GetString(L"BlockDescSlowRise")));
            }
            else if (shape.Curve == ValueCurve::FastRise)
            {
                parts.push_back(Text(resources::GetString(L"BlockDescFastRise")));
            }

            if (shape.InputMinimumHundredths != 0 || shape.InputMaximumHundredths != FullScaleHundredths)
            {
                parts.push_back(Text(resources::FormatString(L"BlockDescInputFormat",
                    DescribeScaledValue(shape.InputMinimumHundredths, scale),
                    DescribeScaledValue(shape.InputMaximumHundredths, scale))));
            }

            if (shape.OutputMinimumHundredths != 0 || shape.OutputMaximumHundredths != FullScaleHundredths)
            {
                parts.push_back(Text(resources::FormatString(L"BlockDescOutputFormat",
                    DescribeScaledValue(shape.OutputMinimumHundredths, scale),
                    DescribeScaledValue(shape.OutputMaximumHundredths, scale))));
            }

            return JoinParts(parts, L", ");
        }

        std::wstring DescribeCondition(_In_ MaskCondition const& condition, _In_ bool hex)
        {
            std::wstring values{};

            switch (condition.Match)
            {
            case MaskMatch::AnyOf:
            {
                std::vector<std::wstring> items{};

                for (auto const value : condition.Values)
                {
                    items.push_back(Text(DescribeMaskValue(value, hex)));
                }

                if (items.size() > MaximumNamesInSummary)
                {
                    values = Text(resources::FormatString(L"BlockDescValueCountFormat", static_cast<int>(items.size())));
                }
                else
                {
                    values = JoinList(items, L"BlockDescListOrFormat");
                }
                break;
            }

            case MaskMatch::Between:
                values = Text(resources::FormatString(L"BlockDescRangeFormat",
                    DescribeMaskValue((std::min)(condition.Lowest, condition.Highest), hex),
                    DescribeMaskValue((std::max)(condition.Lowest, condition.Highest), hex)));
                break;

            default:
                values = Text(DescribeMaskValue(condition.Value, hex));
                break;
            }

            if (condition.HighBit == condition.LowBit)
            {
                return Text(resources::FormatString(L"BlockDescMaskBitFormat",
                    static_cast<int>(condition.Word) + 1, static_cast<int>(condition.HighBit), values));
            }

            return Text(resources::FormatString(L"BlockDescMaskConditionFormat",
                static_cast<int>(condition.Word) + 1,
                static_cast<int>(condition.HighBit),
                static_cast<int>(condition.LowBit),
                values));
        }
    }

    _Use_decl_annotations_
    winrt::hstring SummarizeFilter(MessageFilter const& filter) noexcept
    {
        try
        {
            if (filter.PassesEverything())
            {
                return resources::GetString(L"FilterSummaryEverything");
            }

            std::vector<std::wstring> parts{};

            DescribeGroup(filter.MessageTypes.data(), MessageTypeCount,
                [](size_t i) { return DescribeMessageType(static_cast<uint8_t>(i)); },
                L"FilterSummaryOnlyTypesFormat", L"FilterSummaryNoTypesFormat",
                L"FilterSummaryNothingFormat", parts);

            DescribeGroup(filter.ChannelVoiceStatuses.data(), ChannelVoiceStatusCount,
                [](size_t i) { return DescribeChannelVoiceStatus(static_cast<uint8_t>(i)); },
                L"FilterSummaryOnlyMessagesFormat", L"FilterSummaryNoMessagesFormat",
                L"FilterSummaryNoChannelMessages", parts);

            DescribeGroup(filter.SystemMessages.data(), SystemMessageCount,
                [](size_t i) { return DescribeSystemMessage(SystemMessageList[i]); },
                L"FilterSummaryOnlyMessagesFormat", L"FilterSummaryNoMessagesFormat",
                L"FilterSummaryNoSystemMessages", parts);

            if (!AllTrue(filter.Channels.data(), filter.Channels.size()))
            {
                auto const runs = DescribeRuns(filter.Channels.data(), ChannelCount, 1);

                parts.push_back(std::wstring{ resources::FormatString(L"FilterSummaryChannelsFormat", runs) });
            }

            if (filter.LimitNoteRange)
            {
                parts.push_back(std::wstring{ resources::FormatString(L"FilterSummaryNotesFormat",
                    DescribeNote(filter.LowestAllowedNote), DescribeNote(filter.HighestAllowedNote)) });
            }

            if (parts.empty())
            {
                return resources::GetString(L"FilterSummaryEverything");
            }

            return winrt::hstring{ JoinParts(parts, L". ") };
        }
        catch (...)
        {
        }

        return resources::GetString(L"FilterSummaryEverything");
    }

    _Use_decl_annotations_
    winrt::hstring DescribeScaledValue(int32_t hundredths, ValueScale scale) noexcept
    {
        try
        {
            if (scale == ValueScale::SevenBit)
            {
                return winrt::hstring{ std::to_wstring(SevenBitFromHundredths(hundredths)) };
            }

            auto const clamped = std::clamp(hundredths, 0, FullScaleHundredths);

            // A whole percentage reads better without its zeros.
            if (clamped % 100 == 0)
            {
                return resources::FormatString(L"BlockWholePercentFormat", clamped / 100);
            }

            return resources::FormatString(L"TransformPercentFormat",
                clamped / 100, clamped % 100);
        }
        catch (...)
        {
        }

        return {};
    }

    namespace
    {
        struct KindText
        {
            BlockKind Kind;
            wchar_t const* Name;
            wchar_t const* Short;
            wchar_t const* Badge;
            wchar_t const* Hint;
        };

        constexpr KindText KindTexts[] =
        {
            { BlockKind::MessageTypeFilter, L"BlockNameMessageTypeFilter", L"BlockShortMessageTypeFilter", L"BlockBadgeMessageTypeFilter", L"BlockHintMessageTypeFilter" },
            { BlockKind::GroupFilter, L"BlockNameGroupFilter", L"BlockShortGroupFilter", L"BlockBadgeGroupFilter", L"BlockHintGroupFilter" },
            { BlockKind::ChannelFilter, L"BlockNameChannelFilter", L"BlockShortChannelFilter", L"BlockBadgeChannelFilter", L"BlockHintChannelFilter" },
            { BlockKind::NoteFilter, L"BlockNameNoteFilter", L"BlockShortNoteFilter", L"BlockBadgeNoteFilter", L"BlockHintNoteFilter" },
            { BlockKind::ControlChangeFilter, L"BlockNameControlChangeFilter", L"BlockShortControlChangeFilter", L"BlockBadgeControlChangeFilter", L"BlockHintControlChangeFilter" },
            { BlockKind::VelocityFilter, L"BlockNameVelocityFilter", L"BlockShortVelocityFilter", L"BlockBadgeVelocityFilter", L"BlockHintVelocityFilter" },
            { BlockKind::MessageMaskFilter, L"BlockNameMessageMaskFilter", L"BlockShortMessageMaskFilter", L"BlockBadgeMessageMaskFilter", L"BlockHintMessageMaskFilter" },
            { BlockKind::ChannelMap, L"BlockNameChannelMap", L"BlockShortChannelMap", L"BlockBadgeChannelMap", L"BlockHintChannelMap" },
            { BlockKind::GroupMap, L"BlockNameGroupMap", L"BlockShortGroupMap", L"BlockBadgeGroupMap", L"BlockHintGroupMap" },
            { BlockKind::NoteMap, L"BlockNameNoteMap", L"BlockShortNoteMap", L"BlockBadgeNoteMap", L"BlockHintNoteMap" },
            { BlockKind::Transpose, L"BlockNameTranspose", L"BlockShortTranspose", L"BlockBadgeTranspose", L"BlockHintTranspose" },
            { BlockKind::Velocity, L"BlockNameVelocity", L"BlockShortVelocity", L"BlockBadgeVelocity", L"BlockHintVelocity" },
            { BlockKind::Aftertouch, L"BlockNameAftertouch", L"BlockShortAftertouch", L"BlockBadgeAftertouch", L"BlockHintAftertouch" },
            { BlockKind::ControlChangeMap, L"BlockNameControlChangeMap", L"BlockShortControlChangeMap", L"BlockBadgeControlChangeMap", L"BlockHintControlChangeMap" },
            { BlockKind::ControlChangeValue, L"BlockNameControlChangeValue", L"BlockShortControlChangeValue", L"BlockBadgeControlChangeValue", L"BlockHintControlChangeValue" },
            { BlockKind::ProgramMap, L"BlockNameProgramMap", L"BlockShortProgramMap", L"BlockBadgeProgramMap", L"BlockHintProgramMap" },
            { BlockKind::Throttle, L"BlockNameThrottle", L"BlockShortThrottle", L"BlockBadgeThrottle", L"BlockHintThrottle" },
            { BlockKind::ClockDivider, L"BlockNameClockDivider", L"BlockShortClockDivider", L"BlockBadgeClockDivider", L"BlockHintClockDivider" },
            { BlockKind::ClockGenerator, L"BlockNameClockGenerator", L"BlockShortClockGenerator", L"BlockBadgeClockGenerator", L"BlockHintClockGenerator" },
            { BlockKind::TimeCodeGenerator, L"BlockNameTimeCodeGenerator", L"BlockShortTimeCodeGenerator", L"BlockBadgeTimeCodeGenerator", L"BlockHintTimeCodeGenerator" },
            { BlockKind::LfoGenerator, L"BlockNameLfoGenerator", L"BlockShortLfoGenerator", L"BlockBadgeLfoGenerator", L"BlockHintLfoGenerator" },
            { BlockKind::Annotation, L"BlockNameAnnotation", L"BlockShortAnnotation", L"BlockBadgeAnnotation", L"BlockHintAnnotation" },
            { BlockKind::ParameterFilter, L"BlockNameParameterFilter", L"BlockShortParameterFilter", L"BlockBadgeParameterFilter", L"BlockHintParameterFilter" },
            { BlockKind::ParameterTransform, L"BlockNameParameterTransform", L"BlockShortParameterTransform", L"BlockBadgeParameterTransform", L"BlockHintParameterTransform" },
            { BlockKind::NoteDistributor, L"BlockNameNoteDistributor", L"BlockShortNoteDistributor", L"BlockBadgeNoteDistributor", L"BlockHintNoteDistributor" },
            { BlockKind::Gate, L"BlockNameGate", L"BlockShortGate", L"BlockBadgeGate", L"BlockHintGate" },
            { BlockKind::CiResponder, L"BlockNameCiResponder", L"BlockShortCiResponder", L"BlockBadgeCiResponder", L"BlockHintCiResponder" },
            { BlockKind::CiFilter, L"BlockNameCiFilter", L"BlockShortCiFilter", L"BlockBadgeCiFilter", L"BlockHintCiFilter" },
            { BlockKind::Branch, L"BlockNameBranch", L"BlockShortBranch", L"BlockBadgeBranch", L"BlockHintBranch" },
            { BlockKind::Switch, L"BlockNameSwitch", L"BlockShortSwitch", L"BlockBadgeSwitch", L"BlockHintSwitch" },
            { BlockKind::SetTag, L"BlockNameSetTag", L"BlockShortSetTag", L"BlockBadgeSetTag", L"BlockHintSetTag" },
            { BlockKind::SetMemory, L"BlockNameSetMemory", L"BlockShortSetMemory", L"BlockBadgeSetMemory", L"BlockHintSetMemory" },
            { BlockKind::PutValue, L"BlockNamePutValue", L"BlockShortPutValue", L"BlockBadgePutValue", L"BlockHintPutValue" },
        };

        KindText const* FindKindText(_In_ BlockKind kind) noexcept
        {
            for (auto const& entry : KindTexts)
            {
                if (entry.Kind == kind)
                {
                    return &entry;
                }
            }

            return nullptr;
        }
    }

    _Use_decl_annotations_
    winrt::hstring BlockKindName(BlockKind kind) noexcept
    {
        auto const* text = FindKindText(kind);
        return text == nullptr ? winrt::hstring{} : resources::GetString(text->Name);
    }

    _Use_decl_annotations_
    winrt::hstring BlockKindShortName(BlockKind kind) noexcept
    {
        auto const* text = FindKindText(kind);
        return text == nullptr ? winrt::hstring{} : resources::GetString(text->Short);
    }

    _Use_decl_annotations_
    winrt::hstring BlockKindBadge(BlockKind kind) noexcept
    {
        auto const* text = FindKindText(kind);
        return text == nullptr ? winrt::hstring{} : resources::GetString(text->Badge);
    }

    _Use_decl_annotations_
    winrt::hstring BlockKindHint(BlockKind kind) noexcept
    {
        auto const* text = FindKindText(kind);
        return text == nullptr ? winrt::hstring{} : resources::GetString(text->Hint);
    }

    _Use_decl_annotations_
    winrt::hstring BlockCategoryName(BlockCategory category) noexcept
    {
        switch (category)
        {
        case BlockCategory::Transform:  return resources::GetString(L"BlockCategoryTransforms");
        case BlockCategory::Sending:    return resources::GetString(L"BlockCategorySending");
        case BlockCategory::Generator:  return resources::GetString(L"BlockCategoryGenerators");
        case BlockCategory::Annotation: return resources::GetString(L"BlockCategoryAnnotations");
        case BlockCategory::Distribution: return resources::GetString(L"BlockCategoryDistribution");
        case BlockCategory::CapabilityInquiry: return resources::GetString(L"BlockCategoryCapabilityInquiry");
        case BlockCategory::Logic:      return resources::GetString(L"BlockCategoryLogic");
        default:                        return resources::GetString(L"BlockCategoryFilters");
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeMaskValue(uint32_t value, bool hex) noexcept
    {
        try
        {
            if (hex)
            {
                return winrt::hstring{ std::format(L"0x{:X}", value) };
            }

            return winrt::hstring{ std::to_wstring(value) };
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeSendSpeed(uint32_t speed) noexcept
    {
        if (speed == 0)
        {
            return resources::GetString(L"SendSpeedLimitUnlimited");
        }

        if (speed == 1)
        {
            return resources::GetString(L"SendSpeedLimitWireSpeed");
        }

        return resources::FormatString(L"SendSpeedLimitMultipleFormat", static_cast<int>(speed));
    }

    namespace
    {
        // At most this many places, without trailing zeros, so 120 reads as "120" and not "120.00".
        std::wstring TrimmedNumber(_In_ double value, _In_ int places)
        {
            auto text = std::format(L"{:.{}f}", value, places);

            if (text.find(L'.') != std::wstring::npos)
            {
                while (!text.empty() && text.back() == L'0')
                {
                    text.pop_back();
                }

                if (!text.empty() && text.back() == L'.')
                {
                    text.pop_back();
                }
            }

            return text;
        }

        constexpr wchar_t const* LfoWaveKeys[]
        {
            L"LfoWaveSine", L"LfoWaveTriangle", L"LfoWaveSquare", L"LfoWaveRampUp",
            L"LfoWaveRampDown", L"LfoWaveWhite", L"LfoWavePink", L"LfoWaveBrown", L"LfoWaveBlue",
        };

        static_assert(std::size(midiapp::LfoWaveOrder) == std::size(LfoWaveKeys));

        constexpr wchar_t const* LfoRateKeys[]
        {
            L"LfoRateSixteenth", L"LfoRateEighth", L"LfoRateQuarter", L"LfoRateDottedQuarter",
            L"LfoRateHalf", L"LfoRateDottedHalf", L"LfoRateBar", L"LfoRateTwoBars",
            L"LfoRateFourBars", L"LfoRateEightBars",
        };

        static_assert(std::size(midiapp::LfoRateChoices) == std::size(LfoRateKeys));

        winrt::hstring DescribeLfoTarget(_In_ midiapp::ValueMessageTarget const& target)
        {
            auto const bank = static_cast<int>(target.Number >> 7);
            auto const index = static_cast<int>(target.Number & 0x7F);

            switch (target.Kind)
            {
            case midiapp::ValueMessageKind::PitchBend:
                return resources::GetString(L"LfoTargetPitchBend");

            case midiapp::ValueMessageKind::ChannelPressure:
                return resources::GetString(L"LfoTargetChannelPressure");

            case midiapp::ValueMessageKind::PolyPressure:
                return resources::FormatString(L"LfoTargetPolyPressureFormat", DescribeNote(static_cast<uint8_t>(index)));

            case midiapp::ValueMessageKind::RegisteredController:
                return resources::FormatString(L"LfoTargetRegisteredFormat", bank, index);

            case midiapp::ValueMessageKind::AssignableController:
                return resources::FormatString(L"LfoTargetAssignableFormat", bank, index);

            default:
                return resources::FormatString(L"LfoTargetControlChangeFormat", index);
            }
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeNumber(double value, int places) noexcept
    {
        try
        {
            return winrt::hstring{ TrimmedNumber(value, std::clamp(places, 0, 6)) };
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeTempo(double beatsPerMinute) noexcept
    {
        return DescribeNumber(beatsPerMinute, 2);
    }

    _Use_decl_annotations_
    winrt::hstring DescribeFrameRate(midiapp::MidiTimeCodeFrameRate rate, bool forPicker) noexcept
    {
        switch (rate)
        {
        case midiapp::MidiTimeCodeFrameRate::Frames24:
            return resources::GetString(forPicker ? L"FrameRate24" : L"FrameRateShort24");

        case midiapp::MidiTimeCodeFrameRate::Frames25:
            return resources::GetString(forPicker ? L"FrameRate25" : L"FrameRateShort25");

        case midiapp::MidiTimeCodeFrameRate::Frames2997Drop:
            return resources::GetString(forPicker ? L"FrameRate2997Drop" : L"FrameRateShort2997Drop");

        default:
            return resources::GetString(forPicker ? L"FrameRate30" : L"FrameRateShort30");
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeLfoWave(midiapp::LfoWave wave) noexcept
    {
        for (size_t i = 0; i < std::size(midiapp::LfoWaveOrder); i++)
        {
            if (midiapp::LfoWaveOrder[i] == wave)
            {
                return resources::GetString(LfoWaveKeys[i]);
            }
        }

        return resources::GetString(LfoWaveKeys[0]);
    }

    _Use_decl_annotations_
    winrt::hstring DescribeLfoLength(double beatsPerCycle) noexcept
    {
        try
        {
            for (size_t i = 0; i < std::size(midiapp::LfoRateChoices); i++)
            {
                if (std::abs(midiapp::LfoRateChoices[i] - beatsPerCycle) < 0.0001)
                {
                    return resources::GetString(LfoRateKeys[i]);
                }
            }

            return resources::FormatString(L"BlockDescLfoBeatsFormat", TrimmedNumber(beatsPerCycle, 3));
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeValueMessageKind(midiapp::ValueMessageKind kind) noexcept
    {
        switch (kind)
        {
        case midiapp::ValueMessageKind::PitchBend:              return resources::GetString(L"LfoMessagePitchBend");
        case midiapp::ValueMessageKind::ChannelPressure:        return resources::GetString(L"LfoMessageChannelPressure");
        case midiapp::ValueMessageKind::PolyPressure:           return resources::GetString(L"LfoMessagePolyPressure");
        case midiapp::ValueMessageKind::RegisteredController:   return resources::GetString(L"LfoMessageRegistered");
        case midiapp::ValueMessageKind::AssignableController:   return resources::GetString(L"LfoMessageAssignable");
        default:                                                return resources::GetString(L"LfoMessageControlChange");
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeBlock(BlockKind kind, BlockSettings const& settings) noexcept
    {
        try
        {
            auto const& transform = settings.Transform;
            std::wstring text{};

            switch (kind)
            {
            case BlockKind::MessageTypeFilter:
                if (settings.Filter.PassesEverything())
                {
                    text = Text(resources::GetString(L"BlockDescEverything"));
                }
                else if (KeepsOutOnlyRealTime(settings.Filter))
                {
                    text = Text(resources::GetString(L"BlockDescNoRealTime"));
                }
                else
                {
                    text = Text(SummarizeFilter(settings.Filter));
                }
                break;

            case BlockKind::ChannelFilter:
            {
                auto const& channels = settings.Filter.Channels;
                auto const kept = std::count(channels.begin(), channels.end(), true);

                if (kept == static_cast<ptrdiff_t>(channels.size()))
                {
                    text = Text(resources::GetString(L"BlockDescEverything"));
                }
                else if (kept == 0)
                {
                    text = Text(resources::GetString(L"BlockDescNoChannels"));
                }
                else
                {
                    text = Text(resources::FormatString(kept == 1 ? L"BlockDescChannelOneFormat" : L"BlockDescChannelsFormat",
                        DescribeRuns(channels.data(), channels.size(), 1)));
                }
                break;
            }

            case BlockKind::GroupFilter:
            {
                auto const kept = std::count(settings.Groups.begin(), settings.Groups.end(), true);

                if (kept == static_cast<ptrdiff_t>(settings.Groups.size()))
                {
                    text = Text(resources::GetString(L"BlockDescEverything"));
                }
                else if (kept == 0)
                {
                    text = Text(resources::GetString(L"BlockDescNoGroups"));
                }
                else
                {
                    text = Text(resources::FormatString(kept == 1 ? L"BlockDescGroupOneFormat" : L"BlockDescGroupsFormat",
                        DescribeRuns(settings.Groups.data(), settings.Groups.size(), 1)));
                }
                break;
            }

            case BlockKind::NoteFilter:
                text = DescribeValueSet(settings.Values, true);
                break;

            case BlockKind::ControlChangeFilter:
                text = DescribeValueSet(settings.Values, false);
                break;

            case BlockKind::VelocityFilter:
            {
                auto const& range = settings.Velocities;

                if (range.PassesEverything())
                {
                    text = Text(resources::GetString(L"BlockDescEveryNote"));
                }
                else
                {
                    text = Text(resources::FormatString(
                        range.Action == FilterAction::LetThrough ? L"BlockDescVelocityLetFormat" : L"BlockDescVelocityKeepFormat",
                        DescribeScaledValue((std::min)(range.LowestHundredths, range.HighestHundredths), range.Scale),
                        DescribeScaledValue((std::max)(range.LowestHundredths, range.HighestHundredths), range.Scale)));
                }
                break;
            }

            case BlockKind::MessageMaskFilter:
            {
                auto const& mask = settings.Mask;

                if (mask.Conditions.empty())
                {
                    text = Text(resources::GetString(L"BlockDescMaskNone"));
                    break;
                }

                auto const where = mask.Conditions.size() == 1
                    ? DescribeCondition(mask.Conditions.front(), mask.ShowHex)
                    : Text(resources::FormatString(L"BlockDescMaskManyFormat", static_cast<int>(mask.Conditions.size())));

                text = Text(resources::FormatString(
                    mask.Action == FilterAction::LetThrough ? L"BlockDescMaskLetFormat" : L"BlockDescMaskKeepFormat",
                    static_cast<int>(mask.WordCount), where));
                break;
            }

            case BlockKind::ChannelMap:
                text = DescribeMapOne(transform.ChannelMap.data(), transform.ChannelMap.size(), 1,
                    L"BlockDescChannelMapOneFormat", L"BlockDescChannelMapFormat", false);
                break;

            case BlockKind::GroupMap:
            {
                std::array<int16_t, 16> wide{};

                for (size_t i = 0; i < wide.size(); i++)
                {
                    wide[i] = settings.GroupMap[i];
                }

                text = DescribeMapOne(wide.data(), wide.size(), 1,
                    L"BlockDescGroupMapOneFormat", L"BlockDescGroupMapFormat", false);
                break;
            }

            case BlockKind::NoteMap:
                text = DescribeMapOne(transform.NoteMap.data(), transform.NoteMap.size(), 0,
                    L"BlockDescNoteMapOneFormat", L"BlockDescNoteMapFormat", true);
                break;

            case BlockKind::Transpose:
            {
                auto const semitones = transform.TransposeSemitones;

                if (semitones != 0)
                {
                    auto const up = semitones > 0;
                    auto const one = std::abs(semitones) == 1;

                    text = one
                        ? Text(resources::GetString(up ? L"BlockDescTransposeUpOne" : L"BlockDescTransposeDownOne"))
                        : Text(resources::FormatString(up ? L"BlockDescTransposeUpFormat" : L"BlockDescTransposeDownFormat",
                            SignedSemitones(semitones)));
                }
                break;
            }

            case BlockKind::Velocity:
            {
                std::vector<std::wstring> parts{};

                if (transform.Curve == VelocityCurve::LinearToCurved)
                {
                    parts.push_back(Text(resources::GetString(L"BlockDescVelocityCurved")));
                }
                else if (transform.Curve == VelocityCurve::CurvedToLinear)
                {
                    parts.push_back(Text(resources::GetString(L"BlockDescVelocityLinear")));
                }
                else if (transform.Curve == VelocityCurve::Fixed)
                {
                    parts.push_back(Text(resources::FormatString(L"BlockDescVelocityFixedFormat",
                        DescribeScaledValue(transform.FixedVelocityHundredths, transform.Scale))));
                }

                if (transform.RescaleVelocity && transform.Curve != VelocityCurve::Fixed)
                {
                    parts.push_back(Text(resources::FormatString(L"BlockDescRangeFormat",
                        DescribeScaledValue(transform.MinimumVelocityHundredths, transform.Scale),
                        DescribeScaledValue(transform.MaximumVelocityHundredths, transform.Scale))));
                }

                text = JoinParts(parts, L", ");
                break;
            }

            case BlockKind::Aftertouch:
                text = DescribeShape(transform.AftertouchShape, transform.Scale);
                break;

            case BlockKind::ControlChangeMap:
                text = DescribeMapOne(transform.ControlMap.data(), transform.ControlMap.size(), 0,
                    L"BlockDescControlMapOneFormat", L"BlockDescControlMapFormat", false);
                break;

            case BlockKind::ControlChangeValue:
            {
                std::vector<size_t> shaped{};

                for (size_t i = 0; i < transform.ControlValueShapes.size(); i++)
                {
                    if (!transform.ControlValueShapes[i].ChangesNothing())
                    {
                        shaped.push_back(i);
                    }
                }

                if (shaped.size() == 1)
                {
                    text = Text(resources::FormatString(L"BlockDescControlValueOneFormat",
                        static_cast<int>(shaped.front()),
                        DescribeShape(transform.ControlValueShapes[shaped.front()], transform.Scale)));
                }
                else if (!shaped.empty())
                {
                    text = Text(resources::FormatString(L"BlockDescControlValueFormat", static_cast<int>(shaped.size())));
                }
                break;
            }

            case BlockKind::ProgramMap:
            {
                std::vector<std::wstring> parts{};

                auto const programs = CountMapEntries(transform.ProgramMap.data(), transform.ProgramMap.size());
                auto const banks =
                    CountMapEntries(transform.BankMsbMap.data(), transform.BankMsbMap.size()) +
                    CountMapEntries(transform.BankLsbMap.data(), transform.BankLsbMap.size());

                if (programs == 1 && banks == 0)
                {
                    text = DescribeMapOne(transform.ProgramMap.data(), transform.ProgramMap.size(), 0,
                        L"BlockDescProgramMapOneFormat", L"BlockDescProgramsFormat", false);
                    break;
                }

                if (programs > 0)
                {
                    parts.push_back(Text(programs == 1
                        ? resources::GetString(L"BlockDescProgramsOne")
                        : resources::FormatString(L"BlockDescProgramsFormat", static_cast<int>(programs))));
                }

                if (banks > 0)
                {
                    parts.push_back(Text(banks == 1
                        ? resources::GetString(L"BlockDescBanksOne")
                        : resources::FormatString(L"BlockDescBanksFormat", static_cast<int>(banks))));
                }

                if (!parts.empty())
                {
                    text = Text(resources::FormatString(L"BlockDescRemapsFormat", JoinList(parts, L"BlockDescListAndFormat")));
                }
                break;
            }

            case BlockKind::Throttle:
                text = settings.SendSpeedLimit == 0
                    ? Text(resources::GetString(L"BlockDescThrottleUnlimited"))
                    : Text(resources::FormatString(L"BlockDescThrottleFormat", DescribeSendSpeed(settings.SendSpeedLimit)));
                break;

            case BlockKind::ClockDivider:
                if (settings.ClockDivision > 1)
                {
                    text = Text(resources::FormatString(L"BlockDescClockDivideFormat", static_cast<int>(settings.ClockDivision)));
                }
                break;

            case BlockKind::ClockGenerator:
            {
                auto const& clock = settings.Clock;
                std::vector<std::wstring> parts{};

                parts.push_back(Text(resources::FormatString(L"BlockDescClockFormat", DescribeTempo(clock.BeatsPerMinute))));

                auto const swing = static_cast<int>(std::lround(clock.SwingPercent));

                if (swing > 50)
                {
                    parts.push_back(Text(resources::FormatString(L"BlockDescSwingFormat", swing)));
                }

                if (!clock.SendStartStop)
                {
                    parts.push_back(Text(resources::GetString(L"BlockDescNoStartStop")));
                }

                text = JoinParts(parts, L", ");
                break;
            }

            case BlockKind::TimeCodeGenerator:
                text = Text(resources::FormatString(L"BlockDescTimeCodeFormat",
                    DescribeFrameRate(settings.TimeCode.FrameRate, false),
                    midiapp::FormatPosition(settings.TimeCode.Start, settings.TimeCode.FrameRate)));
                break;

            case BlockKind::LfoGenerator:
            {
                auto const& lfo = settings.Lfo;

                text = Text(resources::FormatString(L"BlockDescLfoFormat",
                    DescribeLfoWave(lfo.Wave),
                    DescribeLfoTarget(lfo.Target),
                    static_cast<int>(lfo.Target.Channel) + 1,
                    DescribeLfoLength(lfo.BeatsPerCycle)));
                break;
            }

            case BlockKind::Annotation:
                text = settings.Annotation.Text.empty()
                    ? Text(resources::GetString(L"BlockDescAnnotationEmpty"))
                    : settings.Annotation.Text;
                break;

            case BlockKind::ParameterFilter:
            {
                auto const& filter = settings.ParameterFilter;
                auto const count = static_cast<int>(filter.Parameters.size());

                if (count == 1)
                {
                    text = Text(resources::FormatString(filter.Action == FilterAction::KeepOut
                        ? L"BlockDescParameterKeepOutOneFormat" : L"BlockDescParameterOnlyOneFormat",
                        DescribeParameter(filter.Parameters.front().Kind, filter.Parameters.front().Bank, filter.Parameters.front().Index)));
                }
                else if (count > 1)
                {
                    text = Text(resources::FormatString(filter.Action == FilterAction::KeepOut
                        ? L"BlockDescParameterKeepOutFormat" : L"BlockDescParameterOnlyFormat", count));
                }
                break;
            }

            case BlockKind::ParameterTransform:
            {
                auto const& rows = settings.ParameterTransform.Rows;

                if (rows.size() == 1)
                {
                    auto const& row = rows.front();

                    text = Text(resources::FormatString(L"BlockDescParameterMoveFormat",
                        DescribeParameter(row.From.Kind, row.From.Bank, row.From.Index),
                        DescribeParameter(row.ToKind == ParameterKind::Either ? row.From.Kind : row.ToKind,
                            row.ToBank >= 0 ? row.ToBank : row.From.Bank,
                            row.ToIndex >= 0 ? row.ToIndex : row.From.Index)));
                }
                else if (!rows.empty())
                {
                    text = Text(resources::FormatString(L"BlockDescParameterRowsFormat", static_cast<int>(rows.size())));
                }
                break;
            }

            case BlockKind::NoteDistributor:
                switch (settings.Distributor.Mode)
                {
                case DistributionMode::FirstFree:    text = Text(resources::GetString(L"BlockDescDistributeFirstFree")); break;
                case DistributionMode::HighestNotes: text = Text(resources::GetString(L"BlockDescDistributeHighest")); break;
                case DistributionMode::LowestNotes:  text = Text(resources::GetString(L"BlockDescDistributeLowest")); break;
                default:                             text = Text(resources::GetString(L"BlockDescDistributeTurns")); break;
                }
                break;

            case BlockKind::Gate:
                text = Text(resources::FormatString(L"BlockDescGateFormat",
                    DescribeGateTrigger(settings.Gate.Open), DescribeGateTrigger(settings.Gate.Close)));
                break;

            case BlockKind::CiResponder:
                text = settings.CiResponder.FileName.empty()
                    ? Text(resources::GetString(L"BlockDescCiResponder"))
                    : Text(resources::FormatString(L"BlockDescCiResponderFileFormat", settings.CiResponder.FileName));
                break;

            case BlockKind::CiFilter:
            {
                auto const& filter = settings.CiFilter;
                auto const keepOut = filter.Action == FilterAction::KeepOut;
                auto const categories = static_cast<uint8_t>(filter.Categories & CiCategoryAll);

                if (categories == 0)
                {
                    text = Text(resources::GetString(keepOut ? L"BlockDescNothing" : L"BlockDescCiOnlyNone"));
                }
                else if (categories == CiCategoryAll)
                {
                    text = Text(resources::GetString(keepOut ? L"BlockDescCiKeepOutAll" : L"BlockDescCiOnlyAll"));
                }
                else
                {
                    text = Text(resources::FormatString(keepOut ? L"BlockDescCiKeepOutFormat" : L"BlockDescCiOnlyFormat",
                        DescribeCiCategories(categories)));
                }
                break;
            }

            case BlockKind::Branch:
            {
                auto const& branch = settings.Branch;

                text = branch.Condition.Test == LogicTest::Anything
                    ? Text(resources::GetString(L"BlockDescBranchAnything"))
                    : Text(resources::FormatString(L"BlockDescBranchFormat",
                        DescribeLogicSource(branch.Subject, branch.Scale),
                        DescribeCondition(branch.Condition, branch.Unit, branch.Scale, false)));
                break;
            }

            case BlockKind::Switch:
            {
                auto const& choice = settings.Switch;

                text = choice.Cases.empty()
                    ? Text(resources::GetString(L"BlockDescSwitchNone"))
                    : Text(resources::FormatString(L"BlockDescSwitchFormat",
                        DescribeLogicSource(choice.Subject, choice.Scale), static_cast<int>(choice.Cases.size() + 1)));
                break;
            }

            case BlockKind::SetTag:
                if (!settings.SetTag.Tag.empty())
                {
                    text = Text(resources::FormatString(L"BlockDescAssignFormat",
                        resources::FormatString(L"LogicTagFormat", settings.SetTag.Tag),
                        DescribeLogicSource(settings.SetTag.Value, settings.SetTag.Scale)));
                }
                break;

            case BlockKind::SetMemory:
            {
                auto const& memory = settings.SetMemory;

                if (memory.Memory.empty())
                {
                    break;
                }

                auto const name = resources::FormatString(L"LogicMemoryFormat", memory.Memory);

                switch (memory.Action)
                {
                case MemoryAction::Toggle:
                    text = Text(resources::FormatString(L"BlockDescToggleFormat", name,
                        DescribeUnitNumber(memory.First, memory.Unit, memory.Scale, true),
                        DescribeUnitNumber(memory.Second, memory.Unit, memory.Scale, true)));
                    break;

                case MemoryAction::StepUp:
                case MemoryAction::StepDown:
                {
                    // Steps are whole numbers, shown the way the memory's unit counts them.
                    auto const unit = memory.Unit == LogicUnit::Value ? LogicUnit::Number : memory.Unit;

                    text = Text(resources::FormatString(memory.Action == MemoryAction::StepUp ? L"BlockDescStepUpFormat" : L"BlockDescStepDownFormat",
                        name,
                        DescribeUnitNumber(memory.Lowest, unit, memory.Scale, false),
                        DescribeUnitNumber(memory.Highest, unit, memory.Scale, false)));
                    break;
                }

                case MemoryAction::Clear:
                    text = Text(resources::FormatString(L"BlockDescClearFormat", name));
                    break;

                default:
                    text = Text(resources::FormatString(L"BlockDescAssignFormat", name, DescribeLogicSource(memory.Value, memory.Scale)));
                    break;
                }

                if (!memory.EveryMessage)
                {
                    text = Text(resources::FormatString(L"BlockDescOnTriggerFormat", DescribeGateTrigger(memory.Trigger), text));
                }
                break;
            }

            case BlockKind::PutValue:
            {
                auto const& put = settings.PutValue;
                auto const named = put.Value.Kind == LogicSourceKind::Tag || put.Value.Kind == LogicSourceKind::Memory;

                if (!named || !put.Value.Name.empty())
                {
                    text = Text(resources::FormatString(L"BlockDescAssignFormat",
                        DescribePartPlace(put.Target), DescribeLogicSource(put.Value, put.Scale)));
                }
                break;
            }

            default:
                break;
            }

            if (text.empty())
            {
                text = Text(resources::GetString(L"BlockDescNothing"));
            }

            return winrt::hstring{ text };
        }
        catch (...)
        {
        }

        return resources::GetString(L"BlockDescNothing");
    }

    _Use_decl_annotations_
    winrt::hstring DescribeCiCategories(uint8_t categories) noexcept
    {
        try
        {
            constexpr wchar_t const* Keys[]{ L"CiCategoryManagement", L"CiCategoryProfiles", L"CiCategoryPropertyExchange", L"CiCategoryProcessInquiry" };

            std::wstring text{};

            for (size_t i = 0; i < std::size(Keys); i++)
            {
                if ((categories & (1u << i)) == 0)
                {
                    continue;
                }

                if (!text.empty())
                {
                    text += L", ";
                }

                text += resources::GetString(Keys[i]);
            }

            return winrt::hstring{ text };
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeCiMessage(uint8_t messageType) noexcept
    {
        try
        {
            wchar_t const* key{ nullptr };

            switch (messageType)
            {
            case 0x70: key = L"CiMessageDiscovery"; break;
            case 0x72: key = L"CiMessageEndpoint"; break;
            case 0x7E: key = L"CiMessageInvalidateMuid"; break;
            case 0x20: key = L"CiMessageProfileInquiry"; break;
            case 0x22: key = L"CiMessageSetProfileOn"; break;
            case 0x23: key = L"CiMessageSetProfileOff"; break;
            case 0x28: key = L"CiMessageProfileDetails"; break;
            case 0x30: key = L"CiMessagePropertyCapabilities"; break;
            case 0x34: key = L"CiMessageGetProperty"; break;
            case 0x36: key = L"CiMessageSetProperty"; break;
            case 0x38: key = L"CiMessageSubscription"; break;
            case 0x40: key = L"CiMessageProcessInquiryCapabilities"; break;
            case 0x42: key = L"CiMessageReport"; break;
            default: break;
            }

            if (key != nullptr)
            {
                return resources::GetString(key);
            }

            return resources::FormatString(L"CiMessageOtherFormat", std::format(L"{:02X}", messageType));
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeCiFileProblem(CiFileProblem const& problem) noexcept
    {
        // People count from 1.
        auto const number = problem.Index + 1;

        switch (problem.Kind)
        {
        case CiFileProblemKind::UnknownKey:
            switch (problem.Section)
            {
            case CiFileSection::Profiles:   return resources::FormatString(L"CiProblemProfileKeyFormat", number, problem.Key);
            case CiFileSection::Resources:  return resources::FormatString(L"CiProblemPropertyKeyFormat", number, problem.Key);
            case CiFileSection::DeviceInfo: return resources::FormatString(L"CiProblemDeviceInfoKeyFormat", problem.Key);
            default:                        return resources::FormatString(L"CiProblemFileKeyFormat", problem.Key);
            }

        case CiFileProblemKind::BadProfile:
            return problem.Index < 0
                ? resources::GetString(L"CiProblemProfilesNotList")
                : resources::FormatString(L"CiProblemBadProfileFormat", number);

        case CiFileProblemKind::DuplicateProfile:
            return resources::FormatString(L"CiProblemDuplicateProfileFormat", number);

        case CiFileProblemKind::BadDeviceInfo:
            return problem.Key.empty()
                ? resources::GetString(L"CiProblemDeviceInfoNotObject")
                : resources::FormatString(L"CiProblemBadDeviceInfoFormat", problem.Key);

        case CiFileProblemKind::BadResource:
            return problem.Index < 0
                ? resources::GetString(L"CiProblemPropertiesNotList")
                : resources::FormatString(L"CiProblemBadPropertyFormat", number);

        case CiFileProblemKind::DuplicateResource:
            return resources::FormatString(L"CiProblemDuplicatePropertyFormat", number);

        case CiFileProblemKind::TooMany:
            return resources::GetString(problem.Section == CiFileSection::Profiles
                ? L"CiProblemTooManyProfiles"
                : L"CiProblemTooManyProperties");

        case CiFileProblemKind::TooLarge:
            return problem.Index < 0
                ? resources::GetString(L"CiProblemFileTooLarge")
                : resources::FormatString(L"CiProblemPropertyTooLargeFormat", number);

        default:
            return resources::GetString(L"CiProblemNotJson");
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeMessagePart(MessagePart part) noexcept
    {
        constexpr wchar_t const* keys[]{
            L"MessagePartGroup", L"MessagePartChannel", L"MessagePartNote", L"MessagePartVelocity",
            L"MessagePartController", L"MessagePartControllerValue", L"MessagePartProgram",
            L"MessagePartBankMsb", L"MessagePartBankLsb", L"MessagePartPressure",
            L"MessagePartPitchBend", L"MessagePartBits" };

        static_assert(std::size(keys) == MessagePartCount);

        auto const index = static_cast<size_t>(part);

        return index < std::size(keys) ? resources::GetString(keys[index]) : winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring DescribePartPlace(PartPlace const& place) noexcept
    {
        if (place.Part != MessagePart::Bits)
        {
            return DescribeMessagePart(place.Part);
        }

        return resources::FormatString(L"MessagePartBitsFormat",
            static_cast<int>(place.HighBit), static_cast<int>(place.LowBit), static_cast<int>(place.Word) + 1);
    }

    _Use_decl_annotations_
    winrt::hstring DescribeLogicTest(LogicTest test) noexcept
    {
        constexpr wchar_t const* keys[]{
            L"LogicTestAnything", L"LogicTestIs", L"LogicTestIsNot", L"LogicTestAtLeast", L"LogicTestBelow",
            L"LogicTestBetween", L"LogicTestOneOf", L"LogicTestHasValue", L"LogicTestIsEmpty" };

        auto const index = static_cast<size_t>(test);

        return index < std::size(keys) ? resources::GetString(keys[index]) : winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeLogicUnit(LogicUnit unit) noexcept
    {
        constexpr wchar_t const* keys[]{
            L"LogicUnitNumber", L"LogicUnitChannel", L"LogicUnitGroup", L"LogicUnitNote", L"LogicUnitValue" };

        auto const index = static_cast<size_t>(unit);

        return index < std::size(keys) ? resources::GetString(keys[index]) : winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeMemoryAction(MemoryAction action) noexcept
    {
        constexpr wchar_t const* keys[]{
            L"MemoryActionSet", L"MemoryActionToggle", L"MemoryActionStepUp", L"MemoryActionStepDown", L"MemoryActionClear" };

        auto const index = static_cast<size_t>(action);

        return index < std::size(keys) ? resources::GetString(keys[index]) : winrt::hstring{};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeUnitNumber(uint32_t number, LogicUnit unit, ValueScale scale, bool standalone) noexcept
    {
        try
        {
            switch (unit)
            {
            case LogicUnit::Channel:
            case LogicUnit::Group:
            {
                // Counted from 1 on screen, like every channel and group in the app.
                auto const shown = static_cast<int64_t>(number) + 1;

                if (!standalone)
                {
                    return winrt::to_hstring(shown);
                }

                return resources::FormatString(unit == LogicUnit::Channel ? L"UnitChannelFormat" : L"UnitGroupFormat", shown);
            }

            case LogicUnit::Note:
                return DescribeNote(static_cast<uint8_t>((std::min)(number, 127u)));

            case LogicUnit::Value:
                return DescribeScaledValue(static_cast<int32_t>((std::min)(number, static_cast<uint32_t>(FullScaleHundredths))), scale);

            default:
                return winrt::to_hstring(number);
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeLogicSource(LogicSource const& source, ValueScale scale) noexcept
    {
        switch (source.Kind)
        {
        case LogicSourceKind::Number:
            return DescribeUnitNumber(source.Number, source.Unit, scale, true);

        case LogicSourceKind::Part:
            return DescribePartPlace(source.Place);

        case LogicSourceKind::Tag:
            return resources::FormatString(L"LogicTagFormat", source.Name);

        case LogicSourceKind::Memory:
            return resources::FormatString(L"LogicMemoryFormat", source.Name);

        default:
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring DescribeCondition(LogicCondition const& condition, LogicUnit unit, ValueScale scale, bool standalone) noexcept
    {
        try
        {
            auto const value = [&](uint32_t number) { return DescribeUnitNumber(number, unit, scale, standalone); };

            switch (condition.Test)
            {
            case LogicTest::Anything:
                return resources::GetString(standalone ? L"ConditionAnythingLabel" : L"ConditionAnything");

            case LogicTest::Is:
                return value(condition.Value);

            case LogicTest::IsNot:
                return resources::FormatString(standalone ? L"ConditionIsNotLabelFormat" : L"ConditionIsNotFormat", value(condition.Value));

            case LogicTest::AtLeast:
                return resources::FormatString(L"ConditionAtLeastFormat", value(condition.Value));

            case LogicTest::Below:
                return resources::FormatString(standalone ? L"ConditionBelowLabelFormat" : L"ConditionBelowFormat", value(condition.Value));

            case LogicTest::Between:
                return resources::FormatString(L"ConditionBetweenFormat",
                    value((std::min)(condition.Lowest, condition.Highest)), value((std::max)(condition.Lowest, condition.Highest)));

            case LogicTest::OneOf:
            {
                if (condition.Values.empty())
                {
                    return resources::GetString(standalone ? L"ConditionNothingLabel" : L"ConditionNothing");
                }

                std::vector<std::wstring> items{};

                for (size_t i = 0; i < condition.Values.size() && i < MaximumNamesInSummary; i++)
                {
                    items.push_back(std::wstring{ value(condition.Values[i]) });
                }

                auto text = JoinList(items, L"BlockDescListOrFormat");

                if (condition.Values.size() > MaximumNamesInSummary)
                {
                    text = Text(resources::FormatString(L"FilterSummaryMoreFormat",
                        text, static_cast<int>(condition.Values.size() - MaximumNamesInSummary)));
                }

                return winrt::hstring{ text };
            }

            case LogicTest::HasValue:
                return resources::GetString(standalone ? L"ConditionHasValueLabel" : L"ConditionHasValue");

            case LogicTest::IsEmpty:
                return resources::GetString(standalone ? L"ConditionIsEmptyLabel" : L"ConditionIsEmpty");

            default:
                return {};
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeWay(BlockKind kind, BlockSettings const& settings, int32_t way) noexcept
    {
        if (kind == BlockKind::Branch)
        {
            return resources::GetString(way == BranchNoWay ? L"WayNo" : L"WayYes");
        }

        if (way == SwitchOtherwiseWay)
        {
            return resources::GetString(L"WayOtherwise");
        }

        for (auto const& entry : settings.Switch.Cases)
        {
            if (entry.Id == way)
            {
                return DescribeCondition(entry.Condition, settings.Switch.Unit, settings.Switch.Scale, true);
            }
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring DescribeLogicValue(LogicValue const& value, LogicUnit unit, ValueScale scale) noexcept
    {
        if (!value.HasValue)
        {
            return resources::GetString(L"LogicValueEmpty");
        }

        return DescribeUnitNumber(ValueInUnit(value, unit), unit, scale, true);
    }
}
