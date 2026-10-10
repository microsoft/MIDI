// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceSerializer.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <format>
#include <initializer_list>

namespace mjson = winrt::Windows::Data::Json;

namespace midisequencer
{
    namespace
    {
        // ---- keys ----

        constexpr wchar_t KeyFileVersion[] = L"fileVersion";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyTicksPerQuarterNote[] = L"ticksPerQuarterNote";
        constexpr wchar_t KeyTempo[] = L"tempo";
        constexpr wchar_t KeyMeter[] = L"meter";
        constexpr wchar_t KeyTags[] = L"tags";
        constexpr wchar_t KeyScenes[] = L"scenes";
        constexpr wchar_t KeyTracks[] = L"tracks";
        constexpr wchar_t KeyClips[] = L"clips";

        constexpr wchar_t KeyTick[] = L"tick";
        constexpr wchar_t KeyBpm[] = L"bpm";
        constexpr wchar_t KeyRampToNext[] = L"rampToNext";
        constexpr wchar_t KeyNumerator[] = L"numerator";
        constexpr wchar_t KeyDenominator[] = L"denominator";
        constexpr wchar_t KeyText[] = L"text";
        constexpr wchar_t KeyColor[] = L"color";
        constexpr wchar_t KeyId[] = L"id";

        constexpr wchar_t KeyKind[] = L"kind";
        constexpr wchar_t KeyPinned[] = L"pinned";
        constexpr wchar_t KeyMute[] = L"mute";
        constexpr wchar_t KeySolo[] = L"solo";
        constexpr wchar_t KeyOpen[] = L"open";
        constexpr wchar_t KeySource[] = L"source";
        constexpr wchar_t KeyDestination[] = L"destination";
        constexpr wchar_t KeyStartup[] = L"startup";
        constexpr wchar_t KeyTimeline[] = L"timeline";
        constexpr wchar_t KeySlots[] = L"slots";
        constexpr wchar_t KeyEndpoint[] = L"endpoint";
        constexpr wchar_t KeyGroup[] = L"group";
        constexpr wchar_t KeyChannels[] = L"channels";
        constexpr wchar_t KeyChannel[] = L"channel";
        constexpr wchar_t KeySystemExclusive[] = L"systemExclusive";
        constexpr wchar_t KeyRecord[] = L"record";
        constexpr wchar_t KeyEcho[] = L"echo";
        constexpr wchar_t KeyProtocol[] = L"protocol";
        constexpr wchar_t KeyClip[] = L"clip";
        constexpr wchar_t KeyLength[] = L"length";

        constexpr wchar_t KeyLoop[] = L"loop";
        constexpr wchar_t KeySeed[] = L"seed";
        constexpr wchar_t KeyOrigin[] = L"origin";
        constexpr wchar_t KeyHow[] = L"how";
        constexpr wchar_t KeyDetail[] = L"detail";
        constexpr wchar_t KeySettings[] = L"settings";
        constexpr wchar_t KeyNotes[] = L"notes";
        constexpr wchar_t KeyEvents[] = L"events";

        constexpr wchar_t ValueAny[] = L"any";
        constexpr wchar_t ValueAsRecorded[] = L"asRecorded";
        constexpr wchar_t ValueTrack[] = L"track";
        constexpr wchar_t ValueFolder[] = L"folder";

        // What a track records, as written in the file.
        constexpr std::array<std::pair<uint8_t, wchar_t const*>, 5> RecordKindNames{ {
            { RecordNotes, L"notes" },
            { RecordControllers, L"controllers" },
            { RecordPitchBend, L"pitchBend" },
            { RecordPressure, L"pressure" },
            { RecordProgram, L"program" },
        } };

        // ---- reading ----

        struct ReadContext
        {
            SequenceLimits Limits{};
            double TickScale{ 1.0 };
            size_t Skipped{ 0 };
            size_t TotalNotes{ 0 };
            size_t TotalTracks{ 0 };
        };

        mjson::IJsonValue Find(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
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

        bool IsType(_In_ mjson::IJsonValue const& value, _In_ mjson::JsonValueType type) noexcept
        {
            try
            {
                return value != nullptr && value.ValueType() == type;
            }
            catch (...)
            {
                return false;
            }
        }

        std::optional<double> ReadNumber(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                if (IsType(value, mjson::JsonValueType::Number))
                {
                    auto const number = value.GetNumber();

                    if (std::isfinite(number))
                    {
                        return number;
                    }
                }
            }
            catch (...)
            {
            }

            return std::nullopt;
        }

        int64_t ReadInteger(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ int64_t fallback,
            _In_ int64_t minimum,
            _In_ int64_t maximum) noexcept
        {
            auto const number = ReadNumber(object, key);

            if (!number.has_value())
            {
                return fallback;
            }

            auto const clamped = std::clamp(*number, static_cast<double>(minimum), static_cast<double>(maximum));
            return static_cast<int64_t>(std::llround(clamped));
        }

        bool ReadBool(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key, _In_ bool fallback) noexcept
        {
            try
            {
                auto const value = Find(object, key);

                if (IsType(value, mjson::JsonValueType::Boolean))
                {
                    return value.GetBoolean();
                }
            }
            catch (...)
            {
            }

            return fallback;
        }

        std::optional<std::wstring> ReadRawString(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key)
        {
            auto const value = Find(object, key);

            if (IsType(value, mjson::JsonValueType::String))
            {
                return std::wstring{ value.GetString() };
            }

            return std::nullopt;
        }

        std::wstring ReadText(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key, _In_ size_t maximumLength)
        {
            auto const raw = ReadRawString(object, key);
            return raw.has_value() ? midiapp::SanitizeProvenanceText(*raw, maximumLength) : std::wstring{};
        }

        mjson::JsonArray ReadArray(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key)
        {
            auto const value = Find(object, key);
            return IsType(value, mjson::JsonValueType::Array) ? value.GetArray() : nullptr;
        }

