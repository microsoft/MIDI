// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "CaptureImport.h"

#include "RecordingTake.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>

namespace midisequencer
{
    namespace
    {
        // The columns both tools write first. Anything after Word3 is for people, and is ignored.
        constexpr std::string_view CaptureColumns = "Index,TimestampTicks,OffsetMicroseconds,DeltaMicroseconds,Group,Channel,WordCount,Word0,Word1,Word2,Word3";

        constexpr size_t OffsetColumn = 2;
        constexpr size_t WordCountColumn = 6;
        constexpr size_t FirstWordColumn = 7;
        constexpr size_t ColumnsRead = 11;

        // A capture can come from anywhere. This is as many notes as a whole sequence holds.
        constexpr size_t MaximumMessages = 8000000;

        std::string_view Trimmed(_In_ std::string_view text) noexcept
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '"'))
            {
                text.remove_prefix(1);
            }

            while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '"'))
            {
                text.remove_suffix(1);
            }

            return text;
        }

        std::optional<uint32_t> Hex(_In_ std::string_view text) noexcept
        {
            text = Trimmed(text);

            if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            {
                text.remove_prefix(2);
            }

            uint32_t value{ 0 };
            auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);

            if (text.empty() || text.size() > 8 || error != std::errc{} || end != text.data() + text.size())
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<double> Decimal(_In_ std::string_view text) noexcept
        {
            text = Trimmed(text);

            double value{ 0 };
            auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

            if (text.empty() || error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value))
            {
                return std::nullopt;
            }

            return value;
        }
    }

    _Use_decl_annotations_
    ReadCapture ReadCaptureCsv(std::string_view text)
    {
        ReadCapture result{};

        try
        {
            if (text.starts_with("\xEF\xBB\xBF"))
            {
                text.remove_prefix(3);
            }

            auto header{ false };

            while (!text.empty())
            {
                auto const end = text.find('\n');
                auto line = text.substr(0, end);
                text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);

                if (!line.empty() && line.back() == '\r')
                {
                    line.remove_suffix(1);
                }

                if (line.empty() || line.front() == '#')
                {
                    continue;
                }

                if (!header)
                {
                    if (!line.starts_with(CaptureColumns))
                    {
                        result.Status = CaptureStatus::NotACapture;
                        result.Messages.clear();
                        return result;
                    }

                    header = true;
                    continue;
                }

                // The columns read never hold a comma, so a later quoted column can't confuse them.
                std::array<std::string_view, ColumnsRead> fields{};
                size_t count{ 0 };
                auto rest = line;

                while (count < fields.size())
                {
                    auto const comma = rest.find(',');
                    fields[count++] = rest.substr(0, comma);

                    if (comma == std::string_view::npos)
                    {
                        break;
                    }

                    rest.remove_prefix(comma + 1);
                }

                auto const wordCount = count > WordCountColumn ? Decimal(fields[WordCountColumn]) : std::nullopt;

                // MIDI Monitor writes its notes, such as where it was paused, with no words.
                if (wordCount.has_value() && *wordCount == 0)
                {
                    continue;
                }

                auto const offset = count > OffsetColumn ? Decimal(fields[OffsetColumn]) : std::nullopt;

                if (!wordCount.has_value() || *wordCount < 1 || *wordCount > 4 || std::floor(*wordCount) != *wordCount ||
                    count < FirstWordColumn + static_cast<size_t>(*wordCount) || !offset.has_value() || *offset < 0)
                {
                    ++result.SkippedLines;
                    continue;
                }

                CapturedMessage message{};
                message.Seconds = *offset / 1000000.0;
                message.WordCount = static_cast<uint8_t>(*wordCount);

                auto valid{ true };

                for (uint8_t i = 0; i < message.WordCount && valid; ++i)
                {
                    auto const word = Hex(fields[FirstWordColumn + i]);
                    valid = word.has_value();
                    message.Words[i] = word.value_or(0);
                }

                if (!valid || UmpWordCount(message.Words[0]) != message.WordCount)
                {
                    ++result.SkippedLines;
                    continue;
                }

                if (result.Messages.size() >= MaximumMessages)
                {
                    result.Status = CaptureStatus::TooLarge;
                    result.Messages.clear();
                    return result;
                }

                result.Messages.push_back(message);
            }

            if (!header)
            {
                result.Status = CaptureStatus::NotACapture;
            }
        }
        catch (std::bad_alloc const&)
        {
            result = ReadCapture{ CaptureStatus::TooLarge };
        }

        return result;
    }

    _Use_decl_annotations_
    ImportedCapture ImportCapture(
        std::vector<CapturedMessage> const& messages,
        std::vector<TempoPoint> const& tempo,
        std::vector<MeterChange> const& meter,
        CaptureText const& text)
    {
        ImportedCapture result{};

        if (messages.empty())
        {
            return result;
        }

        // In the order they arrived, from the first.
        std::vector<CapturedMessage const*> ordered{};
        ordered.reserve(messages.size());

        for (auto const& message : messages)
        {
            ordered.push_back(&message);
        }

        std::stable_sort(ordered.begin(), ordered.end(), [](CapturedMessage const* a, CapturedMessage const* b) { return a->Seconds < b->Seconds; });

        auto const first = ordered.front()->Seconds;
        TempoMap const map{ tempo };
        SequenceLimits const limits{};

        // Everything a track records, system exclusive included.
        RecordFilter anything{};
        anything.SystemExclusive = true;

        std::array<std::optional<RecordingTake>, 16> takes{};
        std::array<int64_t, 16> lastTick{};

        for (auto const* message : ordered)
        {
            if (!PassesRecordFilter(anything, message->Words.data(), message->WordCount))
            {
                ++result.SkippedMessages;
                continue;
            }

            auto const group = static_cast<uint8_t>((message->Words[0] >> 24) & 0x0F);
            auto const tick = std::clamp<int64_t>(map.TickAtSeconds(std::min(message->Seconds - first, 1.0e7)), 0, limits.MaximumTick);

            if (!takes[group].has_value())
            {
                auto filter = anything;
                filter.Group = static_cast<int8_t>(group);
                takes[group].emplace(0, filter);
            }

            takes[group]->Add(tick, message->Words.data(), message->WordCount);
            lastTick[group] = std::max(lastTick[group], tick);
        }

        for (uint8_t group = 0; group < 16; ++group)
        {
            auto& take = takes[group];

            if (!take.has_value() || take->IsEmpty())
            {
                continue;
            }

            auto const name = text.TrackName ? text.TrackName(group) : std::wstring{};
            auto clip = take->Finish(lastTick[group] + 1, meter, NewId(L"c"), name, text.OriginDetail);

            Track track{};
            track.Id = NewId(L"t");
            track.Name = name;

            // A capture has notes on many channels: each keeps its own.
            track.Destination.Channel = -1;
            track.Timeline.push_back(Placement{ clip.Id, take->PlacementTick(), 0 });

            result.Tracks.push_back(std::move(track));
            result.Clips.push_back(std::move(clip));
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<CapturedMessage> ReadUmpWordText(std::wstring_view text, size_t& skippedLines)
    {
        // More than anyone copies, and still finite.
        constexpr size_t MaximumPasted = 1000000;

        skippedLines = 0;
        std::vector<CapturedMessage> messages{};

        while (!text.empty() && messages.size() < MaximumPasted)
        {
            auto const end = text.find(L'\n');
            auto line = text.substr(0, end);
            text = end == std::wstring_view::npos ? std::wstring_view{} : text.substr(end + 1);

            std::vector<uint32_t> words{};
            auto valid{ true };

            while (!line.empty() && valid)
            {
                auto const start = line.find_first_not_of(L" \t\r,");

                if (start == std::wstring_view::npos)
                {
                    break;
                }

                line.remove_prefix(start);
                auto const length = std::min(line.find_first_of(L" \t\r,"), line.size());
                auto token = line.substr(0, length);
                line.remove_prefix(length);

                if (token.size() > 2 && token[0] == L'0' && (token[1] == L'x' || token[1] == L'X'))
                {
                    token.remove_prefix(2);
                }

                uint32_t value{ 0 };
                valid = !token.empty() && token.size() <= 8;

                for (auto const c : token)
                {
                    auto const digit = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;

                    if (digit < 0)
                    {
                        valid = false;
                        break;
                    }

                    value = (value << 4) | static_cast<uint32_t>(digit);
                }

                words.push_back(value);
            }

            if (words.empty() && valid)
            {
                continue;
            }

            // The words have to make whole messages, or none of the line is used.
            std::vector<CapturedMessage> found{};

            for (size_t index = 0; valid && index < words.size();)
            {
                auto const count = UmpWordCount(words[index]);

                if (index + count > words.size())
                {
                    valid = false;
                    break;
                }

                CapturedMessage message{};
                message.WordCount = count;
                std::copy_n(words.begin() + static_cast<std::ptrdiff_t>(index), count, message.Words.begin());
                found.push_back(message);
                index += count;
            }

            if (!valid)
            {
                ++skippedLines;
                continue;
            }

            for (auto const& message : found)
            {
                if (messages.size() < MaximumPasted)
                {
                    messages.push_back(message);
                }
            }
        }

        return messages;
    }

    _Use_decl_annotations_
    PastedClip ClipFromMessages(
        std::vector<CapturedMessage> const& messages,
        int64_t startTick,
        int64_t stepTicks,
        std::vector<MeterChange> const& meter,
        std::wstring name,
        std::wstring originDetail)
    {
        SequenceLimits const limits{};
        startTick = std::clamp<int64_t>(startTick, 0, limits.MaximumTick);
        stepTicks = std::max<int64_t>(0, stepTicks);

        // Everything a track records, on any group.
        RecordFilter anything{};
        anything.SystemExclusive = true;

        RecordingTake take{ startTick, anything };
        auto tick = startTick;

        for (auto const& message : messages)
        {
            take.Add(tick, message.Words.data(), message.WordCount);
            tick = std::min(tick + stepTicks, limits.MaximumTick);
        }

        PastedClip pasted{};
        pasted.Clip = take.Finish(std::max(tick, startTick + 1), meter, NewId(L"c"), std::move(name), std::move(originDetail));
        pasted.PlacementTick = take.PlacementTick();
        return pasted;
    }
}
