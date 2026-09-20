// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MessageFilter.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t KeyActive[] = L"active";
        constexpr wchar_t KeyMessageTypes[] = L"messageTypes";
        constexpr wchar_t KeyChannels[] = L"channels";
        constexpr wchar_t KeyChannelVoice[] = L"channelVoiceStatuses";
        constexpr wchar_t KeySystem[] = L"systemMessages";
        constexpr wchar_t KeyLimitNotes[] = L"limitNoteRange";
        constexpr wchar_t KeyLowestNote[] = L"lowestNote";
        constexpr wchar_t KeyHighestNote[] = L"highestNote";

        // A bit per entry, so a filter costs a few characters in the patch file rather than a
        // list of sixteen booleans.
        uint32_t PackFlags(_In_ bool const* values, _In_ size_t count) noexcept
        {
            uint32_t packed{ 0 };

            for (size_t i = 0; i < count && i < 32; i++)
            {
                if (values[i])
                {
                    packed |= (1u << i);
                }
            }

            return packed;
        }

        void UnpackFlags(_In_ uint32_t packed, _Out_writes_(count) bool* values, _In_ size_t count) noexcept
        {
            for (size_t i = 0; i < count; i++)
            {
                values[i] = i < 32 && (packed & (1u << i)) != 0;
            }
        }

        bool AllTrue(_In_ bool const* values, _In_ size_t count) noexcept
        {
            for (size_t i = 0; i < count; i++)
            {
                if (!values[i])
                {
                    return false;
                }
            }

            return true;
        }

        // "1, 2, 5 to 8" rather than a list of sixteen numbers.
        std::wstring DescribeRuns(
            _In_ bool const* values,
            _In_ size_t count,
            _In_ int offset,
            _In_ std::function<winrt::hstring(size_t)> const& name) noexcept
        {
            std::wstring text{};

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

                if (name != nullptr)
                {
                    text += start == index
                        ? std::wstring{ name(start) }
                        : std::wstring{ name(start) } + L" - " + std::wstring{ name(index) };
                }
                else
                {
                    text += start == index
                        ? std::to_wstring(static_cast<int>(start) + offset)
                        : std::to_wstring(static_cast<int>(start) + offset) + L" - " +
                          std::to_wstring(static_cast<int>(index) + offset);
                }

                index++;
            }

            return text;
        }

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;

        // MIDI 2.0 only, and all carry a note index in the same byte.
        constexpr uint8_t StatusRegisteredPerNote = 0x0;
        constexpr uint8_t StatusAssignablePerNote = 0x1;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;
        constexpr uint8_t StatusPerNoteManagement = 0xF;
    }

    MessageFilter::MessageFilter() noexcept
    {
        Reset();
    }

    void MessageFilter::Reset() noexcept
    {
        IsActive = false;

        MessageTypes.fill(true);
        Channels.fill(true);
        ChannelVoiceStatuses.fill(true);
        SystemMessages.fill(true);

        LimitNoteRange = false;
        LowestAllowedNote = LowestNote;
        HighestAllowedNote = HighestNote;
    }

    bool MessageFilter::PassesEverything() const noexcept
    {
        if (!IsActive)
        {
            return true;
        }

        return !LimitNoteRange &&
            AllTrue(MessageTypes.data(), MessageTypes.size()) &&
            AllTrue(Channels.data(), Channels.size()) &&
            AllTrue(ChannelVoiceStatuses.data(), ChannelVoiceStatuses.size()) &&
            AllTrue(SystemMessages.data(), SystemMessages.size());
    }

    _Use_decl_annotations_
    bool MessageFilter::Allows(uint32_t const* words, uint8_t wordCount) const noexcept
    {
        if (!IsActive)
        {
            return true;
        }

        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        auto const first = words[0];
        auto const messageType = static_cast<uint8_t>((first >> 28) & 0x0F);

        if (!MessageTypes[messageType])
        {
            return false;
        }

        if (messageType == static_cast<uint8_t>(UmpMessageType::System))
        {
            auto const status = static_cast<uint8_t>((first >> 16) & 0xFF);
            auto const index = IndexOfSystemMessage(status);

            // A system message this app does not break out rides on the message type alone.
            return index >= SystemMessageCount || SystemMessages[index];
        }

        auto const isMidi1Voice = messageType == static_cast<uint8_t>(UmpMessageType::Midi1ChannelVoice);
        auto const isMidi2Voice = messageType == static_cast<uint8_t>(UmpMessageType::Midi2ChannelVoice);

        if (!isMidi1Voice && !isMidi2Voice)
        {
            return true;
        }

        auto const status = static_cast<uint8_t>((first >> 20) & 0x0F);
        auto const channel = static_cast<uint8_t>((first >> 16) & 0x0F);

        if (!ChannelVoiceStatuses[status] || !Channels[channel])
        {
            return false;
        }

        if (LimitNoteRange && StatusCarriesNote(status, isMidi2Voice))
        {
            auto const note = static_cast<uint8_t>((first >> 8) & 0x7F);

            if (note < LowestAllowedNote || note > HighestAllowedNote)
            {
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    size_t IndexOfSystemMessage(uint8_t status) noexcept
    {
        for (size_t i = 0; i < SystemMessageCount; i++)
        {
            if (SystemMessageList[i] == status)
            {
                return i;
            }
        }

        return SystemMessageCount;
    }

    _Use_decl_annotations_
    bool StatusCarriesNote(uint8_t status, bool isMidi2) noexcept
    {
        if (status == StatusNoteOff || status == StatusNoteOn || status == StatusPolyPressure)
        {
            return true;
        }

        return isMidi2 &&
            (status == StatusRegisteredPerNote || status == StatusAssignablePerNote ||
             status == StatusPerNotePitchBend || status == StatusPerNoteManagement);
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

            if (!AllTrue(filter.MessageTypes.data(), filter.MessageTypes.size()))
            {
                std::wstring names{};

                for (size_t i = 0; i < MessageTypeCount; i++)
                {
                    if (filter.MessageTypes[i])
                    {
                        continue;
                    }

                    if (!names.empty())
                    {
                        names += L", ";
                    }

                    names += std::wstring{ DescribeMessageType(static_cast<uint8_t>(i)) };
                }

                parts.push_back(std::wstring{ resources::FormatString(L"FilterSummaryNoTypesFormat", names) });
            }

            if (!AllTrue(filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size()))
            {
                std::wstring names{};

                for (size_t i = 0; i < ChannelVoiceStatusCount; i++)
                {
                    if (filter.ChannelVoiceStatuses[i])
                    {
                        continue;
                    }

                    if (!names.empty())
                    {
                        names += L", ";
                    }

                    names += std::wstring{ DescribeChannelVoiceStatus(static_cast<uint8_t>(i)) };
                }

                parts.push_back(std::wstring{ resources::FormatString(L"FilterSummaryNoMessagesFormat", names) });
            }

            if (!AllTrue(filter.SystemMessages.data(), filter.SystemMessages.size()))
            {
                std::wstring names{};

                for (size_t i = 0; i < SystemMessageCount; i++)
                {
                    if (filter.SystemMessages[i])
                    {
                        continue;
                    }

                    if (!names.empty())
                    {
                        names += L", ";
                    }

                    names += std::wstring{ DescribeSystemMessage(SystemMessageList[i]) };
                }

                parts.push_back(std::wstring{ resources::FormatString(L"FilterSummaryNoMessagesFormat", names) });
            }

            if (!AllTrue(filter.Channels.data(), filter.Channels.size()))
            {
                auto const runs = DescribeRuns(filter.Channels.data(), ChannelCount, 1, nullptr);

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

            std::wstring text{};

            for (auto const& part : parts)
            {
                if (!text.empty())
                {
                    text += L". ";
                }

                text += part;
            }

            return winrt::hstring{ text };
        }
        catch (...)
        {
        }

        return resources::GetString(L"FilterSummaryEverything");
    }

    _Use_decl_annotations_
    std::wstring FilterSignature(MessageFilter const& filter) noexcept
    {
        if (filter.PassesEverything())
        {
            return L"*";
        }

        return std::to_wstring(PackFlags(filter.MessageTypes.data(), filter.MessageTypes.size())) + L'.' +
            std::to_wstring(PackFlags(filter.Channels.data(), filter.Channels.size())) + L'.' +
            std::to_wstring(PackFlags(filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size())) + L'.' +
            std::to_wstring(PackFlags(filter.SystemMessages.data(), filter.SystemMessages.size())) + L'.' +
            (filter.LimitNoteRange
                ? std::to_wstring(filter.LowestAllowedNote) + L'-' + std::to_wstring(filter.HighestAllowedNote)
                : std::wstring{ L"n" });
    }

    _Use_decl_annotations_
    json::JsonObject FilterToJson(MessageFilter const& filter) noexcept    {
        json::JsonObject object{};

        try
        {
            object.SetNamedValue(KeyActive, json::JsonValue::CreateBooleanValue(filter.IsActive));

            object.SetNamedValue(KeyMessageTypes, json::JsonValue::CreateNumberValue(
                PackFlags(filter.MessageTypes.data(), filter.MessageTypes.size())));
            object.SetNamedValue(KeyChannels, json::JsonValue::CreateNumberValue(
                PackFlags(filter.Channels.data(), filter.Channels.size())));
            object.SetNamedValue(KeyChannelVoice, json::JsonValue::CreateNumberValue(
                PackFlags(filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size())));
            object.SetNamedValue(KeySystem, json::JsonValue::CreateNumberValue(
                PackFlags(filter.SystemMessages.data(), filter.SystemMessages.size())));

            object.SetNamedValue(KeyLimitNotes, json::JsonValue::CreateBooleanValue(filter.LimitNoteRange));
            object.SetNamedValue(KeyLowestNote, json::JsonValue::CreateNumberValue(filter.LowestAllowedNote));
            object.SetNamedValue(KeyHighestNote, json::JsonValue::CreateNumberValue(filter.HighestAllowedNote));
        }
        catch (...)
        {
        }

        return object;
    }

    _Use_decl_annotations_
    MessageFilter FilterFromJson(json::JsonObject const& object) noexcept
    {
        MessageFilter filter{};

        if (object == nullptr)
        {
            return filter;
        }

        try
        {
            auto const readFlags = [&object](std::wstring_view key, bool* values, size_t count)
                {
                    if (!object.HasKey(key))
                    {
                        return;
                    }

                    auto const value = object.GetNamedValue(key);

                    if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                    {
                        return;
                    }

                    auto const number = value.GetNumber();

                    if (!std::isfinite(number) || number < 0 || number > 0xFFFFFFFF)
                    {
                        return;
                    }

                    UnpackFlags(static_cast<uint32_t>(number), values, count);
                };

            auto const readBool = [&object](std::wstring_view key, bool defaultValue)
                {
                    if (!object.HasKey(key))
                    {
                        return defaultValue;
                    }

                    auto const value = object.GetNamedValue(key);

                    return value != nullptr && value.ValueType() == json::JsonValueType::Boolean
                        ? value.GetBoolean() : defaultValue;
                };

            auto const readNote = [&object](std::wstring_view key, uint8_t defaultValue)
                {
                    if (!object.HasKey(key))
                    {
                        return defaultValue;
                    }

                    auto const value = object.GetNamedValue(key);

                    if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                    {
                        return defaultValue;
                    }

                    auto const number = value.GetNumber();

                    if (!std::isfinite(number) || number < LowestNote || number > HighestNote)
                    {
                        return defaultValue;
                    }

                    return static_cast<uint8_t>(number);
                };

            filter.IsActive = readBool(KeyActive, false);

            readFlags(KeyMessageTypes, filter.MessageTypes.data(), filter.MessageTypes.size());
            readFlags(KeyChannels, filter.Channels.data(), filter.Channels.size());
            readFlags(KeyChannelVoice, filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size());
            readFlags(KeySystem, filter.SystemMessages.data(), filter.SystemMessages.size());

            filter.LimitNoteRange = readBool(KeyLimitNotes, false);
            filter.LowestAllowedNote = readNote(KeyLowestNote, LowestNote);
            filter.HighestAllowedNote = readNote(KeyHighestNote, HighestNote);

            // A hand edited file could invert them, which would silently drop everything.
            if (filter.LowestAllowedNote > filter.HighestAllowedNote)
            {
                std::swap(filter.LowestAllowedNote, filter.HighestAllowedNote);
            }
        }
        catch (...)
        {
        }

        return filter;
    }
}