        mjson::JsonObject ReadObject(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key)
        {
            auto const value = Find(object, key);
            return IsType(value, mjson::JsonValueType::Object) ? value.GetObject() : nullptr;
        }

        int64_t ScaleTick(_In_ ReadContext const& context, _In_ double ticks) noexcept
        {
            auto const scaled = ticks * context.TickScale;

            if (!std::isfinite(scaled) || scaled <= 0)
            {
                return 0;
            }

            return std::min<int64_t>(static_cast<int64_t>(std::llround(std::min(scaled, 9.0e15))), context.Limits.MaximumTick);
        }

        int64_t ReadTick(_In_ ReadContext const& context, _In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            auto const number = ReadNumber(object, key);
            return number.has_value() ? ScaleTick(context, *number) : 0;
        }

        std::optional<uint32_t> ParseColor(_In_ std::wstring_view text) noexcept
        {
            if (text.size() != 7 || text[0] != L'#')
            {
                return std::nullopt;
            }

            uint32_t value{ 0 };

            for (size_t i = 1; i < text.size(); ++i)
            {
                auto const c = text[i];
                uint32_t digit{ 0 };

                if (c >= L'0' && c <= L'9') digit = static_cast<uint32_t>(c - L'0');
                else if (c >= L'a' && c <= L'f') digit = static_cast<uint32_t>(c - L'a' + 10);
                else if (c >= L'A' && c <= L'F') digit = static_cast<uint32_t>(c - L'A' + 10);
                else return std::nullopt;

                value = (value << 4) | digit;
            }

            return value;
        }

        std::optional<uint32_t> ReadColor(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key)
        {
            auto const raw = ReadRawString(object, key);
            return raw.has_value() ? ParseColor(*raw) : std::nullopt;
        }

        // Keys this version doesn't know, kept so saving again doesn't lose them.
        mjson::JsonObject UnknownKeys(_In_ mjson::JsonObject const& object, _In_ std::initializer_list<std::wstring_view> known)
        {
            mjson::JsonObject unknown{ nullptr };

            try
            {
                for (auto const& pair : object)
                {
                    std::wstring_view const key{ pair.Key() };

                    if (std::find(known.begin(), known.end(), key) != known.end())
                    {
                        continue;
                    }

                    if (unknown == nullptr)
                    {
                        unknown = mjson::JsonObject{};
                    }

                    unknown.SetNamedValue(pair.Key(), pair.Value());
                }
            }
            catch (...)
            {
            }

            return unknown;
        }

        std::vector<Tag> ReadTags(_Inout_ ReadContext& context, _In_ mjson::JsonArray const& array, _Inout_ size_t& tagCount)
        {
            std::vector<Tag> tags{};

            if (array == nullptr)
            {
                return tags;
            }

            for (auto const& value : array)
            {
                if (!IsType(value, mjson::JsonValueType::Object) || tagCount >= context.Limits.MaximumTags)
                {
                    ++context.Skipped;
                    continue;
                }

                auto const object = value.GetObject();

                Tag tag{};
                tag.Tick = ReadTick(context, object, KeyTick);
                tag.Text = ReadText(object, KeyText, context.Limits.MaximumTextLength);
                tag.Color = ReadColor(object, KeyColor);

                if (tag.Text.empty())
                {
                    ++context.Skipped;
                    continue;
                }

                tags.push_back(std::move(tag));
                ++tagCount;
            }

            return tags;
        }

        EndpointRef ReadEndpoint(_In_ ReadContext const& context, _In_ mjson::JsonObject const& object)
        {
            EndpointRef endpoint{};

            if (object != nullptr)
            {
                endpoint.Name = ReadText(object, KeyName, context.Limits.MaximumTextLength);

                // An endpoint device id is a path, not something to show, but it still has a ceiling.
                auto const id = ReadRawString(object, KeyId);

                if (id.has_value() && id->size() <= 2048)
                {
                    endpoint.Id = *id;
                }
            }

            return endpoint;
        }

        std::vector<ClipEvent> ReadUmpTextArray(_Inout_ ReadContext& context, _In_ mjson::JsonArray const& array, _In_ size_t maximum)
        {
            std::vector<ClipEvent> events{};

            if (array == nullptr)
            {
                return events;
            }

            for (auto const& value : array)
            {
                ClipEvent event{};

                if (events.size() >= maximum || !IsType(value, mjson::JsonValueType::String) || !UmpFromText(value.GetString(), event))
                {
                    ++context.Skipped;
                    continue;
                }

                events.push_back(event);
            }

            return events;
        }

