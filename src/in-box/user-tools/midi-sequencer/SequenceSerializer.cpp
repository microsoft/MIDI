// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceSerializer.h"
#include "Lyrics.h"
#include "SequenceGenerator.h"
#include "SequencePattern.h"

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
        constexpr wchar_t KeyCopyright[] = L"copyright";
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
        constexpr wchar_t KeyLanes[] = L"lanes";
        constexpr wchar_t KeyLanesOpen[] = L"lanesOpen";
        constexpr wchar_t KeyTarget[] = L"target";
        constexpr wchar_t KeyNumber[] = L"number";
        constexpr wchar_t KeyBank[] = L"bank";
        constexpr wchar_t KeyIndex[] = L"index";
        constexpr wchar_t KeyPoints[] = L"points";

        constexpr wchar_t KeyMode[] = L"mode";
        constexpr wchar_t KeyStepCount[] = L"stepCount";
        constexpr wchar_t KeyStepTicks[] = L"stepTicks";
        constexpr wchar_t KeySwing[] = L"swing";
        constexpr wchar_t KeyLanesKeepLength[] = L"lanesKeepLength";
        constexpr wchar_t KeyNote[] = L"note";
        constexpr wchar_t KeySteps[] = L"steps";
        constexpr wchar_t KeyChance[] = L"chance";
        constexpr wchar_t KeyRatchet[] = L"ratchet";
        constexpr wchar_t KeyShape[] = L"shape";
        constexpr wchar_t KeyCondition[] = L"condition";
        constexpr wchar_t KeyNudge[] = L"nudge";
        constexpr wchar_t KeyAccent[] = L"accent";
        constexpr wchar_t KeySlide[] = L"slide";

        constexpr wchar_t ValueDrums[] = L"drums";
        constexpr wchar_t ValueMelodic[] = L"melodic";
        constexpr wchar_t ValueSofter[] = L"softer";
        constexpr wchar_t ValueLouder[] = L"louder";
        constexpr wchar_t ValueFirstPass[] = L"firstPass";
        constexpr wchar_t ValueNotFirstPass[] = L"notFirstPass";

        constexpr wchar_t KeyType[] = L"type";
        constexpr wchar_t KeyEachPass[] = L"eachPass";
        constexpr wchar_t KeyNoteLength[] = L"noteLength";
        constexpr wchar_t KeyRings[] = L"rings";
        constexpr wchar_t KeyPulses[] = L"pulses";
        constexpr wchar_t KeyRotate[] = L"rotate";
        constexpr wchar_t KeyVelocity[] = L"velocity";
        constexpr wchar_t KeyScale[] = L"scale";
        constexpr wchar_t KeyRoot[] = L"root";
        constexpr wchar_t KeyLow[] = L"low";
        constexpr wchar_t KeyHigh[] = L"high";
        constexpr wchar_t KeyBusy[] = L"busy";
        constexpr wchar_t KeyKeep[] = L"keep";
        constexpr wchar_t KeyArpTrack[] = L"track";
        constexpr wchar_t KeyOrder[] = L"order";
        constexpr wchar_t KeyOctaves[] = L"octaves";
        constexpr wchar_t KeyLatch[] = L"latch";
        constexpr wchar_t ValueHeld[] = L"held";

        // Arpeggiator orders, as written in the file, in ArpeggiatorOrder order.
        constexpr std::array<wchar_t const*, 5> ArpOrderNames{ L"up", L"down", L"upAndDown", L"asHeld", L"random" };

        constexpr wchar_t ValueEuclidean[] = L"euclidean";
        constexpr wchar_t ValueChanceMelody[] = L"chanceMelody";
        constexpr wchar_t ValueArpeggiator[] = L"arpeggiator";
        constexpr wchar_t ValueSame[] = L"same";
        constexpr wchar_t ValueChanges[] = L"changes";

        // Scales, as written in the file, in ScaleKind order.
        constexpr std::array<wchar_t const*, 11> ScaleNames{
            L"major", L"minor", L"dorian", L"phrygian", L"lydian", L"mixolydian", L"harmonicMinor",
            L"majorPentatonic", L"minorPentatonic", L"blues", L"chromatic" };

        constexpr wchar_t ValueAny[] = L"any";
        constexpr wchar_t ValueAsRecorded[] = L"asRecorded";
        constexpr wchar_t ValueTrack[] = L"track";
        constexpr wchar_t ValueFolder[] = L"folder";
        constexpr wchar_t ValueHold[] = L"hold";

        // What an automation lane changes, as written in the file.
        constexpr std::array<std::pair<LaneTarget, wchar_t const*>, 6> LaneTargetNames{ {
            { LaneTarget::Controller, L"controller" },
            { LaneTarget::PitchBend, L"pitchBend" },
            { LaneTarget::ChannelPressure, L"channelPressure" },
            { LaneTarget::Program, L"program" },
            { LaneTarget::Registered, L"registered" },
            { LaneTarget::Assignable, L"assignable" },
        } };

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
            size_t TotalLanePoints{ 0 };
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

        std::vector<AutomationLane> ReadLanes(_Inout_ ReadContext& context, _In_ mjson::JsonArray const& array)
        {
            std::vector<AutomationLane> lanes{};

            if (array == nullptr)
            {
                return lanes;
            }

            for (auto const& value : array)
            {
                if (!IsType(value, mjson::JsonValueType::Object) || lanes.size() >= context.Limits.MaximumLanesPerOwner)
                {
                    ++context.Skipped;
                    continue;
                }

                auto const object = value.GetObject();
                auto const targetText = ReadRawString(object, KeyTarget).value_or(L"");
                auto const target = std::find_if(LaneTargetNames.begin(), LaneTargetNames.end(),
                    [&targetText](auto const& entry) { return targetText == entry.second; });

                // A target from a newer version can't be sent, so its lane is left out.
                if (target == LaneTargetNames.end())
                {
                    ++context.Skipped;
                    continue;
                }

                AutomationLane lane{};
                lane.Id = ReadText(object, KeyId, 128);
                lane.Name = ReadText(object, KeyName, context.Limits.MaximumTextLength);
                lane.Target = target->first;
                lane.Channel = static_cast<uint8_t>(ReadInteger(object, KeyChannel, 0, 0, 15));

                if (lane.Target == LaneTarget::Controller)
                {
                    lane.Number = static_cast<uint16_t>(ReadInteger(object, KeyNumber, 0, 0, 127));
                }
                else if (lane.Target == LaneTarget::Registered || lane.Target == LaneTarget::Assignable)
                {
                    auto const bank = ReadInteger(object, KeyBank, 0, 0, 127);
                    auto const index = ReadInteger(object, KeyIndex, 0, 0, 127);
                    lane.Number = static_cast<uint16_t>(bank * 128 + index);
                }

                if (auto const points = ReadArray(object, KeyPoints); points != nullptr)
                {
                    auto const count = points.Size();

                    for (uint32_t i = 0; i < count; ++i)
                    {
                        if (lane.Points.size() >= context.Limits.MaximumPointsPerLane || context.TotalLanePoints >= context.Limits.MaximumLanePointsInSequence)
                        {
                            context.Skipped += count - i;
                            break;
                        }

                        auto const entry = points.GetAt(i);

                        if (!IsType(entry, mjson::JsonValueType::Array))
                        {
                            ++context.Skipped;
                            continue;
                        }

                        auto const fields = entry.GetArray();

                        if (fields.Size() < 2 || !IsType(fields.GetAt(0), mjson::JsonValueType::Number) || !IsType(fields.GetAt(1), mjson::JsonValueType::Number))
                        {
                            ++context.Skipped;
                            continue;
                        }

                        auto const raw = fields.GetAt(1).GetNumber();

                        LanePoint point{};
                        point.Tick = ScaleTick(context, fields.GetAt(0).GetNumber());
                        point.Value = std::isfinite(raw) ? static_cast<uint32_t>(std::clamp(std::round(raw), 0.0, 4294967295.0)) : 0;
                        point.Hold = fields.Size() >= 3 && IsType(fields.GetAt(2), mjson::JsonValueType::String) && fields.GetAt(2).GetString() == ValueHold;

                        lane.Points.push_back(point);
                        ++context.TotalLanePoints;
                    }
                }

                lane.Unknown = UnknownKeys(object, { KeyId, KeyName, KeyTarget, KeyNumber, KeyBank, KeyIndex, KeyChannel, KeyPoints });
                lanes.push_back(std::move(lane));
            }

            return lanes;
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
                KeySource, KeyDestination, KeyStartup, KeyTimeline, KeySlots, KeyTracks, KeyLanes, KeyLanesOpen });

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

            track.Lanes = ReadLanes(context, ReadArray(object, KeyLanes));
            track.LanesOpen = ReadBool(object, KeyLanesOpen, true);

            return true;
        }

        ClipKind ParseClipKind(_In_ std::wstring_view text) noexcept
        {
            if (text == L"pattern") return ClipKind::Pattern;
            if (text == L"generator") return ClipKind::Generator;
            return ClipKind::Notes;
        }

        // "firstPass", "notFirstPass", or "1:2" for the first pass of every two.
        void ParseCondition(_In_ std::wstring_view text, _Inout_ PatternStep& step) noexcept
        {
            step.Condition = StepCondition::Always;

            if (text == ValueFirstPass)
            {
                step.Condition = StepCondition::FirstPass;
                return;
            }

            if (text == ValueNotFirstPass)
            {
                step.Condition = StepCondition::NotFirstPass;
                return;
            }

            auto const colon = text.find(L':');

            if (colon == std::wstring_view::npos || colon == 0 || colon + 1 >= text.size() || text.size() > 8)
            {
                return;
            }

            uint32_t of{ 0 };
            uint32_t every{ 0 };

            for (auto const c : text.substr(0, colon))
            {
                if (c < L'0' || c > L'9') return;
                of = of * 10 + static_cast<uint32_t>(c - L'0');
            }

            for (auto const c : text.substr(colon + 1))
            {
                if (c < L'0' || c > L'9') return;
                every = every * 10 + static_cast<uint32_t>(c - L'0');
            }

            if (every < 2 || every > 16 || of < 1 || of > every)
            {
                return;
            }

            step.Condition = StepCondition::Every;
            step.Every = static_cast<uint8_t>(every);
            step.Of = static_cast<uint8_t>(of);
        }

        // [index, velocity] or [index, velocity, { options }].
        bool ReadPatternStep(_In_ ReadContext const& context, _In_ mjson::IJsonValue const& value, _In_ uint8_t laneNote, _Out_ PatternStep& step)
        {
            step = PatternStep{};
            step.Note = laneNote;

            if (!IsType(value, mjson::JsonValueType::Array))
            {
                return false;
            }

            auto const fields = value.GetArray();

            if (fields.Size() < 2 || !IsType(fields.GetAt(0), mjson::JsonValueType::Number) || !IsType(fields.GetAt(1), mjson::JsonValueType::Number))
            {
                return false;
            }

            auto const index = fields.GetAt(0).GetNumber();

            if (!std::isfinite(index) || index < 0 || index >= MaximumPatternSteps)
            {
                return false;
            }

            step.Index = static_cast<uint16_t>(index);

            if (auto const velocity = fields.GetAt(1).GetNumber(); std::isfinite(velocity))
            {
                step.Velocity = static_cast<uint16_t>(std::clamp(std::round(velocity), 0.0, 65535.0));
            }

            if (fields.Size() >= 3 && IsType(fields.GetAt(2), mjson::JsonValueType::Object))
            {
                auto const options = fields.GetAt(2).GetObject();

                step.Chance = static_cast<uint8_t>(ReadInteger(options, KeyChance, 100, 0, 100));
                step.Ratchet = static_cast<uint8_t>(ReadInteger(options, KeyRatchet, 1, 1, MaximumRatchet));

                auto const shape = ReadRawString(options, KeyShape).value_or(L"");
                step.Shape = shape == ValueSofter ? RatchetShape::Softer : shape == ValueLouder ? RatchetShape::Louder : RatchetShape::Even;

                ParseCondition(ReadRawString(options, KeyCondition).value_or(L""), step);

                if (auto const nudge = ReadNumber(options, KeyNudge); nudge.has_value())
                {
                    step.Nudge = static_cast<int16_t>(std::clamp(std::round(*nudge * context.TickScale), -32768.0, 32767.0));
                }

                step.Length = static_cast<uint16_t>(ReadInteger(options, KeyLength, 50, 1, MaximumStepLength));
                step.Note = static_cast<uint8_t>(ReadInteger(options, KeyNote, laneNote, 0, 127));
                step.Accent = ReadBool(options, KeyAccent, false);
                step.Slide = ReadBool(options, KeySlide, false);
            }

            return true;
        }

        PatternSettings ReadPattern(_Inout_ ReadContext& context, _In_ mjson::JsonObject const& object)
        {
            PatternSettings pattern{};

            if (object == nullptr)
            {
                return pattern;
            }

            pattern.Mode = ReadRawString(object, KeyMode).value_or(ValueDrums) == ValueMelodic ? PatternMode::Melodic : PatternMode::Drums;
            pattern.StepCount = static_cast<uint16_t>(ReadInteger(object, KeyStepCount, 16, 1, MaximumPatternSteps));

            if (auto const ticks = ReadNumber(object, KeyStepTicks); ticks.has_value())
            {
                pattern.StepTicks = std::max<int64_t>(1, ScaleTick(context, *ticks));
            }

            pattern.Swing = static_cast<uint8_t>(ReadInteger(object, KeySwing, 50, MinimumSwing, MaximumSwing));
            pattern.LanesKeepLength = ReadBool(object, KeyLanesKeepLength, true);
            pattern.Channel = static_cast<uint8_t>(ReadInteger(object, KeyChannel, pattern.Mode == PatternMode::Drums ? 9 : 0, 0, 15));

            if (auto const lanes = ReadArray(object, KeyLanes); lanes != nullptr)
            {
                for (auto const& value : lanes)
                {
                    if (!IsType(value, mjson::JsonValueType::Object) || pattern.Lanes.size() >= MaximumPatternLanes)
                    {
                        ++context.Skipped;
                        continue;
                    }

                    auto const laneObject = value.GetObject();

                    PatternLane lane{};
                    lane.Id = ReadText(laneObject, KeyId, 128);
                    lane.Name = ReadText(laneObject, KeyName, context.Limits.MaximumTextLength);
                    lane.Note = static_cast<uint8_t>(ReadInteger(laneObject, KeyNote, 36, 0, 127));
                    lane.StepCount = static_cast<uint16_t>(ReadInteger(laneObject, KeyStepCount, pattern.StepCount, 1, MaximumPatternSteps));
                    lane.Muted = ReadBool(laneObject, KeyMute, false);
                    lane.Soloed = ReadBool(laneObject, KeySolo, false);

                    if (auto const steps = ReadArray(laneObject, KeySteps); steps != nullptr)
                    {
                        for (auto const& entry : steps)
                        {
                            PatternStep step{};

                            if (lane.Steps.size() >= MaximumPatternSteps || !ReadPatternStep(context, entry, lane.Note, step))
                            {
                                ++context.Skipped;
                                continue;
                            }

                            lane.Steps.push_back(step);
                        }
                    }

                    lane.Unknown = UnknownKeys(laneObject, { KeyId, KeyName, KeyNote, KeyStepCount, KeyMute, KeySolo, KeySteps });
                    pattern.Lanes.push_back(std::move(lane));
                }
            }

            pattern.Unknown = UnknownKeys(object, { KeyMode, KeyStepCount, KeyStepTicks, KeySwing, KeyLanesKeepLength, KeyChannel, KeyLanes });
            return pattern;
        }

        GeneratorSettings ReadGenerator(_Inout_ ReadContext& context, _In_ mjson::JsonObject const& object)
        {
            GeneratorSettings generator{};

            if (object == nullptr)
            {
                return generator;
            }

            auto const type = ReadRawString(object, KeyType).value_or(ValueEuclidean);
            generator.Type = type == ValueChanceMelody ? GeneratorType::ChanceMelody : type == ValueArpeggiator ? GeneratorType::Arpeggiator : GeneratorType::Euclidean;

            if (auto const ticks = ReadNumber(object, KeyStepTicks); ticks.has_value())
            {
                generator.StepTicks = std::max<int64_t>(1, ScaleTick(context, *ticks));
            }

            generator.ChangesEachPass = ReadRawString(object, KeyEachPass).value_or(ValueSame) == ValueChanges;
            generator.Channel = static_cast<uint8_t>(ReadInteger(object, KeyChannel, 0, 0, 15));
            generator.NoteLength = static_cast<uint16_t>(ReadInteger(object, KeyNoteLength, 50, 1, 1600));

            if (auto const rings = ReadArray(object, KeyRings); rings != nullptr)
            {
                for (auto const& value : rings)
                {
                    if (!IsType(value, mjson::JsonValueType::Object) || generator.Rings.size() >= MaximumEuclidRings)
                    {
                        ++context.Skipped;
                        continue;
                    }

                    auto const ringObject = value.GetObject();

                    EuclidRing ring{};
                    ring.Note = static_cast<uint8_t>(ReadInteger(ringObject, KeyNote, 36, 0, 127));
                    ring.Steps = static_cast<uint8_t>(ReadInteger(ringObject, KeySteps, 16, 1, MaximumRingSteps));
                    ring.Pulses = static_cast<uint8_t>(ReadInteger(ringObject, KeyPulses, 4, 0, ring.Steps));
                    ring.Rotate = static_cast<uint8_t>(ReadInteger(ringObject, KeyRotate, 0, 0, ring.Steps - 1));
                    ring.Velocity = static_cast<uint16_t>(ReadInteger(ringObject, KeyVelocity, ring.Velocity, 0, 0xFFFF));
                    ring.Chance = static_cast<uint8_t>(ReadInteger(ringObject, KeyChance, 100, 0, 100));
                    generator.Rings.push_back(ring);
                }
            }

            auto const scale = ReadRawString(object, KeyScale).value_or(L"");
            auto const named = std::find_if(ScaleNames.begin(), ScaleNames.end(), [&scale](wchar_t const* name) { return scale == name; });

            if (named != ScaleNames.end())
            {
                generator.Scale = static_cast<ScaleKind>(std::distance(ScaleNames.begin(), named));
            }

            generator.ScaleRoot = static_cast<uint8_t>(ReadInteger(object, KeyRoot, generator.ScaleRoot, 0, 11));
            generator.LowNote = static_cast<uint8_t>(ReadInteger(object, KeyLow, generator.LowNote, 0, 127));
            generator.HighNote = static_cast<uint8_t>(ReadInteger(object, KeyHigh, generator.HighNote, 0, 127));
            generator.Busy = static_cast<uint8_t>(ReadInteger(object, KeyBusy, generator.Busy, 0, 100));
            generator.Keep = static_cast<uint8_t>(ReadInteger(object, KeyKeep, generator.Keep, 0, 100));

            if (auto const velocity = ReadArray(object, KeyVelocity); velocity != nullptr && velocity.Size() == 2 &&
                IsType(velocity.GetAt(0), mjson::JsonValueType::Number) && IsType(velocity.GetAt(1), mjson::JsonValueType::Number))
            {
                auto const clamp16 = [](double value) { return static_cast<uint16_t>(std::isfinite(value) ? std::clamp(std::round(value), 0.0, 65535.0) : 0.0); };
                generator.VelocityLow = clamp16(velocity.GetAt(0).GetNumber());
                generator.VelocityHigh = clamp16(velocity.GetAt(1).GetNumber());
            }

            generator.ArpSource = ReadRawString(object, KeySource).value_or(ValueHeld) == ValueTrack ? ArpeggiatorSource::Track : ArpeggiatorSource::Held;
            generator.ArpTrackId = ReadRawString(object, KeyArpTrack).value_or(L"");

            auto const order = ReadRawString(object, KeyOrder).value_or(L"");
            auto const ordered = std::find_if(ArpOrderNames.begin(), ArpOrderNames.end(), [&order](wchar_t const* name) { return order == name; });

            if (ordered != ArpOrderNames.end())
            {
                generator.ArpOrder = static_cast<ArpeggiatorOrder>(std::distance(ArpOrderNames.begin(), ordered));
            }

            generator.ArpOctaves = static_cast<uint8_t>(ReadInteger(object, KeyOctaves, 1, 1, 4));
            generator.ArpLatch = ReadBool(object, KeyLatch, false);

            generator.Unknown = UnknownKeys(object, {
                KeyType, KeyStepTicks, KeyEachPass, KeyChannel, KeyNoteLength, KeyRings, KeyScale, KeyRoot, KeyLow, KeyHigh, KeyBusy, KeyKeep, KeyVelocity,
                KeySource, KeyArpTrack, KeyOrder, KeyOctaves, KeyLatch });

            return generator;
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

            if (clip.Kind == ClipKind::Pattern)
            {
                // A pattern's notes come from its steps, so any written with it are left out.
                clip.Pattern = ReadPattern(context, ReadObject(object, KeySettings));
            }
            else if (clip.Kind == ClipKind::Generator)
            {
                // The same for a generator's.
                clip.Generator = ReadGenerator(context, ReadObject(object, KeySettings));
            }
            else
            {
                clip.Settings = ReadObject(object, KeySettings);
                ReadNotes(context, ReadArray(object, KeyNotes), clip);
            }

            ReadEvents(context, ReadArray(object, KeyEvents), clip);
            clip.Lanes = ReadLanes(context, ReadArray(object, KeyLanes));

            clip.Unknown = UnknownKeys(object, {
                KeyId, KeyKind, KeyName, KeyColor, KeyLength, KeyLoop, KeySeed, KeyOrigin, KeySettings, KeyNotes, KeyEvents, KeyLanes });

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

        std::wstring_view LaneTargetText(_In_ LaneTarget target) noexcept
        {
            for (auto const& [kind, text] : LaneTargetNames)
            {
                if (kind == target)
                {
                    return text;
                }
            }

            return LaneTargetNames.front().second;
        }

        void WriteLanes(_Inout_ std::wstring& out, _In_ std::vector<AutomationLane> const& lanes, _In_ int32_t depth)
        {
            out += L"[";

            for (size_t i = 0; i < lanes.size(); ++i)
            {
                auto const& lane = lanes[i];

                out += i == 0 ? L"\n" : L",\n";
                AppendIndent(out, depth + 1);
                out += L"{";

                Members members{ out, depth + 1, false };
                members.Text(KeyId, lane.Id);

                if (!lane.Name.empty())
                {
                    members.Text(KeyName, lane.Name);
                }

                members.Text(KeyTarget, LaneTargetText(lane.Target));

                if (lane.Target == LaneTarget::Controller)
                {
                    members.Integer(KeyNumber, lane.Number);
                }
                else if (lane.Target == LaneTarget::Registered || lane.Target == LaneTarget::Assignable)
                {
                    members.Integer(KeyBank, lane.Number >> 7);
                    members.Integer(KeyIndex, lane.Number & 0x7F);
                }

                members.Integer(KeyChannel, lane.Channel);
                members.Key(KeyPoints);
                out += L"[";

                for (size_t p = 0; p < lane.Points.size(); ++p)
                {
                    auto const& point = lane.Points[p];

                    out += p == 0 ? L"\n" : L",\n";
                    AppendIndent(out, depth + 3);
                    out += L"[";
                    AppendInteger(out, point.Tick);
                    out += L", ";
                    AppendInteger(out, point.Value);

                    if (point.Hold)
                    {
                        out += L", ";
                        AppendQuoted(out, ValueHold);
                    }

                    out += L"]";
                }

                if (!lane.Points.empty())
                {
                    out += L"\n";
                    AppendIndent(out, depth + 2);
                }

                out += L"]";

                members.Unknown(lane.Unknown);
                members.Close();
            }

            out += L"\n";
            AppendIndent(out, depth);
            out += L"]";
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

                if (!track.Lanes.empty())
                {
                    members.Key(KeyLanes);
                    WriteLanes(out, track.Lanes, depth + 1);
                }

                if (!track.LanesOpen)
                {
                    members.Bool(KeyLanesOpen, false);
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

        std::wstring ConditionText(_In_ PatternStep const& step)
        {
            switch (step.Condition)
            {
            case StepCondition::Every: return std::format(L"{}:{}", step.Of, step.Every);
            case StepCondition::FirstPass: return ValueFirstPass;
            case StepCondition::NotFirstPass: return ValueNotFirstPass;
            default: return {};
            }
        }

        // [index, velocity], with an object of the settings that aren't the defaults after them.
        std::wstring PatternStepText(_In_ PatternStep const& step, _In_ PatternMode mode, _In_ uint8_t laneNote)
        {
            std::wstring text{ L"[" };
            AppendInteger(text, step.Index);
            text += L", ";
            AppendInteger(text, step.Velocity);

            std::wstring options{ L"{" };
            Members members{ options, 0, true };

            if (step.Chance != 100) members.Integer(KeyChance, step.Chance);
            if (step.Ratchet > 1) members.Integer(KeyRatchet, step.Ratchet);
            if (step.Shape == RatchetShape::Softer) members.Text(KeyShape, ValueSofter);
            if (step.Shape == RatchetShape::Louder) members.Text(KeyShape, ValueLouder);
            if (step.Condition != StepCondition::Always) members.Text(KeyCondition, ConditionText(step));
            if (step.Nudge != 0) members.Integer(KeyNudge, step.Nudge);
            if (step.Length != 50) members.Integer(KeyLength, step.Length);
            if (mode == PatternMode::Melodic && step.Note != laneNote) members.Integer(KeyNote, step.Note);
            if (step.Accent) members.Bool(KeyAccent, true);
            if (step.Slide) members.Bool(KeySlide, true);

            members.Close();

            if (options != L"{}")
            {
                text += L", ";
                text += options;
            }

            text += L"]";
            return text;
        }

        // depth is the indent of the line the "settings" key is on.
        void WritePattern(_Inout_ std::wstring& out, _In_ PatternSettings const& pattern, _In_ int32_t depth)
        {
            out += L"{";

            Members members{ out, depth, false };
            members.Text(KeyMode, pattern.Mode == PatternMode::Melodic ? ValueMelodic : ValueDrums);
            members.Integer(KeyStepCount, pattern.StepCount);
            members.Integer(KeyStepTicks, pattern.StepTicks);
            members.Integer(KeySwing, pattern.Swing);
            members.Bool(KeyLanesKeepLength, pattern.LanesKeepLength);
            members.Integer(KeyChannel, pattern.Channel);
            members.Key(KeyLanes);
            out += L"[";

            for (size_t i = 0; i < pattern.Lanes.size(); ++i)
            {
                auto const& lane = pattern.Lanes[i];

                out += i == 0 ? L"\n" : L",\n";
                AppendIndent(out, depth + 2);
                out += L"{";

                Members laneMembers{ out, depth + 2, false };
                laneMembers.Text(KeyId, lane.Id);

                if (!lane.Name.empty())
                {
                    laneMembers.Text(KeyName, lane.Name);
                }

                laneMembers.Integer(KeyNote, lane.Note);
                laneMembers.Integer(KeyStepCount, lane.StepCount);

                if (lane.Muted) laneMembers.Bool(KeyMute, true);
                if (lane.Soloed) laneMembers.Bool(KeySolo, true);

                std::wstring steps{ L"[" };

                for (size_t s = 0; s < lane.Steps.size(); ++s)
                {
                    steps += s == 0 ? L" " : L", ";
                    steps += PatternStepText(lane.Steps[s], pattern.Mode, lane.Note);
                }

                steps += lane.Steps.empty() ? L"]" : L" ]";
                laneMembers.Raw(KeySteps, steps);

                laneMembers.Unknown(lane.Unknown);
                laneMembers.Close();
            }

            if (!pattern.Lanes.empty())
            {
                out += L"\n";
                AppendIndent(out, depth + 1);
            }

            out += L"]";

            members.Unknown(pattern.Unknown);
            members.Close();
        }

        // depth is the indent of the line the "settings" key is on.
        void WriteGenerator(_Inout_ std::wstring& out, _In_ GeneratorSettings const& generator, _In_ int32_t depth)
        {
            out += L"{";

            Members members{ out, depth, false };
            members.Text(KeyType,
                generator.Type == GeneratorType::ChanceMelody ? ValueChanceMelody :
                generator.Type == GeneratorType::Arpeggiator ? ValueArpeggiator : ValueEuclidean);
            members.Integer(KeyStepTicks, generator.StepTicks);
            members.Text(KeyEachPass, generator.ChangesEachPass ? ValueChanges : ValueSame);
            members.Integer(KeyChannel, generator.Channel);
            members.Integer(KeyNoteLength, generator.NoteLength);

            if (generator.Type == GeneratorType::Euclidean)
            {
                members.Key(KeyRings);
                out += L"[";

                for (size_t i = 0; i < generator.Rings.size(); ++i)
                {
                    auto const& ring = generator.Rings[i];

                    out += i == 0 ? L"\n" : L",\n";
                    AppendIndent(out, depth + 2);
                    out += L"{";

                    Members ringMembers{ out, depth + 2, true };
                    ringMembers.Integer(KeyNote, ring.Note);
                    ringMembers.Integer(KeyPulses, ring.Pulses);
                    ringMembers.Integer(KeySteps, ring.Steps);
                    ringMembers.Integer(KeyRotate, ring.Rotate);
                    ringMembers.Integer(KeyVelocity, ring.Velocity);
                    ringMembers.Integer(KeyChance, ring.Chance);
                    ringMembers.Close();
                }

                if (!generator.Rings.empty())
                {
                    out += L"\n";
                    AppendIndent(out, depth + 1);
                }

                out += L"]";
            }
            else if (generator.Type == GeneratorType::ChanceMelody)
            {
                members.Text(KeyScale, ScaleNames[std::min<size_t>(static_cast<size_t>(generator.Scale), ScaleNames.size() - 1)]);
                members.Integer(KeyRoot, generator.ScaleRoot);
                members.Integer(KeyLow, generator.LowNote);
                members.Integer(KeyHigh, generator.HighNote);
                members.Integer(KeyBusy, generator.Busy);
                members.Integer(KeyKeep, generator.Keep);
                members.Raw(KeyVelocity, std::format(L"[{}, {}]", generator.VelocityLow, generator.VelocityHigh));
            }
            else if (generator.Type == GeneratorType::Arpeggiator)
            {
                members.Text(KeySource, generator.ArpSource == ArpeggiatorSource::Track ? ValueTrack : ValueHeld);

                if (generator.ArpSource == ArpeggiatorSource::Track)
                {
                    members.Text(KeyArpTrack, generator.ArpTrackId);
                }

                members.Text(KeyOrder, ArpOrderNames[std::min<size_t>(static_cast<size_t>(generator.ArpOrder), ArpOrderNames.size() - 1)]);
                members.Integer(KeyOctaves, generator.ArpOctaves);

                if (generator.ArpLatch)
                {
                    members.Bool(KeyLatch, true);
                }
            }

            members.Unknown(generator.Unknown);
            members.Close();
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

            if (clip.Kind == ClipKind::Pattern && clip.Pattern.has_value())
            {
                members.Key(KeySettings);
                WritePattern(out, *clip.Pattern, depth + 1);
            }
            else if (clip.Kind == ClipKind::Generator && clip.Generator.has_value())
            {
                members.Key(KeySettings);
                WriteGenerator(out, *clip.Generator, depth + 1);
            }
            else if (clip.Settings != nullptr)
            {
                members.Raw(KeySettings, clip.Settings.Stringify());
            }

            // Pattern and generator notes are worked out again when the file is read.
            if (!clip.Notes.empty() && clip.Kind == ClipKind::Notes)
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

            if (!clip.Lanes.empty())
            {
                members.Key(KeyLanes);
                WriteLanes(out, clip.Lanes, depth + 1);
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
        sequence.Copyright = ReadText(root, KeyCopyright, limits.MaximumTextLength);
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

        sequence.Lyrics = ReadLyricsJson(root, limits, context.Skipped);

        sequence.Unknown = UnknownKeys(root, {
            KeyFileVersion, KeyName, midiapp::ProvenanceKey, KeyTicksPerQuarterNote, KeyTempo, KeyMeter,
            KeyTags, KeyScenes, KeyTracks, KeyClips, LyricsKey, KeyCopyright });

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

        if (!sequence.Copyright.empty())
        {
            root.Text(KeyCopyright, sequence.Copyright);
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

        if (sequence.Lyrics)
        {
            root.Raw(LyricsKey, LyricsToJsonText(*sequence.Lyrics));
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