        bool ReadTrack(
            _Inout_ ReadContext& context,
            _In_ mjson::JsonObject const& object,
            _In_ size_t depth,
            _Out_ Track& track,
            _Inout_ size_t& tagCount)
        {
            track = Track{};

            if (context.TotalTracks >= context.Limits.MaximumTracks)
            {
                ++context.Skipped;
                return false;
            }

            ++context.TotalTracks;

            track.Id = ReadText(object, KeyId, 128);
            track.IsFolder = ReadRawString(object, KeyKind).value_or(ValueTrack) == ValueFolder;
            track.Name = ReadText(object, KeyName, context.Limits.MaximumTextLength);
            track.Color = ReadColor(object, KeyColor).value_or(0x9E9E9E);
            track.Pinned = ReadBool(object, KeyPinned, false);
            track.Muted = ReadBool(object, KeyMute, false);
            track.Soloed = ReadBool(object, KeySolo, false);
            track.Open = ReadBool(object, KeyOpen, true);
            track.Tags = ReadTags(context, ReadArray(object, KeyTags), tagCount);

            track.Unknown = UnknownKeys(object, {
                KeyId, KeyKind, KeyName, KeyColor, KeyPinned, KeyMute, KeySolo, KeyOpen, KeyTags,
                KeySource, KeyDestination, KeyStartup, KeyTimeline, KeySlots, KeyTracks });

            if (track.IsFolder)
            {
                auto const children = ReadArray(object, KeyTracks);

                if (children != nullptr)
                {
                    if (depth + 1 >= context.Limits.MaximumFolderDepth)
                    {
                        context.Skipped += children.Size();
                    }
                    else
                    {
                        for (auto const& value : children)
                        {
                            Track child{};

                            if (IsType(value, mjson::JsonValueType::Object) && ReadTrack(context, value.GetObject(), depth + 1, child, tagCount))
                            {
                                track.Children.push_back(std::move(child));
                            }
                        }
                    }
                }

                return true;
            }

            if (auto const source = ReadObject(object, KeySource); source != nullptr)
            {
                track.Source.Endpoint = ReadEndpoint(context, ReadObject(source, KeyEndpoint));
                track.Source.Group = static_cast<int8_t>(ReadInteger(source, KeyGroup, -1, 0, 15));
                track.Source.SystemExclusive = ReadBool(source, KeySystemExclusive, false);
                track.Source.Echo = ReadBool(source, KeyEcho, true);

                if (auto const kinds = ReadArray(source, KeyRecord); kinds != nullptr)
                {
                    uint8_t mask{ 0 };

                    for (auto const& value : kinds)
                    {
                        if (!IsType(value, mjson::JsonValueType::String))
                        {
                            continue;
                        }

                        auto const name = value.GetString();

                        for (auto const& [bit, text] : RecordKindNames)
                        {
                            if (name == text)
                            {
                                mask |= bit;
                            }
                        }
                    }

                    track.Source.Record = mask;
                }

                if (auto const channels = ReadArray(source, KeyChannels); channels != nullptr)
                {
                    uint16_t mask{ 0 };

                    for (auto const& value : channels)
                    {
                        if (IsType(value, mjson::JsonValueType::Number))
                        {
                            auto const channel = value.GetNumber();

                            if (channel >= 0 && channel <= 15)
                            {
                                mask |= static_cast<uint16_t>(1u << static_cast<uint32_t>(channel));
                            }
                        }
                    }

                    track.Source.Channels = mask == 0 ? AllChannels : mask;
                }
            }

            if (auto const destination = ReadObject(object, KeyDestination); destination != nullptr)
            {
                track.Destination.Endpoint = ReadEndpoint(context, ReadObject(destination, KeyEndpoint));
                track.Destination.Group = static_cast<uint8_t>(ReadInteger(destination, KeyGroup, 0, 0, 15));

                auto const channelText = ReadRawString(destination, KeyChannel);
                track.Destination.Channel = (channelText.has_value() && *channelText == ValueAsRecorded)
                    ? static_cast<int8_t>(-1)
                    : static_cast<int8_t>(ReadInteger(destination, KeyChannel, 0, 0, 15));

                auto const protocol = ReadRawString(destination, KeyProtocol).value_or(L"automatic");
                track.Destination.Protocol =
                    protocol == L"midi1" ? ProtocolChoice::Midi1 :
                    protocol == L"midi2" ? ProtocolChoice::Midi2 :
                    ProtocolChoice::Automatic;
            }

            track.Startup = ReadUmpTextArray(context, ReadArray(object, KeyStartup), 1024);

            if (auto const timeline = ReadArray(object, KeyTimeline); timeline != nullptr)
            {
                for (auto const& value : timeline)
                {
                    if (!IsType(value, mjson::JsonValueType::Object) || track.Timeline.size() >= context.Limits.MaximumPlacementsPerTrack)
                    {
                        ++context.Skipped;
                        continue;
                    }

                    auto const entry = value.GetObject();

                    Placement placement{};
                    placement.ClipId = ReadText(entry, KeyClip, 128);
                    placement.Tick = ReadTick(context, entry, KeyTick);
                    placement.Length = ReadTick(context, entry, KeyLength);
                    track.Timeline.push_back(std::move(placement));
                }
            }

            if (auto const slots = ReadArray(object, KeySlots); slots != nullptr)
            {
                for (auto const& value : slots)
                {
                    if (track.Slots.size() >= context.Limits.MaximumScenes)
                    {
                        ++context.Skipped;
                        break;
                    }

                    track.Slots.push_back(IsType(value, mjson::JsonValueType::String)
                        ? midiapp::SanitizeProvenanceText(value.GetString(), 128)
                        : std::wstring{});
                }
            }

            return true;
        }

        ClipKind ParseClipKind(_In_ std::wstring_view text) noexcept
        {
            if (text == L"pattern") return ClipKind::Pattern;
            if (text == L"generator") return ClipKind::Generator;
            return ClipKind::Notes;
        }

        ClipOrigin ParseOrigin(_In_ std::wstring_view text) noexcept
        {
            if (text == L"recorded") return ClipOrigin::Recorded;
            if (text == L"drawn") return ClipOrigin::Drawn;
            if (text == L"generated") return ClipOrigin::Generated;
            if (text == L"imported") return ClipOrigin::Imported;
            if (text == L"assistant") return ClipOrigin::Assistant;
            return ClipOrigin::Unknown;
        }

        uint32_t ClampedField(_In_ mjson::JsonArray const& array, _In_ uint32_t index, _In_ uint32_t fallback, _In_ uint32_t maximum)
        {
            if (index >= array.Size())
            {
                return fallback;
            }

            auto const value = array.GetAt(index);

            if (!IsType(value, mjson::JsonValueType::Number))
            {
                return fallback;
            }

            auto const number = value.GetNumber();

            if (!std::isfinite(number) || number < 0)
            {
                return fallback;
            }

            return static_cast<uint32_t>(std::min(std::round(number), static_cast<double>(maximum)));
        }

        void ReadNotes(_Inout_ ReadContext& context, _In_ mjson::JsonArray const& array, _Inout_ Clip& clip)
        {
            if (array == nullptr)
            {
                return;
            }

            auto const count = array.Size();
            clip.Notes.reserve(std::min<size_t>(count, context.Limits.MaximumNotesPerClip));

            for (uint32_t i = 0; i < count; ++i)
            {
                if (clip.Notes.size() >= context.Limits.MaximumNotesPerClip || context.TotalNotes >= context.Limits.MaximumNotesInSequence)
                {
                    context.Skipped += count - i;
                    break;
                }

                auto const value = array.GetAt(i);

                if (!IsType(value, mjson::JsonValueType::Array))
                {
                    ++context.Skipped;
                    continue;
                }

                auto const fields = value.GetArray();

                if (fields.Size() < 5)
                {
                    ++context.Skipped;
                    continue;
                }

                auto const tick = fields.GetAt(0);
                auto const length = fields.GetAt(1);

                if (!IsType(tick, mjson::JsonValueType::Number) || !IsType(length, mjson::JsonValueType::Number))
                {
                    ++context.Skipped;
                    continue;
                }

                Note note{};
                note.Tick = ScaleTick(context, tick.GetNumber());
                note.Length = std::max<int64_t>(1, ScaleTick(context, length.GetNumber()));
                note.Channel = static_cast<uint8_t>(ClampedField(fields, 2, 0, 15));
                note.Number = static_cast<uint8_t>(ClampedField(fields, 3, 60, 127));
                note.Velocity = static_cast<uint16_t>(ClampedField(fields, 4, 0x8000, 0xFFFF));
                note.ReleaseVelocity = static_cast<uint16_t>(ClampedField(fields, 5, 0, 0xFFFF));
                note.AttributeType = static_cast<uint8_t>(ClampedField(fields, 6, 0, 0xFF));
                note.AttributeData = static_cast<uint16_t>(ClampedField(fields, 7, 0, 0xFFFF));
                note.Chance = static_cast<uint8_t>(ClampedField(fields, 8, 100, 100));

                clip.Notes.push_back(note);
                ++context.TotalNotes;
            }
        }

        void ReadEvents(_Inout_ ReadContext& context, _In_ mjson::JsonArray const& array, _Inout_ Clip& clip)
        {
            if (array == nullptr)
            {
                return;
            }

            auto const count = array.Size();

            for (uint32_t i = 0; i < count; ++i)
            {
                if (clip.Events.size() >= context.Limits.MaximumEventsPerClip)
                {
                    context.Skipped += count - i;
                    break;
                }

                auto const value = array.GetAt(i);

                if (!IsType(value, mjson::JsonValueType::Array))
                {
                    ++context.Skipped;
                    continue;
                }

                auto const fields = value.GetArray();

                if (fields.Size() < 2 || !IsType(fields.GetAt(0), mjson::JsonValueType::Number) || !IsType(fields.GetAt(1), mjson::JsonValueType::String))
                {
                    ++context.Skipped;
                    continue;
                }

                ClipEvent event{};

                if (!UmpFromText(fields.GetAt(1).GetString(), event))
                {
                    ++context.Skipped;
                    continue;
                }

                event.Tick = ScaleTick(context, fields.GetAt(0).GetNumber());
                clip.Events.push_back(event);
            }
        }

        bool ReadClip(_Inout_ ReadContext& context, _In_ mjson::JsonObject const& object, _Out_ Clip& clip)
        {
            clip = Clip{};

            clip.Id = ReadText(object, KeyId, 128);
            clip.Kind = ParseClipKind(ReadRawString(object, KeyKind).value_or(L"notes"));
            clip.Name = ReadText(object, KeyName, context.Limits.MaximumTextLength);
            clip.Color = ReadColor(object, KeyColor);
            clip.Length = std::max<int64_t>(1, ReadTick(context, object, KeyLength));
            clip.Loop = ReadBool(object, KeyLoop, true);
            clip.Seed = static_cast<uint32_t>(ReadInteger(object, KeySeed, 0, 0, 0xFFFFFFFF));

            if (auto const origin = ReadObject(object, KeyOrigin); origin != nullptr)
            {
                clip.Origin = ParseOrigin(ReadRawString(origin, KeyHow).value_or(L""));
                clip.OriginDetail = ReadText(origin, KeyDetail, context.Limits.MaximumTextLength);
            }

            clip.Settings = ReadObject(object, KeySettings);

            ReadNotes(context, ReadArray(object, KeyNotes), clip);
            ReadEvents(context, ReadArray(object, KeyEvents), clip);

            clip.Unknown = UnknownKeys(object, {
                KeyId, KeyKind, KeyName, KeyColor, KeyLength, KeyLoop, KeySeed, KeyOrigin, KeySettings, KeyNotes, KeyEvents });

            return true;
        }

        // ---- writing ----

        void AppendIndent(_Inout_ std::wstring& out, _In_ int32_t depth)
        {
            out.append(static_cast<size_t>(depth) * 2, L' ');
        }

        void AppendQuoted(_Inout_ std::wstring& out, _In_ std::wstring_view text)
        {
            out.push_back(L'"');

            for (auto const c : text)
            {
                switch (c)
                {
                case L'"': out += L"\\\""; break;
                case L'\\': out += L"\\\\"; break;
                case L'\n': out += L"\\n"; break;
                case L'\r': out += L"\\r"; break;
                case L'\t': out += L"\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        out += std::format(L"\\u{:04x}", static_cast<uint32_t>(c));
                    }
                    else
                    {
                        out.push_back(c);
                    }
                    break;
                }
            }

            out.push_back(L'"');
        }

        void AppendInteger(_Inout_ std::wstring& out, _In_ int64_t value)
        {
            char buffer[24]{};
            auto const result = std::to_chars(buffer, buffer + sizeof(buffer), value);

            for (auto p = buffer; p < result.ptr; ++p)
            {
                out.push_back(static_cast<wchar_t>(*p));
            }
        }

        std::wstring ColorText(_In_ uint32_t color)
        {
            return std::format(L"#{:06X}", color & 0xFFFFFF);
        }

        // Writes "key": value pairs, comma separated, either one per line or all on one line.
        class Members
        {
        public:
            Members(_Inout_ std::wstring& out, _In_ int32_t depth, _In_ bool inline_) :
                m_out(out), m_depth(depth), m_inline(inline_)
            {
            }

            void Key(_In_ std::wstring_view key)
            {
                if (m_inline)
                {
                    m_out += m_any ? L", " : L" ";
                }
                else
                {
                    m_out += m_any ? L",\n" : L"\n";
                    AppendIndent(m_out, m_depth + 1);
                }

                AppendQuoted(m_out, key);
                m_out += L": ";
                m_any = true;
            }

            void Text(_In_ std::wstring_view key, _In_ std::wstring_view value)
            {
                Key(key);
                AppendQuoted(m_out, value);
            }

            void Integer(_In_ std::wstring_view key, _In_ int64_t value)
            {
                Key(key);
                AppendInteger(m_out, value);
            }

            void Bool(_In_ std::wstring_view key, _In_ bool value)
            {
                Key(key);
                m_out += value ? L"true" : L"false";
            }

            void Raw(_In_ std::wstring_view key, _In_ std::wstring_view json)
            {
                Key(key);
                m_out += json;
            }

            void Unknown(_In_ mjson::JsonObject const& unknown)
            {
                if (unknown == nullptr)
                {
                    return;
                }

                std::vector<std::pair<std::wstring, std::wstring>> pairs{};

                for (auto const& pair : unknown)
                {
                    pairs.emplace_back(std::wstring{ pair.Key() }, std::wstring{ pair.Value().Stringify() });
                }

                std::sort(pairs.begin(), pairs.end());

                for (auto const& [key, json] : pairs)
                {
                    Raw(key, json);
                }
            }

            int32_t Depth() const noexcept { return m_depth; }

            void Close()
            {
                if (m_inline)
                {
                    m_out += m_any ? L" }" : L"}";
                }
                else
                {
                    m_out += L"\n";
                    AppendIndent(m_out, m_depth);
                    m_out += L"}";
                }
            }

        private:
            std::wstring& m_out;
            int32_t m_depth{ 0 };
            bool m_inline{ false };
            bool m_any{ false };
        };

        void WriteTagArray(_Inout_ std::wstring& out, _In_ std::vector<Tag> const& tags, _In_ int32_t depth)
        {
            out += L"[";

            for (size_t i = 0; i < tags.size(); ++i)
            {
                out += i == 0 ? L"\n" : L",\n";
                AppendIndent(out, depth + 1);
                out += L"{";

                Members tag{ out, depth + 1, true };
                tag.Integer(KeyTick, tags[i].Tick);
                tag.Text(KeyText, tags[i].Text);

                if (tags[i].Color.has_value())
                {
                    tag.Text(KeyColor, ColorText(*tags[i].Color));
                }

                tag.Close();
            }

            out += L"\n";
            AppendIndent(out, depth);
            out += L"]";
        }

        std::wstring EndpointText(_In_ EndpointRef const& endpoint)
        {
            std::wstring text{ L"{" };
            Members members{ text, 0, true };

            if (!endpoint.Name.empty())
            {
                members.Text(KeyName, endpoint.Name);
            }

            if (!endpoint.Id.empty())
            {
                members.Text(KeyId, endpoint.Id);
            }

            members.Close();
            return text;
        }

        std::wstring UmpTextArray(_In_ std::vector<ClipEvent> const& events)
        {
            std::wstring text{ L"[" };

            for (size_t i = 0; i < events.size(); ++i)
            {
                text += i == 0 ? L" " : L", ";
                AppendQuoted(text, UmpToText(events[i]));
            }

            text += events.empty() ? L"]" : L" ]";
            return text;
        }

        void WriteTrack(_Inout_ std::wstring& out, _In_ Track const& track, _In_ int32_t depth)
        {
            AppendIndent(out, depth);
            out += L"{";

            Members members{ out, depth, false };
            members.Text(KeyId, track.Id);
            members.Text(KeyKind, track.IsFolder ? ValueFolder : ValueTrack);
            members.Text(KeyName, track.Name);
            members.Text(KeyColor, ColorText(track.Color));

            if (track.Pinned) members.Bool(KeyPinned, true);
            if (track.Muted) members.Bool(KeyMute, true);
            if (track.Soloed) members.Bool(KeySolo, true);

            if (track.IsFolder)
            {
                if (!track.Open)
                {
                    members.Bool(KeyOpen, false);
                }
            }
            else
            {
                std::wstring source{ L"{" };
                Members sourceMembers{ source, 0, true };
                sourceMembers.Raw(KeyEndpoint, EndpointText(track.Source.Endpoint));

                if (track.Source.Group < 0)
                {
                    sourceMembers.Text(KeyGroup, ValueAny);
                }
                else
                {
                    sourceMembers.Integer(KeyGroup, track.Source.Group);
                }

                if (track.Source.Channels == AllChannels)
                {
                    sourceMembers.Text(KeyChannels, ValueAny);
                }
                else
                {
                    std::wstring channels{ L"[" };

                    for (uint32_t channel = 0, written = 0; channel < 16; ++channel)
                    {
                        if ((track.Source.Channels & (1u << channel)) != 0)
                        {
                            channels += written++ == 0 ? L" " : L", ";
                            AppendInteger(channels, channel);
                        }
                    }

                    channels += L" ]";
                    sourceMembers.Raw(KeyChannels, channels);
                }

                if (track.Source.SystemExclusive)
                {
                    sourceMembers.Bool(KeySystemExclusive, true);
                }

                if (track.Source.Record != RecordEverything)
                {
                    std::wstring kinds{ L"[" };
                    size_t written{ 0 };

                    for (auto const& [bit, text] : RecordKindNames)
                    {
                        if ((track.Source.Record & bit) != 0)
                        {
                            kinds += written++ == 0 ? L" \"" : L", \"";
                            kinds += text;
                            kinds += L"\"";
                        }
                    }

                    kinds += written == 0 ? L"]" : L" ]";
                    sourceMembers.Raw(KeyRecord, kinds);
                }

                if (!track.Source.Echo)
                {
                    sourceMembers.Bool(KeyEcho, false);
                }

                sourceMembers.Close();
                members.Raw(KeySource, source);

                std::wstring destination{ L"{" };
                Members destinationMembers{ destination, 0, true };
                destinationMembers.Raw(KeyEndpoint, EndpointText(track.Destination.Endpoint));
                destinationMembers.Integer(KeyGroup, track.Destination.Group);

                if (track.Destination.Channel < 0)
                {
                    destinationMembers.Text(KeyChannel, ValueAsRecorded);
                }
                else
                {
                    destinationMembers.Integer(KeyChannel, track.Destination.Channel);
                }

                destinationMembers.Text(KeyProtocol,
                    track.Destination.Protocol == ProtocolChoice::Midi1 ? L"midi1" :
                    track.Destination.Protocol == ProtocolChoice::Midi2 ? L"midi2" : L"automatic");
                destinationMembers.Close();
                members.Raw(KeyDestination, destination);

                if (!track.Startup.empty())
                {
                    members.Raw(KeyStartup, UmpTextArray(track.Startup));
                }

                if (!track.Timeline.empty())
                {
                    members.Key(KeyTimeline);
                    out += L"[";

                    for (size_t i = 0; i < track.Timeline.size(); ++i)
                    {
                        auto const& placement = track.Timeline[i];

                        out += i == 0 ? L"\n" : L",\n";
                        AppendIndent(out, depth + 2);
                        out += L"{";

                        Members entry{ out, depth + 2, true };
                        entry.Text(KeyClip, placement.ClipId);
                        entry.Integer(KeyTick, placement.Tick);

                        if (placement.Length > 0)
                        {
                            entry.Integer(KeyLength, placement.Length);
                        }

                        entry.Close();
                    }

                    out += L"\n";
                    AppendIndent(out, depth + 1);
                    out += L"]";
                }

                if (!track.Slots.empty())
                {
                    std::wstring slots{ L"[" };

                    for (size_t i = 0; i < track.Slots.size(); ++i)
                    {
                        slots += i == 0 ? L" " : L", ";

                        if (track.Slots[i].empty())
                        {
                            slots += L"null";
                        }
                        else
                        {
                            AppendQuoted(slots, track.Slots[i]);
                        }
                    }

                    slots += L" ]";
                    members.Raw(KeySlots, slots);
                }
            }

            if (!track.Tags.empty())
            {
                members.Key(KeyTags);
                WriteTagArray(out, track.Tags, depth + 1);
            }

            if (track.IsFolder && !track.Children.empty())
            {
                members.Key(KeyTracks);
                out += L"[\n";

                for (size_t i = 0; i < track.Children.size(); ++i)
                {
                    WriteTrack(out, track.Children[i], depth + 2);
                    out += i + 1 < track.Children.size() ? L",\n" : L"\n";
                }

                AppendIndent(out, depth + 1);
                out += L"]";
            }

            members.Unknown(track.Unknown);
            members.Close();
        }

        std::wstring_view ClipKindText(_In_ ClipKind kind) noexcept
        {
            switch (kind)
            {
            case ClipKind::Pattern: return L"pattern";
            case ClipKind::Generator: return L"generator";
            default: return L"notes";
            }
        }

        std::wstring_view OriginText(_In_ ClipOrigin origin) noexcept
        {
            switch (origin)
            {
            case ClipOrigin::Recorded: return L"recorded";
            case ClipOrigin::Drawn: return L"drawn";
            case ClipOrigin::Generated: return L"generated";
            case ClipOrigin::Imported: return L"imported";
            case ClipOrigin::Assistant: return L"assistant";
            default: return L"";
            }
        }

        void WriteNote(_Inout_ std::wstring& out, _In_ Note const& note)
        {
            out += L"[";
            AppendInteger(out, note.Tick);
            out += L", ";
            AppendInteger(out, note.Length);
            out += L", ";
            AppendInteger(out, note.Channel);
            out += L", ";
            AppendInteger(out, note.Number);
            out += L", ";
            AppendInteger(out, note.Velocity);

            // Trailing fields only when a note has them, which keeps a big recording small.
            int32_t last{ 4 };

            if (note.ReleaseVelocity != 0) last = 5;
            if (note.AttributeType != 0 || note.AttributeData != 0) last = 7;
            if (note.Chance != 100) last = 8;

            if (last >= 5) { out += L", "; AppendInteger(out, note.ReleaseVelocity); }
            if (last >= 7) { out += L", "; AppendInteger(out, note.AttributeType); out += L", "; AppendInteger(out, note.AttributeData); }
            if (last >= 8) { out += L", "; AppendInteger(out, note.Chance); }

            out += L"]";
        }

        void WriteClip(_Inout_ std::wstring& out, _In_ Clip const& clip, _In_ int32_t depth)
        {
            AppendIndent(out, depth);
            out += L"{";

            Members members{ out, depth, false };
            members.Text(KeyId, clip.Id);
            members.Text(KeyKind, ClipKindText(clip.Kind));
            members.Text(KeyName, clip.Name);

            if (clip.Color.has_value())
            {
                members.Text(KeyColor, ColorText(*clip.Color));
            }

            members.Integer(KeyLength, clip.Length);
            members.Bool(KeyLoop, clip.Loop);

            if (clip.Seed != 0)
            {
                members.Integer(KeySeed, clip.Seed);
            }

            if (clip.Origin != ClipOrigin::Unknown || !clip.OriginDetail.empty())
            {
                std::wstring origin{ L"{" };
                Members originMembers{ origin, 0, true };

                if (clip.Origin != ClipOrigin::Unknown)
                {
                    originMembers.Text(KeyHow, OriginText(clip.Origin));
                }

                if (!clip.OriginDetail.empty())
                {
                    originMembers.Text(KeyDetail, clip.OriginDetail);
                }

                originMembers.Close();
                members.Raw(KeyOrigin, origin);
            }

            if (clip.Settings != nullptr)
            {
                members.Raw(KeySettings, clip.Settings.Stringify());
            }

            if (!clip.Notes.empty())
            {
                members.Key(KeyNotes);
                out += L"[";

                for (size_t i = 0; i < clip.Notes.size(); ++i)
                {
                    out += i == 0 ? L"\n" : L",\n";
                    AppendIndent(out, depth + 2);
                    WriteNote(out, clip.Notes[i]);
                }

                out += L"\n";
                AppendIndent(out, depth + 1);
                out += L"]";
            }

            if (!clip.Events.empty())
            {
                members.Key(KeyEvents);
                out += L"[";

                for (size_t i = 0; i < clip.Events.size(); ++i)
                {
                    out += i == 0 ? L"\n" : L",\n";
                    AppendIndent(out, depth + 2);
                    out += L"[";
                    AppendInteger(out, clip.Events[i].Tick);
                    out += L", ";
                    AppendQuoted(out, UmpToText(clip.Events[i]));
                    out += L"]";
                }

                out += L"\n";
                AppendIndent(out, depth + 1);
                out += L"]";
            }

            members.Unknown(clip.Unknown);
            members.Close();
        }
    }

    _Use_decl_annotations_
    std::wstring UmpToText(ClipEvent const& event)
    {
        std::wstring text{};
        auto const count = std::min<uint8_t>(event.WordCount, 4);

        for (uint8_t i = 0; i < count; ++i)
        {
            if (i > 0)
            {
                text += L" ";
            }

            text += std::format(L"{:08X}", event.Words[i]);
        }

        return text;
    }

    _Use_decl_annotations_
    bool UmpFromText(std::wstring_view text, ClipEvent& event) noexcept
    {
        event = ClipEvent{};

        size_t position{ 0 };
        uint8_t count{ 0 };

        while (position < text.size())
        {
            while (position < text.size() && text[position] == L' ')
            {
                ++position;
            }

            if (position >= text.size())
            {
                break;
            }

            if (count >= 4)
            {
                return false;
            }

            uint32_t word{ 0 };
            size_t digits{ 0 };

            while (position < text.size() && text[position] != L' ')
            {
                auto const c = text[position];
                uint32_t digit{ 0 };

                if (c >= L'0' && c <= L'9') digit = static_cast<uint32_t>(c - L'0');
                else if (c >= L'a' && c <= L'f') digit = static_cast<uint32_t>(c - L'a' + 10);
                else if (c >= L'A' && c <= L'F') digit = static_cast<uint32_t>(c - L'A' + 10);
                else return false;

                if (++digits > 8)
                {
                    return false;
                }

                word = (word << 4) | digit;
                ++position;
            }

            event.Words[count++] = word;
        }

        if (count == 0 || count != UmpWordCount(event.Words[0]))
        {
            event = ClipEvent{};
            return false;
        }

        event.WordCount = count;
        return true;
    }

    _Use_decl_annotations_
    SequenceReadResult ReadSequenceJson(std::wstring_view text, Sequence& sequence, SequenceLimits const& limits)
    {
        sequence = Sequence{};

        SequenceReadResult result{};

        if (text.size() > MaximumSequenceFileCharacters)
        {
            result.Status = SequenceReadStatus::TooLarge;
            return result;
        }

        mjson::JsonObject root{ nullptr };

        if (!mjson::JsonObject::TryParse(winrt::hstring{ text }, root) || root == nullptr)
        {
            result.Status = SequenceReadStatus::NotJson;
            return result;
        }

        if (!ReadNumber(root, KeyFileVersion).has_value() || ReadArray(root, KeyTracks) == nullptr)
        {
            result.Status = SequenceReadStatus::NotASequence;
            return result;
        }

        ReadContext context{};
        context.Limits = limits;

        auto const fileVersion = ReadInteger(root, KeyFileVersion, CurrentFileVersion, 1, 0xFFFF);
        result.FromNewerVersion = fileVersion > static_cast<int64_t>(CurrentFileVersion);
        sequence.FileVersion = static_cast<uint32_t>(fileVersion);

        // Ticks are kept at 960 per quarter note. A file written at another resolution, by hand
        // or by another tool, is scaled on the way in.
        auto const fileTicks = ReadInteger(root, KeyTicksPerQuarterNote, TicksPerQuarterNote, 1, 65535);
        context.TickScale = static_cast<double>(TicksPerQuarterNote) / static_cast<double>(fileTicks);

        sequence.Name = ReadText(root, KeyName, limits.MaximumTextLength);
        sequence.Provenance = midiapp::ReadProvenance(root);

        sequence.Tempo.clear();

        if (auto const tempo = ReadArray(root, KeyTempo); tempo != nullptr)
        {
            for (auto const& value : tempo)
            {
                if (!IsType(value, mjson::JsonValueType::Object) || sequence.Tempo.size() >= limits.MaximumTempoPoints)
                {
                    ++context.Skipped;
                    continue;
                }

                auto const object = value.GetObject();

                TempoPoint point{};
                point.Tick = ReadTick(context, object, KeyTick);
                point.BeatsPerMinute = ReadNumber(object, KeyBpm).value_or(120.0);
                point.RampToNext = ReadBool(object, KeyRampToNext, false);
                sequence.Tempo.push_back(point);
            }
        }

        sequence.Meter.clear();

        if (auto const meter = ReadArray(root, KeyMeter); meter != nullptr)
        {
            for (auto const& value : meter)
            {
                if (!IsType(value, mjson::JsonValueType::Object) || sequence.Meter.size() >= limits.MaximumTempoPoints)
                {
                    ++context.Skipped;
                    continue;
                }

                auto const object = value.GetObject();

                MeterChange change{};
                change.Tick = ReadTick(context, object, KeyTick);
                change.Numerator = static_cast<uint8_t>(ReadInteger(object, KeyNumerator, 4, 1, 64));
                change.Denominator = static_cast<uint8_t>(ReadInteger(object, KeyDenominator, 4, 1, 32));
                sequence.Meter.push_back(change);
            }
        }

        size_t tagCount{ 0 };
        sequence.Tags = ReadTags(context, ReadArray(root, KeyTags), tagCount);

        if (auto const scenes = ReadArray(root, KeyScenes); scenes != nullptr)
        {
            for (auto const& value : scenes)
            {
                if (!IsType(value, mjson::JsonValueType::Object) || sequence.Scenes.size() >= limits.MaximumScenes)
                {
                    ++context.Skipped;
                    continue;
                }

                auto const object = value.GetObject();

                Scene scene{};
                scene.Id = ReadText(object, KeyId, 128);
                scene.Name = ReadText(object, KeyName, limits.MaximumTextLength);
                scene.Unknown = UnknownKeys(object, { KeyId, KeyName });
                sequence.Scenes.push_back(std::move(scene));
            }
        }

        if (auto const tracks = ReadArray(root, KeyTracks); tracks != nullptr)
        {
            for (auto const& value : tracks)
            {
                Track track{};

                if (IsType(value, mjson::JsonValueType::Object) && ReadTrack(context, value.GetObject(), 0, track, tagCount))
                {
                    sequence.Tracks.push_back(std::move(track));
                }
            }
        }

        if (auto const clips = ReadArray(root, KeyClips); clips != nullptr)
        {
            for (auto const& value : clips)
            {
                if (!IsType(value, mjson::JsonValueType::Object) || sequence.Clips.size() >= limits.MaximumClips)
                {
                    ++context.Skipped;
                    continue;
                }

                Clip clip{};

                if (ReadClip(context, value.GetObject(), clip))
                {
                    sequence.Clips.push_back(std::move(clip));
                }
            }
        }

        sequence.Unknown = UnknownKeys(root, {
            KeyFileVersion, KeyName, midiapp::ProvenanceKey, KeyTicksPerQuarterNote, KeyTempo, KeyMeter,
            KeyTags, KeyScenes, KeyTracks, KeyClips });

        NormalizeSequence(sequence);

        result.SkippedItems = context.Skipped;
        return result;
    }

    _Use_decl_annotations_
    std::wstring WriteSequenceJson(Sequence const& sequence)
    {
        size_t noteCount{ 0 };

        for (auto const& clip : sequence.Clips)
        {
            noteCount += clip.Notes.size() + clip.Events.size();
        }

        std::wstring out{};
        out.reserve(4096 + noteCount * 40);

        out += L"{";

        Members root{ out, 0, false };
        root.Integer(KeyFileVersion, sequence.FileVersion < CurrentFileVersion ? CurrentFileVersion : sequence.FileVersion);

        if (!sequence.Name.empty())
        {
            root.Text(KeyName, sequence.Name);
        }

        if (sequence.Provenance.has_value() && !sequence.Provenance->IsEmpty())
        {
            root.Raw(midiapp::ProvenanceKey, midiapp::ProvenanceToJsonText(*sequence.Provenance, 1));
        }

        root.Integer(KeyTicksPerQuarterNote, TicksPerQuarterNote);

        root.Key(KeyTempo);
        out += L"[";

        for (size_t i = 0; i < sequence.Tempo.size(); ++i)
        {
            auto const& point = sequence.Tempo[i];

            out += i == 0 ? L"\n" : L",\n";
            AppendIndent(out, 2);
            out += L"{";

            Members entry{ out, 2, true };
            entry.Integer(KeyTick, point.Tick);
            entry.Raw(KeyBpm, std::format(L"{}", point.BeatsPerMinute));

            if (point.RampToNext)
            {
                entry.Bool(KeyRampToNext, true);
            }

            entry.Close();
        }

        out += L"\n  ]";

        root.Key(KeyMeter);
        out += L"[";

        for (size_t i = 0; i < sequence.Meter.size(); ++i)
        {
            auto const& change = sequence.Meter[i];

            out += i == 0 ? L"\n" : L",\n";
            AppendIndent(out, 2);
            out += L"{";

            Members entry{ out, 2, true };
            entry.Integer(KeyTick, change.Tick);
            entry.Integer(KeyNumerator, change.Numerator);
            entry.Integer(KeyDenominator, change.Denominator);
            entry.Close();
        }

        out += L"\n  ]";

        if (!sequence.Tags.empty())
        {
            root.Key(KeyTags);
            WriteTagArray(out, sequence.Tags, 1);
        }

        if (!sequence.Scenes.empty())
        {
            root.Key(KeyScenes);
            out += L"[";

            for (size_t i = 0; i < sequence.Scenes.size(); ++i)
            {
                out += i == 0 ? L"\n" : L",\n";
                AppendIndent(out, 2);
                out += L"{";

                Members entry{ out, 2, true };
                entry.Text(KeyId, sequence.Scenes[i].Id);
                entry.Text(KeyName, sequence.Scenes[i].Name);
                entry.Unknown(sequence.Scenes[i].Unknown);
                entry.Close();
            }

            out += L"\n  ]";
        }

        root.Key(KeyTracks);
        out += L"[";

        for (size_t i = 0; i < sequence.Tracks.size(); ++i)
        {
            out += i == 0 ? L"\n" : L",\n";
            WriteTrack(out, sequence.Tracks[i], 2);
        }

        out += sequence.Tracks.empty() ? L"]" : L"\n  ]";

        root.Key(KeyClips);
        out += L"[";

        for (size_t i = 0; i < sequence.Clips.size(); ++i)
        {
            out += i == 0 ? L"\n" : L",\n";
            WriteClip(out, sequence.Clips[i], 2);
        }

        out += sequence.Clips.empty() ? L"]" : L"\n  ]";

        root.Unknown(sequence.Unknown);
        root.Close();
        out += L"\n";

        return out;
    }
}
