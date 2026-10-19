// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "DawProject.h"

#include "SequenceGenerator.h"
#include "SequencePattern.h"
#include "ZipArchive.h"

#include <windows.h>
#include <objbase.h>
#include <xmllite.h>
#include <wrl/client.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>

#pragma comment(lib, "xmllite.lib")

namespace midisequencer
{
    namespace
    {
        // ---------------------------------------------------------------- limits

        // A file can come from anywhere. These are far above what a real project holds.
        constexpr size_t MaximumProjectBytes = 32u * 1024u * 1024u;
        constexpr size_t MaximumMetadataBytes = 1024u * 1024u;
        constexpr uint32_t MaximumZipEntries = 65536;
        constexpr LONG_PTR MaximumXmlDepth = 96;
        constexpr size_t MaximumXmlNodes = 1500000;
        constexpr size_t MaximumXmlAttributes = 6000000;
        constexpr size_t MaximumXmlText = 1024u * 1024u;

        // How far content is followed inside content: clips in clips, lanes in lanes.
        constexpr int MaximumNesting = 24;

        // Pieces of content gathered in one import, counting each time content is referred to.
        constexpr size_t MaximumGathered = 20000000;

        constexpr double FullScale = 4294967295.0;
        constexpr uint16_t DefaultVelocity = 51603;         // 100 of 127
        constexpr uint16_t DefaultReleaseVelocity = 0x8000;

        // A pattern or generator goes out as the notes it plays, like Make notes in the editor.
        constexpr int64_t RenderedMaximumBars = 64;
        constexpr uint32_t RenderedGeneratorPasses = 4;

        SequenceLimits const Limits{};

        // ---------------------------------------------------------------- XML in

        struct XmlNode
        {
            std::wstring Name{};
            std::vector<std::pair<std::wstring, std::wstring>> Attributes{};
            std::vector<XmlNode> Children{};
            std::wstring Text{};

            std::wstring const* Attribute(_In_ std::wstring_view name) const noexcept
            {
                for (auto const& [key, value] : Attributes)
                {
                    if (key == name)
                    {
                        return &value;
                    }
                }

                return nullptr;
            }

            XmlNode const* Child(_In_ std::wstring_view name) const noexcept
            {
                for (auto const& child : Children)
                {
                    if (child.Name == name)
                    {
                        return &child;
                    }
                }

                return nullptr;
            }
        };

        DawProjectStatus ParseXml(_In_ std::vector<uint8_t> const& bytes, _Out_ XmlNode& root)
        {
            root = XmlNode{};

            if (bytes.size() > ULONG_MAX)
            {
                return DawProjectStatus::TooLarge;
            }

            Microsoft::WRL::ComPtr<IStream> stream{};

            if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
            {
                return DawProjectStatus::Damaged;
            }

            ULONG written{ 0 };
            LARGE_INTEGER const start{};

            if (FAILED(stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &written)) || written != bytes.size() ||
                FAILED(stream->Seek(start, STREAM_SEEK_SET, nullptr)))
            {
                return DawProjectStatus::Damaged;
            }

            Microsoft::WRL::ComPtr<IXmlReader> reader{};

            if (FAILED(::CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr)))
            {
                return DawProjectStatus::Damaged;
            }

            // No DTD means no entities, so nothing in the file can expand.
            if (FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) ||
                FAILED(reader->SetProperty(XmlReaderProperty_MaxElementDepth, MaximumXmlDepth)) ||
                FAILED(reader->SetInput(stream.Get())))
            {
                return DawProjectStatus::Damaged;
            }

            std::vector<XmlNode> open{};
            size_t nodes{ 0 };
            size_t attributes{ 0 };
            size_t text{ 0 };
            auto haveRoot{ false };

            auto const finish = [&](XmlNode&& node)
            {
                if (open.empty())
                {
                    root = std::move(node);
                    haveRoot = true;
                }
                else
                {
                    open.back().Children.push_back(std::move(node));
                }
            };

            XmlNodeType type{ XmlNodeType_None };
            HRESULT hr{ S_OK };

            while ((hr = reader->Read(&type)) == S_OK)
            {
                switch (type)
                {
                case XmlNodeType_Element:
                {
                    if (++nodes > MaximumXmlNodes)
                    {
                        return DawProjectStatus::TooLarge;
                    }

                    XmlNode node{};
                    wchar_t const* name{ nullptr };
                    UINT length{ 0 };

                    if (FAILED(reader->GetLocalName(&name, &length)))
                    {
                        return DawProjectStatus::Damaged;
                    }

                    node.Name.assign(name, length);

                    // Asked before moving to the attributes, while the reader is on the element.
                    auto const empty = reader->IsEmptyElement() != FALSE;

                    for (hr = reader->MoveToFirstAttribute(); hr == S_OK; hr = reader->MoveToNextAttribute())
                    {
                        if (++attributes > MaximumXmlAttributes)
                        {
                            return DawProjectStatus::TooLarge;
                        }

                        wchar_t const* key{ nullptr };
                        UINT keyLength{ 0 };
                        wchar_t const* value{ nullptr };
                        UINT valueLength{ 0 };

                        if (FAILED(reader->GetLocalName(&key, &keyLength)) || FAILED(reader->GetValue(&value, &valueLength)))
                        {
                            return DawProjectStatus::Damaged;
                        }

                        node.Attributes.emplace_back(std::wstring{ key, keyLength }, std::wstring{ value, valueLength });
                    }

                    if (FAILED(hr))
                    {
                        return DawProjectStatus::Damaged;
                    }

                    if (empty)
                    {
                        finish(std::move(node));
                    }
                    else
                    {
                        open.push_back(std::move(node));
                    }

                    break;
                }

                case XmlNodeType_EndElement:
                {
                    if (open.empty())
                    {
                        return DawProjectStatus::Damaged;
                    }

                    auto node = std::move(open.back());
                    open.pop_back();
                    finish(std::move(node));
                    break;
                }

                case XmlNodeType_Text:
                case XmlNodeType_CDATA:
                {
                    if (open.empty())
                    {
                        break;
                    }

                    wchar_t const* value{ nullptr };
                    UINT length{ 0 };

                    if (FAILED(reader->GetValue(&value, &length)))
                    {
                        return DawProjectStatus::Damaged;
                    }

                    text += length;

                    if (text > MaximumXmlText)
                    {
                        return DawProjectStatus::TooLarge;
                    }

                    open.back().Text.append(value, length);
                    break;
                }

                default:
                    break;
                }
            }

            return FAILED(hr) || !open.empty() || !haveRoot ? DawProjectStatus::Damaged : DawProjectStatus::Read;
        }

        // ---------------------------------------------------------------- values

        std::optional<double> Number(_In_opt_ std::wstring const* text) noexcept
        {
            if (text == nullptr || text->empty() || text->size() > 64)
            {
                return std::nullopt;
            }

            char narrow[65]{};
            size_t length{ 0 };

            for (auto const c : *text)
            {
                if (c > 0x7F)
                {
                    return std::nullopt;
                }

                if (!std::iswspace(c))
                {
                    narrow[length++] = static_cast<char>(c);
                }
            }

            auto begin = narrow;

            // from_chars doesn't take a leading plus.
            if (length > 0 && *begin == '+')
            {
                ++begin;
            }

            double value{ 0 };
            auto const [end, error] = std::from_chars(begin, narrow + length, value);

            if (error != std::errc{} || end != narrow + length || !std::isfinite(value))
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<int64_t> Integer(_In_opt_ std::wstring const* text) noexcept
        {
            auto const value = Number(text);

            if (!value.has_value() || std::floor(*value) != *value || std::abs(*value) > 1e15)
            {
                return std::nullopt;
            }

            return static_cast<int64_t>(*value);
        }

        bool IsTrue(_In_opt_ std::wstring const* text) noexcept
        {
            return text != nullptr && (*text == L"true" || *text == L"1");
        }

        std::optional<uint32_t> Color(_In_opt_ std::wstring const* text) noexcept
        {
            if (text == nullptr || (text->size() != 7 && text->size() != 9) || (*text)[0] != L'#')
            {
                return std::nullopt;
            }

            uint32_t value{ 0 };

            for (size_t i = 1; i < 7; ++i)
            {
                auto const c = (*text)[i];
                auto const digit = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;

                if (digit < 0)
                {
                    return std::nullopt;
                }

                value = (value << 4) | static_cast<uint32_t>(digit);
            }

            return value;
        }

        std::wstring Capped(_In_opt_ std::wstring const* text)
        {
            if (text == nullptr)
            {
                return {};
            }

            return text->substr(0, Limits.MaximumTextLength);
        }

        // contentType is a list of words, such as "audio notes".
        bool HasWord(_In_opt_ std::wstring const* list, _In_ std::wstring_view word) noexcept
        {
            if (list == nullptr)
            {
                return false;
            }

            std::wstring_view rest{ *list };

            while (!rest.empty())
            {
                auto const space = rest.find(L' ');
                auto const item = rest.substr(0, space);

                if (item == word)
                {
                    return true;
                }

                if (space == std::wstring_view::npos)
                {
                    break;
                }

                rest.remove_prefix(space + 1);
            }

            return false;
        }

        enum class TimeUnit : uint8_t
        {
            Beats,
            Seconds,
        };

        TimeUnit UnitFrom(_In_opt_ std::wstring const* text, _In_ TimeUnit inherited) noexcept
        {
            if (text == nullptr)
            {
                return inherited;
            }

            return *text == L"seconds" ? TimeUnit::Seconds : *text == L"beats" ? TimeUnit::Beats : inherited;
        }

        TimeUnit UnitOf(_In_ XmlNode const& node, _In_ TimeUnit inherited) noexcept
        {
            return UnitFrom(node.Attribute(L"timeUnit"), inherited);
        }

        uint32_t ToFullScale(_In_ double normalized) noexcept
        {
            return static_cast<uint32_t>(std::llround(std::clamp(normalized, 0.0, 1.0) * FullScale));
        }

        uint16_t ToVelocity(_In_ double normalized) noexcept
        {
            return static_cast<uint16_t>(std::lround(std::clamp(normalized, 0.0, 1.0) * 65535.0));
        }

        // ---------------------------------------------------------------- the import

        // What a clip's content holds, at ticks from the content's own start.
        struct Gathered
        {
            std::vector<Note> Notes{};
            std::vector<AutomationLane> Lanes{};
            bool HasNotes{ false };
            bool HasAudio{ false };
        };

        class Importer
        {
        public:
            explicit Importer(_In_ ImportedDawProject& result) noexcept : m_result(result)
            {
            }

            void Read(_In_ XmlNode const& project)
            {
                IndexIds(project, 0);

                if (auto const application = project.Child(L"Application"); application != nullptr)
                {
                    m_result.Application = Capped(application->Attribute(L"name"));
                }

                auto const arrangement = project.Child(L"Arrangement");

                ReadTransport(project.Child(L"Transport"));
                ReadTempoAndMeter(arrangement);

                if (auto const structure = project.Child(L"Structure"); structure != nullptr)
                {
                    for (auto const& child : structure->Children)
                    {
                        if (child.Name == L"Track")
                        {
                            if (auto track = ReadTrack(child, 0); track.has_value())
                            {
                                m_work.Tracks.push_back(std::move(*track));
                            }
                        }
                    }
                }

                if (arrangement != nullptr)
                {
                    for (auto const& child : arrangement->Children)
                    {
                        if (child.Name == L"Lanes")
                        {
                            ReadLanes(child, std::wstring{}, TimeUnit::Beats, 0);
                        }
                        else if (child.Name == L"Markers")
                        {
                            ReadMarkers(child, TimeUnit::Beats, m_result.Tags);
                        }
                    }
                }

                ReadScenes(project.Child(L"Scenes"));

                if (m_tooLarge || m_work.Clips.size() > Limits.MaximumClips)
                {
                    m_result.Status = DawProjectStatus::TooLarge;
                    return;
                }

                SizeSlots(m_work.Tracks);

                for (auto& clip : m_work.Clips)
                {
                    SortClip(clip);
                }

                m_result.Tracks = std::move(m_work.Tracks);
                m_result.Clips = std::move(m_work.Clips);
            }

        private:
            void IndexIds(_In_ XmlNode const& node, _In_ int depth)
            {
                if (depth > MaximumXmlDepth)
                {
                    return;
                }

                if (auto const id = node.Attribute(L"id"); id != nullptr)
                {
                    m_byId.try_emplace(*id, &node);
                }

                for (auto const& child : node.Children)
                {
                    IndexIds(child, depth + 1);
                }
            }

            // ---- time ----

            int64_t ToTick(_In_ double value, _In_ TimeUnit unit, _In_ int64_t origin) const
            {
                double tick{ 0 };

                if (unit == TimeUnit::Seconds)
                {
                    auto const seconds = std::max(0.0, m_tempoMap.SecondsAtTick(origin) + value);
                    tick = static_cast<double>(m_tempoMap.TickAtSeconds(std::min(seconds, 1.0e7)));
                }
                else
                {
                    tick = static_cast<double>(origin) + value * static_cast<double>(TicksPerQuarterNote);
                }

                return std::llround(std::clamp(tick, 0.0, static_cast<double>(Limits.MaximumTick)));
            }

            // A time inside content, from the content's start. Seconds use the tempo where the
            // content sits on the timeline.
            int64_t Relative(_In_ double value, _In_ TimeUnit unit, _In_ int64_t base) const
            {
                if (unit == TimeUnit::Beats)
                {
                    return std::llround(std::clamp(value * static_cast<double>(TicksPerQuarterNote), -static_cast<double>(Limits.MaximumTick), static_cast<double>(Limits.MaximumTick)));
                }

                return ToTick(value, unit, base) - base;
            }

            // ---- tempo and meter ----

            void ReadTransport(_In_opt_ XmlNode const* transport)
            {
                auto bpm{ 120.0 };
                uint8_t numerator{ 4 };
                uint8_t denominator{ 4 };

                if (transport != nullptr)
                {
                    if (auto const tempo = transport->Child(L"Tempo"); tempo != nullptr)
                    {
                        bpm = std::clamp(Number(tempo->Attribute(L"value")).value_or(120.0), MinimumBeatsPerMinute, MaximumBeatsPerMinute);
                    }

                    if (auto const meter = transport->Child(L"TimeSignature"); meter != nullptr)
                    {
                        ReadMeterValues(*meter, numerator, denominator);
                    }
                }

                m_result.Tempo = { TempoPoint{ 0, bpm, false } };
                m_result.Meter = { MeterChange{ 0, numerator, denominator } };
                m_tempoMap = TempoMap{ m_result.Tempo };
            }

            static void ReadMeterValues(_In_ XmlNode const& node, _Inout_ uint8_t& numerator, _Inout_ uint8_t& denominator) noexcept
            {
                auto const top = Integer(node.Attribute(L"numerator"));
                auto const bottom = Integer(node.Attribute(L"denominator"));

                if (top.has_value() && *top >= 1 && *top <= 32)
                {
                    numerator = static_cast<uint8_t>(*top);
                }

                if (bottom.has_value() && (*bottom == 1 || *bottom == 2 || *bottom == 4 || *bottom == 8 || *bottom == 16 || *bottom == 32))
                {
                    denominator = static_cast<uint8_t>(*bottom);
                }
            }

            void ReadTempoAndMeter(_In_opt_ XmlNode const* arrangement)
            {
                if (arrangement == nullptr)
                {
                    return;
                }

                if (auto const automation = arrangement->Child(L"TempoAutomation"); automation != nullptr)
                {
                    auto const unit = UnitOf(*automation, TimeUnit::Beats);
                    std::vector<TempoPoint> points{};

                    // Seconds become ticks a point at a time, at the tempo up to that point.
                    double lastSeconds{ 0 };
                    int64_t lastTick{ 0 };
                    auto lastBpm = m_result.Tempo.front().BeatsPerMinute;
                    auto lastRamp{ false };

                    for (auto const& point : automation->Children)
                    {
                        if (point.Name != L"RealPoint" || points.size() >= Limits.MaximumTempoPoints)
                        {
                            continue;
                        }

                        auto const time = Number(point.Attribute(L"time"));
                        auto const value = Number(point.Attribute(L"value"));

                        if (!time.has_value() || !value.has_value() || *time < 0)
                        {
                            continue;
                        }

                        auto const bpm = std::clamp(*value, MinimumBeatsPerMinute, MaximumBeatsPerMinute);
                        int64_t tick{ 0 };

                        if (unit == TimeUnit::Seconds)
                        {
                            auto const pace = lastRamp ? (lastBpm + bpm) / 2.0 : lastBpm;
                            auto const ticks = std::max(0.0, *time - lastSeconds) * pace / 60.0 * static_cast<double>(TicksPerQuarterNote);
                            tick = lastTick + std::llround(std::min(ticks, static_cast<double>(Limits.MaximumTick)));
                            lastSeconds = *time;
                        }
                        else
                        {
                            tick = ToTick(*time, TimeUnit::Beats, 0);
                        }

                        // A tempo usually jumps, so a point that doesn't say how it moves on holds.
                        auto const interpolation = point.Attribute(L"interpolation");
                        auto const ramp = interpolation != nullptr && *interpolation == L"linear";

                        points.push_back(TempoPoint{ std::min(tick, Limits.MaximumTick), bpm, ramp });
                        lastTick = points.back().Tick;
                        lastBpm = bpm;
                        lastRamp = ramp;
                    }

                    if (!points.empty())
                    {
                        std::stable_sort(points.begin(), points.end(), [](TempoPoint const& a, TempoPoint const& b) { return a.Tick < b.Tick; });

                        if (points.front().Tick > 0)
                        {
                            points.insert(points.begin(), TempoPoint{ 0, m_result.Tempo.front().BeatsPerMinute, false });
                        }

                        // Two points at one tick: the later one wins.
                        std::vector<TempoPoint> unique{};

                        for (auto const& point : points)
                        {
                            if (!unique.empty() && unique.back().Tick == point.Tick)
                            {
                                unique.back() = point;
                            }
                            else
                            {
                                unique.push_back(point);
                            }
                        }

                        unique.back().RampToNext = false;
                        m_result.Tempo = std::move(unique);
                        m_tempoMap = TempoMap{ m_result.Tempo };
                    }
                }

                if (auto const automation = arrangement->Child(L"TimeSignatureAutomation"); automation != nullptr)
                {
                    auto const unit = UnitOf(*automation, TimeUnit::Beats);
                    std::vector<MeterChange> changes{};

                    for (auto const& point : automation->Children)
                    {
                        if (point.Name != L"TimeSignaturePoint" || changes.size() >= Limits.MaximumTempoPoints)
                        {
                            continue;
                        }

                        auto const time = Number(point.Attribute(L"time"));

                        if (!time.has_value() || *time < 0)
                        {
                            continue;
                        }

                        MeterChange change{};
                        change.Tick = ToTick(*time, unit, 0);
                        ReadMeterValues(point, change.Numerator, change.Denominator);
                        changes.push_back(change);
                    }

                    if (!changes.empty())
                    {
                        std::stable_sort(changes.begin(), changes.end(), [](MeterChange const& a, MeterChange const& b) { return a.Tick < b.Tick; });

                        if (changes.front().Tick > 0)
                        {
                            changes.insert(changes.begin(), m_result.Meter.front());
                        }

                        m_result.Meter = std::move(changes);
                    }
                }
            }

            // ---- tracks ----

            std::optional<Track> ReadTrack(_In_ XmlNode const& node, _In_ size_t depth)
            {
                // Depths 0 to 15, as a sequence file keeps them.
                if (depth >= Limits.MaximumFolderDepth || ++m_trackCount > Limits.MaximumTracks)
                {
                    return std::nullopt;
                }

                auto const channel = node.Child(L"Channel");
                auto const role = channel != nullptr ? channel->Attribute(L"role") : nullptr;
                auto const hasChildren = node.Child(L"Track") != nullptr;
                std::wstring instrument{};

                if (channel != nullptr)
                {
                    if (auto const devices = channel->Child(L"Devices"); devices != nullptr)
                    {
                        for (auto const& device : devices->Children)
                        {
                            ++m_result.LeftOut.Plugins;

                            auto const deviceRole = device.Attribute(L"deviceRole");

                            if (instrument.empty() && deviceRole != nullptr && *deviceRole == L"instrument")
                            {
                                instrument = Capped(device.Attribute(L"deviceName"));

                                if (instrument.empty())
                                {
                                    instrument = Capped(device.Attribute(L"name"));
                                }
                            }
                        }
                    }
                }

                // The master, effect returns and VCAs are the mixer, not music.
                if (!hasChildren && role != nullptr && (*role == L"master" || *role == L"effect" || *role == L"vca"))
                {
                    return std::nullopt;
                }

                Track track{};
                track.Id = NewId(L"t");
                track.Name = Capped(node.Attribute(L"name"));
                track.Color = Color(node.Attribute(L"color")).value_or(0x9E9E9E);

                if (channel != nullptr)
                {
                    if (auto const mute = channel->Child(L"Mute"); mute != nullptr)
                    {
                        track.Muted = IsTrue(mute->Attribute(L"value"));
                    }

                    track.Soloed = IsTrue(channel->Attribute(L"solo"));
                }

                if (hasChildren)
                {
                    track.IsFolder = true;

                    for (auto const& child : node.Children)
                    {
                        if (child.Name == L"Track")
                        {
                            if (auto inner = ReadTrack(child, depth + 1); inner.has_value())
                            {
                                track.Children.push_back(std::move(*inner));
                            }
                        }
                    }

                    if (track.Children.empty())
                    {
                        return std::nullopt;
                    }
                }
                else if (auto const contentType = node.Attribute(L"contentType");
                    !HasWord(contentType, L"notes") && (contentType != nullptr || instrument.empty()))
                {
                    if (HasWord(contentType, L"audio"))
                    {
                        ++m_result.LeftOut.AudioTracks;
                    }

                    return std::nullopt;
                }
                else
                {
                    // Notes keep the channel the file gave them.
                    track.Destination.Channel = -1;

                    if (!instrument.empty())
                    {
                        m_result.Instruments.emplace_back(track.Id, instrument);
                    }
                }

                if (auto const id = node.Attribute(L"id"); id != nullptr)
                {
                    m_trackIds[*id] = track.Id;
                }

                if (channel != nullptr)
                {
                    if (auto const id = channel->Attribute(L"id"); id != nullptr)
                    {
                        m_trackIds.try_emplace(*id, track.Id);
                    }
                }

                return track;
            }

            void SizeSlots(_Inout_ std::vector<Track>& tracks)
            {
                for (auto& track : tracks)
                {
                    track.Slots.resize(m_result.Scenes.size());
                    SizeSlots(track.Children);
                }
            }

            // The track a timeline element belongs to: its own, or the one it sits in.
            std::wstring OwnerOf(_In_ XmlNode const& node, _In_ std::wstring const& inherited) const
            {
                auto const reference = node.Attribute(L"track");

                if (reference == nullptr)
                {
                    return inherited;
                }

                auto const found = m_trackIds.find(*reference);
                return found != m_trackIds.end() ? found->second : std::wstring{};
            }

            // ---- the arrangement ----

            void ReadLanes(_In_ XmlNode const& lanes, _In_ std::wstring const& trackId, _In_ TimeUnit unit, _In_ int depth)
            {
                if (depth > MaximumNesting)
                {
                    return;
                }

                auto const unitHere = UnitOf(lanes, unit);
                auto const owner = OwnerOf(lanes, trackId);

                for (auto const& child : lanes.Children)
                {
                    auto const childOwner = OwnerOf(child, owner);

                    if (child.Name == L"Lanes")
                    {
                        ReadLanes(child, childOwner, unitHere, depth + 1);
                        continue;
                    }

                    auto const track = childOwner.empty() ? nullptr : FindTrack(m_work, childOwner);

                    if (child.Name == L"Clips")
                    {
                        ReadClips(child, track, unitHere);
                    }
                    else if (child.Name == L"Warps" || child.Name == L"Audio" || child.Name == L"Video")
                    {
                        ++m_result.LeftOut.AudioClips;
                    }
                    else if (track == nullptr || track->IsFolder)
                    {
                        if (child.Name == L"Points")
                        {
                            ++m_result.LeftOut.OtherAutomation;
                        }
                    }
                    else if (child.Name == L"Notes")
                    {
                        ReadLooseNotes(child, *track, unitHere);
                    }
                    else if (child.Name == L"Points")
                    {
                        std::vector<AutomationLane> lanesRead{};
                        ReadPoints(child, unitHere, 0, 0, lanesRead);

                        for (auto& lane : lanesRead)
                        {
                            if (track->Lanes.size() < Limits.MaximumLanesPerOwner)
                            {
                                track->Lanes.push_back(std::move(lane));
                            }
                        }
                    }
                    else if (child.Name == L"markers" || child.Name == L"Markers")
                    {
                        ReadMarkers(child, unitHere, track->Tags);
                    }
                }
            }

            void ReadClips(_In_ XmlNode const& clips, _In_opt_ Track* track, _In_ TimeUnit unit)
            {
                auto const unitHere = UnitOf(clips, unit);

                for (auto const& clip : clips.Children)
                {
                    if (clip.Name != L"Clip")
                    {
                        continue;
                    }

                    if (track == nullptr || track->IsFolder)
                    {
                        if (HoldsAudio(clip, 0))
                        {
                            ++m_result.LeftOut.AudioClips;
                        }

                        continue;
                    }

                    if (auto const placement = ReadClip(clip, unitHere, 0); placement.has_value() &&
                        track->Timeline.size() < Limits.MaximumPlacementsPerTrack)
                    {
                        track->Timeline.push_back(*placement);
                    }
                }
            }

            static bool HoldsAudio(_In_ XmlNode const& node, _In_ int depth) noexcept
            {
                if (depth > MaximumNesting)
                {
                    return false;
                }

                for (auto const& child : node.Children)
                {
                    if (child.Name == L"Audio" || child.Name == L"Warps" || child.Name == L"Video" || HoldsAudio(child, depth + 1))
                    {
                        return true;
                    }
                }

                return false;
            }

            static XmlNode const* ContentOf(_In_ XmlNode const& clip) noexcept
            {
                for (auto const& child : clip.Children)
                {
                    if (child.Name == L"Notes" || child.Name == L"Lanes" || child.Name == L"Points" || child.Name == L"Clips" ||
                        child.Name == L"Warps" || child.Name == L"Audio" || child.Name == L"Video")
                    {
                        return &child;
                    }
                }

                return nullptr;
            }

            // A clip on the timeline or in a slot. Its content plays from playStart: a loop from
            // loopStart to loopEnd, or once, for as long as the clip lasts.
            std::optional<Placement> ReadClip(_In_ XmlNode const& node, _In_ TimeUnit unit, _In_ int64_t origin)
            {
                auto const time = Number(node.Attribute(L"time"));

                if (!time.has_value() || (node.Attribute(L"enable") != nullptr && !IsTrue(node.Attribute(L"enable"))))
                {
                    return std::nullopt;
                }

                auto const start = ToTick(*time, unit, origin);
                auto const duration = Number(node.Attribute(L"duration"));
                auto const end = duration.has_value() ? std::max(start, ToTick(*time + std::max(0.0, *duration), unit, origin)) : int64_t{ -1 };

                auto const contentUnit = UnitFrom(node.Attribute(L"contentTimeUnit"), unit);
                auto const playStart = Number(node.Attribute(L"playStart")).value_or(0.0);
                auto const playStop = Number(node.Attribute(L"playStop"));
                auto const loopStart = Number(node.Attribute(L"loopStart"));
                auto const loopEnd = Number(node.Attribute(L"loopEnd"));

                // Its own content, or the content another clip holds.
                auto content = ContentOf(node);
                std::wstring contentId{};

                if (content == nullptr)
                {
                    if (auto const reference = node.Attribute(L"reference"); reference != nullptr)
                    {
                        if (auto const found = m_byId.find(*reference); found != m_byId.end())
                        {
                            content = found->second;
                            contentId = *reference;
                        }
                    }
                }
                else if (auto const id = content->Attribute(L"id"); id != nullptr)
                {
                    contentId = *id;
                }

                if (content == nullptr)
                {
                    return std::nullopt;
                }

                auto const looping = loopStart.has_value() && loopEnd.has_value() && *loopEnd > *loopStart;

                // Clips that play the same content the same way are one clip, linked.
                std::wstring key{};

                if (!contentId.empty())
                {
                    key = std::format(L"{}|{}|{}|{}|{}|{}", contentId, playStart, playStop.value_or(-1.0), looping ? *loopStart : -1.0, looping ? *loopEnd : -1.0, looping ? int64_t{ -1 } : end - start);

                    if (auto const shared = m_shared.find(key); shared != m_shared.end())
                    {
                        return Placement{ shared->second, start, end >= 0 ? end - start : 0 };
                    }
                }

                Gathered gathered{};
                Gather(*content, contentUnit, 0, start, gathered, 0);

                if (!gathered.HasNotes && gathered.Lanes.empty())
                {
                    if (gathered.HasAudio)
                    {
                        ++m_result.LeftOut.AudioClips;
                    }

                    return std::nullopt;
                }

                Clip clip{};
                clip.Id = NewId(L"c");
                clip.Kind = ClipKind::Notes;
                clip.Name = Capped(node.Attribute(L"name"));
                clip.Color = Color(node.Attribute(L"color"));
                clip.Origin = ClipOrigin::Imported;
                clip.OriginDetail = m_result.Application;

                auto const from = Relative(playStart, contentUnit, start);

                if (looping)
                {
                    auto const a = Relative(*loopStart, contentUnit, start);
                    auto const b = std::max(a + 1, Relative(*loopEnd, contentUnit, start));
                    auto const length = b - a;

                    // Starting part way into the loop is the same loop turned round.
                    auto const turn = std::clamp(from, a, b) - a;
                    auto const place = [&](int64_t tick) { return ((tick - a - turn) % length + length) % length; };

                    for (auto const& note : gathered.Notes)
                    {
                        if (note.Tick >= a && note.Tick < b)
                        {
                            auto copy = note;
                            copy.Tick = place(note.Tick);
                            clip.Notes.push_back(copy);
                        }
                    }

                    for (auto& lane : gathered.Lanes)
                    {
                        AutomationLane kept = lane;
                        kept.Points.clear();

                        for (auto const& point : lane.Points)
                        {
                            if (point.Tick >= a && point.Tick < b)
                            {
                                auto copy = point;
                                copy.Tick = place(point.Tick);
                                kept.Points.push_back(copy);
                            }
                        }

                        std::stable_sort(kept.Points.begin(), kept.Points.end(), [](LanePoint const& x, LanePoint const& y) { return x.Tick < y.Tick; });

                        if (!kept.Points.empty())
                        {
                            clip.Lanes.push_back(std::move(kept));
                        }
                    }

                    clip.Length = length;
                    clip.Loop = true;
                }
                else
                {
                    auto to = playStop.has_value() ? Relative(*playStop, contentUnit, start) : end >= 0 ? from + (end - start) : from;

                    if (!playStop.has_value() && end < 0)
                    {
                        for (auto const& note : gathered.Notes)
                        {
                            to = std::max(to, note.Tick + note.Length);
                        }
                    }

                    to = std::max(to, from + 1);

                    for (auto const& note : gathered.Notes)
                    {
                        if (note.Tick >= from && note.Tick < to)
                        {
                            auto copy = note;
                            copy.Tick -= from;
                            clip.Notes.push_back(copy);
                        }
                    }

                    for (auto& lane : gathered.Lanes)
                    {
                        AutomationLane kept = lane;
                        kept.Points.clear();

                        for (auto const& point : lane.Points)
                        {
                            if (point.Tick >= from && point.Tick <= to)
                            {
                                auto copy = point;
                                copy.Tick -= from;
                                kept.Points.push_back(copy);
                            }
                        }

                        if (!kept.Points.empty())
                        {
                            clip.Lanes.push_back(std::move(kept));
                        }
                    }

                    clip.Length = to - from;
                    clip.Loop = false;
                }

                if (clip.Lanes.size() > Limits.MaximumLanesPerOwner)
                {
                    clip.Lanes.resize(Limits.MaximumLanesPerOwner);
                }

                Placement placement{ clip.Id, start, end >= 0 ? end - start : 0 };

                if (!key.empty())
                {
                    m_shared.emplace(key, clip.Id);
                }

                m_work.Clips.push_back(std::move(clip));
                return placement;
            }

            // Everything playable in some content, with ticks from offset. Base is where on the
            // timeline the content sits, for content timed in seconds.
            void Gather(_In_ XmlNode const& content, _In_ TimeUnit unit, _In_ int64_t offset, _In_ int64_t base, _Inout_ Gathered& out, _In_ int depth)
            {
                if (depth > MaximumNesting || m_tooLarge)
                {
                    return;
                }

                // Content can be referred to again and again, and is gathered each time: a file that
                // asks for far more than any project needs is too large, rather than a long wait.
                if (++m_gathered > MaximumGathered)
                {
                    m_tooLarge = true;
                    return;
                }

                auto const unitHere = UnitOf(content, unit);

                if (content.Name == L"Notes")
                {
                    out.HasNotes = true;

                    for (auto const& note : content.Children)
                    {
                        if (note.Name == L"Note")
                        {
                            ReadNote(note, unitHere, offset, base, out.Notes);
                        }
                    }
                }
                else if (content.Name == L"Lanes")
                {
                    for (auto const& child : content.Children)
                    {
                        Gather(child, unitHere, offset, base, out, depth + 1);
                    }
                }
                else if (content.Name == L"Points")
                {
                    ReadPoints(content, unitHere, offset, base, out.Lanes);
                }
                else if (content.Name == L"Clips")
                {
                    // Clips inside a clip play as part of it, from where each one sits.
                    for (auto const& inner : content.Children)
                    {
                        auto const time = inner.Name == L"Clip" ? Number(inner.Attribute(L"time")) : std::nullopt;

                        if (!time.has_value())
                        {
                            continue;
                        }

                        auto const innerUnit = UnitFrom(inner.Attribute(L"contentTimeUnit"), unitHere);
                        auto const innerStart = offset + Relative(*time, unitHere, base);
                        auto const innerPlay = Relative(Number(inner.Attribute(L"playStart")).value_or(0.0), innerUnit, base);

                        if (auto const innerContent = ContentOf(inner); innerContent != nullptr)
                        {
                            Gather(*innerContent, innerUnit, innerStart - innerPlay, base, out, depth + 1);
                        }
                    }
                }
                else if (content.Name == L"Warps" || content.Name == L"Audio" || content.Name == L"Video")
                {
                    out.HasAudio = true;
                }
            }

            void ReadNote(_In_ XmlNode const& node, _In_ TimeUnit unit, _In_ int64_t offset, _In_ int64_t base, _Inout_ std::vector<Note>& notes)
            {
                auto const time = Number(node.Attribute(L"time"));
                auto const duration = Number(node.Attribute(L"duration"));
                auto const key = Integer(node.Attribute(L"key"));

                if (!time.has_value() || !duration.has_value() || !key.has_value() || *key < 0 || *key > 127)
                {
                    return;
                }

                if (++m_notes > Limits.MaximumNotesInSequence)
                {
                    m_tooLarge = true;
                    return;
                }

                Note note{};
                note.Tick = offset + Relative(*time, unit, base);
                note.Length = std::max<int64_t>(1, offset + Relative(*time + std::max(0.0, *duration), unit, base) - note.Tick);
                note.Number = static_cast<uint8_t>(*key);
                note.Channel = static_cast<uint8_t>(std::clamp<int64_t>(Integer(node.Attribute(L"channel")).value_or(0), 0, 15));

                auto const velocity = Number(node.Attribute(L"vel"));
                auto const release = Number(node.Attribute(L"rel"));
                note.Velocity = velocity.has_value() ? ToVelocity(*velocity) : DefaultVelocity;
                note.ReleaseVelocity = release.has_value() ? ToVelocity(*release) : DefaultReleaseVelocity;

                // A note's own expressions, such as its pitch or pressure over time. Counted once,
                // however many clips play the content it's in.
                for (auto const& child : node.Children)
                {
                    if ((child.Name == L"Points" || child.Name == L"Lanes") && m_expressions.insert(&child).second)
                    {
                        ++m_result.LeftOut.NoteExpressions;
                    }
                }

                notes.push_back(note);
            }

            void ReadLooseNotes(_In_ XmlNode const& notes, _Inout_ Track& track, _In_ TimeUnit unit)
            {
                Gathered gathered{};
                Gather(notes, unit, 0, 0, gathered, 0);

                std::erase_if(gathered.Notes, [](Note const& note) { return note.Tick < 0; });

                if (gathered.Notes.empty())
                {
                    return;
                }

                Clip clip{};
                clip.Id = NewId(L"c");
                clip.Kind = ClipKind::Notes;
                clip.Name = track.Name;
                clip.Loop = false;
                clip.Origin = ClipOrigin::Imported;
                clip.OriginDetail = m_result.Application;

                int64_t end{ 1 };

                for (auto const& note : gathered.Notes)
                {
                    end = std::max(end, note.Tick + note.Length);
                }

                clip.Length = end;
                clip.Notes = std::move(gathered.Notes);

                if (track.Timeline.size() < Limits.MaximumPlacementsPerTrack)
                {
                    track.Timeline.push_back(Placement{ clip.Id, 0, 0 });
                    m_work.Clips.push_back(std::move(clip));
                }
            }

            // Automation a sequence has a lane for: controllers, pitch bend, channel pressure and
            // program. Values become 32 bits.
            void ReadPoints(_In_ XmlNode const& points, _In_ TimeUnit unit, _In_ int64_t offset, _In_ int64_t base, _Inout_ std::vector<AutomationLane>& lanes)
            {
                auto const target = points.Child(L"Target");
                auto const expression = target != nullptr ? target->Attribute(L"expression") : nullptr;

                if (expression == nullptr || target->Attribute(L"parameter") != nullptr)
                {
                    ++m_result.LeftOut.OtherAutomation;
                    return;
                }

                AutomationLane lane{};
                lane.Id = NewId(L"a");

                if (*expression == L"channelController")
                {
                    auto const controller = Integer(target->Attribute(L"controller"));

                    if (!controller.has_value() || *controller < 0 || *controller > 127)
                    {
                        ++m_result.LeftOut.OtherAutomation;
                        return;
                    }

                    lane.Target = LaneTarget::Controller;
                    lane.Number = static_cast<uint16_t>(*controller);
                }
                else if (*expression == L"pitchBend")
                {
                    lane.Target = LaneTarget::PitchBend;
                }
                else if (*expression == L"channelPressure")
                {
                    lane.Target = LaneTarget::ChannelPressure;
                }
                else if (*expression == L"programChange")
                {
                    lane.Target = LaneTarget::Program;
                }
                else
                {
                    ++m_result.LeftOut.OtherAutomation;
                    return;
                }

                lane.Channel = static_cast<uint8_t>(std::clamp<int64_t>(Integer(target->Attribute(L"channel")).value_or(0), 0, 15));

                auto const unitName = points.Attribute(L"unit");
                auto const percent = unitName != nullptr && *unitName == L"percent";
                auto const unitHere = UnitOf(points, unit);

                for (auto const& point : points.Children)
                {
                    if (point.Name != L"RealPoint" && point.Name != L"IntegerPoint" && point.Name != L"EnumPoint")
                    {
                        continue;
                    }

                    auto const time = Number(point.Attribute(L"time"));
                    auto const value = Number(point.Attribute(L"value"));

                    if (!time.has_value() || !value.has_value() || lane.Points.size() >= Limits.MaximumPointsPerLane)
                    {
                        continue;
                    }

                    if (++m_points > Limits.MaximumLanePointsInSequence)
                    {
                        m_tooLarge = true;
                        return;
                    }

                    // A program is a number from 0 to 127 unless it's already from 0 to 1.
                    auto normalized = percent ? *value / 100.0 : *value;

                    if (lane.Target == LaneTarget::Program && (point.Name != L"RealPoint" || *value > 1.0))
                    {
                        normalized = *value / 127.0;
                    }

                    auto const interpolation = point.Attribute(L"interpolation");

                    LanePoint read{};
                    read.Tick = std::max<int64_t>(0, offset + Relative(*time, unitHere, base));
                    read.Value = ToFullScale(normalized);
                    read.Hold = interpolation != nullptr && *interpolation == L"hold";
                    lane.Points.push_back(read);
                }

                if (!lane.Points.empty())
                {
                    std::stable_sort(lane.Points.begin(), lane.Points.end(), [](LanePoint const& a, LanePoint const& b) { return a.Tick < b.Tick; });
                    lanes.push_back(std::move(lane));
                }
            }

            void ReadMarkers(_In_ XmlNode const& markers, _In_ TimeUnit unit, _Inout_ std::vector<Tag>& tags)
            {
                auto const unitHere = UnitOf(markers, unit);

                for (auto const& marker : markers.Children)
                {
                    if (marker.Name != L"Marker" || tags.size() >= Limits.MaximumTags)
                    {
                        continue;
                    }

                    auto const time = Number(marker.Attribute(L"time"));

                    if (!time.has_value())
                    {
                        continue;
                    }

                    Tag tag{};
                    tag.Tick = ToTick(*time, unitHere, 0);
                    tag.Text = Capped(marker.Attribute(L"name"));
                    tag.Color = Color(marker.Attribute(L"color"));
                    tags.push_back(std::move(tag));
                }
            }

            // ---- the launcher ----

            void ReadScenes(_In_opt_ XmlNode const* scenes)
            {
                if (scenes == nullptr)
                {
                    return;
                }

                for (auto const& node : scenes->Children)
                {
                    if (node.Name != L"Scene" || m_result.Scenes.size() >= Limits.MaximumScenes)
                    {
                        continue;
                    }

                    Scene scene{};
                    scene.Id = NewId(L"s");
                    scene.Name = Capped(node.Attribute(L"name"));

                    auto const index = m_result.Scenes.size();
                    m_result.Scenes.push_back(std::move(scene));

                    ReadSlots(node, index, TimeUnit::Beats, 0);
                }
            }

            void ReadSlots(_In_ XmlNode const& node, _In_ size_t scene, _In_ TimeUnit unit, _In_ int depth)
            {
                if (depth > MaximumNesting)
                {
                    return;
                }

                auto const unitHere = UnitOf(node, unit);

                for (auto const& child : node.Children)
                {
                    if (child.Name != L"ClipSlot")
                    {
                        ReadSlots(child, scene, unitHere, depth + 1);
                        continue;
                    }

                    auto const owner = OwnerOf(child, std::wstring{});
                    auto const track = owner.empty() ? nullptr : FindTrack(m_work, owner);
                    auto const clip = child.Child(L"Clip");

                    if (track == nullptr || track->IsFolder || clip == nullptr)
                    {
                        continue;
                    }

                    if (auto const placement = ReadClip(*clip, UnitOf(child, unitHere), 0); placement.has_value())
                    {
                        if (track->Slots.size() <= scene)
                        {
                            track->Slots.resize(scene + 1);
                        }

                        track->Slots[scene] = placement->ClipId;
                    }
                }
            }

            ImportedDawProject& m_result;
            Sequence m_work{};
            TempoMap m_tempoMap{};
            std::unordered_map<std::wstring, std::wstring> m_trackIds{};
            std::unordered_map<std::wstring, XmlNode const*> m_byId{};
            std::map<std::wstring, std::wstring> m_shared{};
            std::set<XmlNode const*> m_expressions{};
            size_t m_trackCount{ 0 };
            size_t m_notes{ 0 };
            size_t m_points{ 0 };
            size_t m_gathered{ 0 };
            bool m_tooLarge{ false };
        };

        void ReadMetadata(_In_ XmlNode const& metadata, _Inout_ ImportedDawProject& result)
        {
            auto const text = [&](wchar_t const* name)
            {
                auto const node = metadata.Child(name);
                return node != nullptr ? node->Text.substr(0, Limits.MaximumTextLength) : std::wstring{};
            };

            result.Title = text(L"Title");
            result.Artist = text(L"Artist");
            result.Copyright = text(L"Copyright");
        }

        DawProjectStatus FromZipStatus(_In_ midiapp::ZipStatus status) noexcept
        {
            switch (status)
            {
            case midiapp::ZipStatus::Read:
                return DawProjectStatus::Read;

            case midiapp::ZipStatus::NotAZip:
            case midiapp::ZipStatus::CannotOpen:
                return DawProjectStatus::NotAProject;

            case midiapp::ZipStatus::TooManyEntries:
            case midiapp::ZipStatus::TooLarge:
                return DawProjectStatus::TooLarge;

            default:
                return DawProjectStatus::Damaged;
            }
        }

        ImportedDawProject ImportFromZip(_In_ midiapp::ZipReader const& zip)
        {
            ImportedDawProject result{};
            midiapp::ZipItem const* project{ nullptr };
            midiapp::ZipItem const* metadata{ nullptr };

            for (auto const& item : zip.Items())
            {
                if (::_wcsicmp(item.Name.c_str(), L"project.xml") == 0)
                {
                    project = &item;
                }
                else if (::_wcsicmp(item.Name.c_str(), L"metadata.xml") == 0)
                {
                    metadata = &item;
                }
            }

            if (project == nullptr)
            {
                result.Status = DawProjectStatus::NotAProject;
                return result;
            }

            XmlNode root{};

            {
                std::vector<uint8_t> bytes{};

                if (auto const extracted = zip.Extract(*project, MaximumProjectBytes, bytes); extracted != midiapp::ZipStatus::Read)
                {
                    result.Status = FromZipStatus(extracted);
                    return result;
                }

                if (auto const parsed = ParseXml(bytes, root); parsed != DawProjectStatus::Read)
                {
                    result.Status = parsed;
                    return result;
                }
            }

            if (root.Name != L"Project")
            {
                result.Status = DawProjectStatus::NotAProject;
                return result;
            }

            Importer importer{ result };
            importer.Read(root);

            if (result.Status != DawProjectStatus::Read)
            {
                return ImportedDawProject{ result.Status };
            }

            // The metadata is a nice to have: a damaged one doesn't stop the import.
            if (metadata != nullptr)
            {
                std::vector<uint8_t> bytes{};
                XmlNode meta{};

                if (zip.Extract(*metadata, MaximumMetadataBytes, bytes) == midiapp::ZipStatus::Read &&
                    ParseXml(bytes, meta) == DawProjectStatus::Read && meta.Name == L"MetaData")
                {
                    ReadMetadata(meta, result);
                }
            }

            return result;
        }

        // ---------------------------------------------------------------- XML out

        std::string Escape(_In_ std::wstring_view text)
        {
            std::wstring escaped{};
            escaped.reserve(text.size());

            for (auto const c : text)
            {
                switch (c)
                {
                case L'&': escaped += L"&amp;"; break;
                case L'<': escaped += L"&lt;"; break;
                case L'>': escaped += L"&gt;"; break;
                case L'"': escaped += L"&quot;"; break;
                case L'\t': escaped += L"&#9;"; break;
                case L'\n': escaped += L"&#10;"; break;
                case L'\r': escaped += L"&#13;"; break;

                default:
                    // XML 1.0 has no way to write the other control characters.
                    if (c >= 0x20 && c != 0xFFFE && c != 0xFFFF)
                    {
                        escaped += c;
                    }

                    break;
                }
            }

            return ToUtf8(escaped);
        }

        std::string Beats(_In_ int64_t ticks)
        {
            return std::format("{:.6f}", static_cast<double>(ticks) / static_cast<double>(TicksPerQuarterNote));
        }

        std::string HexColor(_In_ uint32_t color)
        {
            return std::format("#{:06x}", color & 0xFFFFFF);
        }

        using Attributes = std::vector<std::pair<char const*, std::string>>;

        class XmlOut
        {
        public:
            void Begin(_In_ char const* name, _In_ Attributes const& attributes)
            {
                Start(name, attributes);
                m_text += ">\n";
                ++m_depth;
            }

            void Empty(_In_ char const* name, _In_ Attributes const& attributes)
            {
                Start(name, attributes);
                m_text += "/>\n";
            }

            void End(_In_ char const* name)
            {
                --m_depth;
                m_text.append(m_depth * 2, ' ');
                m_text += "</";
                m_text += name;
                m_text += ">\n";
            }

            void TextElement(_In_ char const* name, _In_ std::wstring const& text)
            {
                m_text.append(m_depth * 2, ' ');
                m_text += std::format("<{}>{}</{}>\n", name, Escape(text), name);
            }

            // Already escaped: for the many notes, written without building attribute lists.
            void Line(_In_ std::string const& line)
            {
                m_text.append(m_depth * 2, ' ');
                m_text += line;
                m_text += '\n';
            }

            std::vector<uint8_t> Take()
            {
                return std::vector<uint8_t>{ m_text.begin(), m_text.end() };
            }

        private:
            void Start(_In_ char const* name, _In_ Attributes const& attributes)
            {
                m_text.append(m_depth * 2, ' ');
                m_text += '<';
                m_text += name;

                for (auto const& [key, value] : attributes)
                {
                    m_text += std::format(" {}=\"{}\"", key, value);
                }
            }

            std::string m_text{ "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n" };
            size_t m_depth{ 0 };
        };

        class Exporter
        {
        public:
            Exporter(_In_ Sequence const& sequence, _In_ DawProjectText const& text, _Inout_ DawProjectExport& result) noexcept :
                m_sequence(sequence), m_text(text), m_result(result)
            {
            }

            std::vector<uint8_t> Project()
            {
                XmlOut x{};
                x.Begin("Project", { { "version", "1.0" } });
                x.Empty("Application", { { "name", Escape(m_text.Application) }, { "version", Escape(m_text.Version.empty() ? std::wstring{ L"1.0" } : m_text.Version) } });

                auto const tempoId = NextId();
                auto const meterId = NextId();
                auto const firstMeter = m_sequence.Meter.empty() ? MeterChange{} : m_sequence.Meter.front();
                auto const firstTempo = m_sequence.Tempo.empty() ? 120.0 : m_sequence.Tempo.front().BeatsPerMinute;

                x.Begin("Transport", {});
                x.Empty("Tempo", { { "id", tempoId }, { "name", "Tempo" }, { "unit", "bpm" }, { "value", std::format("{:.6f}", firstTempo) },
                    { "min", std::format("{:.6f}", MinimumBeatsPerMinute) }, { "max", std::format("{:.6f}", MaximumBeatsPerMinute) } });
                x.Empty("TimeSignature", { { "id", meterId }, { "numerator", std::to_string(firstMeter.Numerator) }, { "denominator", std::to_string(firstMeter.Denominator) } });
                x.End("Transport");

                x.Begin("Structure", {});

                for (auto const& track : m_sequence.Tracks)
                {
                    WriteTrack(x, track);
                }

                x.End("Structure");

                x.Begin("Arrangement", { { "id", NextId() } });
                x.Begin("Lanes", { { "timeUnit", "beats" }, { "id", NextId() } });

                ForEachTrack(m_sequence, [&](Track const& track, size_t)
                {
                    if (!track.IsFolder)
                    {
                        WriteTrackLanes(x, track);
                    }

                    return true;
                });

                x.End("Lanes");

                if (!m_sequence.Tags.empty())
                {
                    x.Begin("Markers", { { "id", NextId() }, { "timeUnit", "beats" } });
                    WriteMarkers(x, m_sequence.Tags);
                    x.End("Markers");
                }

                if (m_sequence.Tempo.size() > 1 || (!m_sequence.Tempo.empty() && m_sequence.Tempo.front().RampToNext))
                {
                    x.Begin("TempoAutomation", { { "id", NextId() }, { "timeUnit", "beats" }, { "unit", "bpm" } });
                    x.Empty("Target", { { "parameter", tempoId } });

                    for (auto const& point : m_sequence.Tempo)
                    {
                        x.Empty("RealPoint", { { "time", Beats(point.Tick) }, { "value", std::format("{:.6f}", point.BeatsPerMinute) },
                            { "interpolation", point.RampToNext ? "linear" : "hold" } });
                    }

                    x.End("TempoAutomation");
                }

                if (m_sequence.Meter.size() > 1)
                {
                    x.Begin("TimeSignatureAutomation", { { "id", NextId() }, { "timeUnit", "beats" } });
                    x.Empty("Target", { { "parameter", meterId } });

                    for (auto const& change : m_sequence.Meter)
                    {
                        x.Empty("TimeSignaturePoint", { { "time", Beats(change.Tick) }, { "numerator", std::to_string(change.Numerator) },
                            { "denominator", std::to_string(change.Denominator) } });
                    }

                    x.End("TimeSignatureAutomation");
                }

                x.End("Arrangement");

                if (!m_sequence.Scenes.empty())
                {
                    x.Begin("Scenes", {});

                    for (size_t index = 0; index < m_sequence.Scenes.size(); ++index)
                    {
                        WriteScene(x, m_sequence.Scenes[index], index);
                    }

                    x.End("Scenes");
                }

                x.End("Project");
                return x.Take();
            }

            std::vector<uint8_t> Metadata()
            {
                XmlOut x{};
                x.Begin("MetaData", {});

                if (!m_text.Title.empty())
                {
                    x.TextElement("Title", m_text.Title);
                }

                if (!m_text.Artist.empty())
                {
                    x.TextElement("Artist", m_text.Artist);
                }

                if (!m_text.Copyright.empty())
                {
                    x.TextElement("Copyright", m_text.Copyright);
                }

                x.End("MetaData");
                return x.Take();
            }

        private:
            std::string NextId()
            {
                return std::format("id{}", m_nextId++);
            }

            void WriteTrack(_Inout_ XmlOut& x, _In_ Track const& track)
            {
                auto const id = NextId();
                m_trackIds[track.Id] = id;

                Attributes attributes{ { "id", id }, { "name", Escape(track.Name) }, { "color", HexColor(track.Color) },
                    { "contentType", track.IsFolder ? "tracks" : "notes" }, { "loaded", "true" } };

                if (!track.IsFolder && !track.Destination.Endpoint.IsEmpty() && m_text.PlayedOn)
                {
                    auto const& endpoint = track.Destination.Endpoint;
                    attributes.emplace_back("comment", Escape(m_text.PlayedOn(endpoint.Name.empty() ? endpoint.Id : endpoint.Name)));
                }

                m_result.SkippedMessages += track.Startup.size();

                x.Begin("Track", attributes);
                x.Begin("Channel", { { "id", NextId() }, { "role", track.IsFolder ? "submix" : "regular" }, { "solo", track.Soloed ? "true" : "false" }, { "audioChannels", "2" } });
                x.Empty("Mute", { { "id", NextId() }, { "name", "Mute" }, { "value", track.Muted ? "true" : "false" } });
                x.End("Channel");

                for (auto const& child : track.Children)
                {
                    WriteTrack(x, child);
                }

                x.End("Track");
            }

            void WriteTrackLanes(_Inout_ XmlOut& x, _In_ Track const& track)
            {
                x.Begin("Lanes", { { "track", m_trackIds[track.Id] }, { "id", NextId() } });

                auto const channel = track.Destination.Channel;

                if (!track.Timeline.empty())
                {
                    x.Begin("Clips", { { "id", NextId() } });

                    for (auto const& placement : track.Timeline)
                    {
                        if (auto const found = FindClip(m_sequence, placement.ClipId); found != nullptr)
                        {
                            auto const& clip = Playable(*found);
                            WriteClip(x, clip, placement.Tick, placement.Length > 0 ? placement.Length : clip.Length, channel);
                        }
                    }

                    x.End("Clips");
                }

                for (auto const& lane : track.Lanes)
                {
                    WritePoints(x, lane, channel);
                }

                m_result.SkippedLanes += Unwritable(track.Lanes);

                if (!track.Tags.empty())
                {
                    x.Begin("markers", { { "id", NextId() }, { "timeUnit", "beats" } });
                    WriteMarkers(x, track.Tags);
                    x.End("markers");
                }

                x.End("Lanes");
            }

            void WriteMarkers(_Inout_ XmlOut& x, _In_ std::vector<Tag> const& tags)
            {
                for (auto const& tag : tags)
                {
                    Attributes attributes{ { "time", Beats(tag.Tick) }, { "name", Escape(tag.Text) } };

                    if (tag.Color.has_value())
                    {
                        attributes.emplace_back("color", HexColor(*tag.Color));
                    }

                    x.Empty("Marker", attributes);
                }
            }

            Clip const& Playable(_In_ Clip const& clip)
            {
                auto const pattern = clip.Kind == ClipKind::Pattern && clip.Pattern.has_value();
                auto const generator = clip.Kind == ClipKind::Generator && clip.Generator.has_value();

                if (!pattern && !generator)
                {
                    return clip;
                }

                auto found = m_rendered.find(clip.Id);

                if (found == m_rendered.end())
                {
                    auto const meter = m_sequence.Meter.empty() ? MeterChange{} : m_sequence.Meter.front();
                    auto const ceiling = TicksPerBar(meter) * RenderedMaximumBars;
                    auto notes = generator ? MakeNotesFromGenerator(clip, RenderedGeneratorPasses, ceiling) : MakeNotesFromPattern(clip, ceiling);

                    // Counted once, under the clip's own id.
                    notes.Id = clip.Id;
                    found = m_rendered.emplace(clip.Id, std::move(notes)).first;
                }

                return found->second;
            }

            // Every placement gets its own copy: other apps haven't been seen reading a reference.
            // A track with a fixed channel plays everything on it, so that's the channel written.
            void WriteClip(_Inout_ XmlOut& x, _In_ Clip const& clip, _In_ int64_t tick, _In_ int64_t length, _In_ int8_t channel)
            {
                auto const& fromEvents = LanesFromEvents(clip);

                if (m_counted.insert(clip.Id).second)
                {
                    m_result.SkippedMessages += fromEvents.Skipped;
                    m_result.SkippedLanes += Unwritable(clip.Lanes);
                }

                Attributes attributes{ { "time", Beats(tick) }, { "duration", Beats(std::max<int64_t>(1, length)) }, { "playStart", Beats(0) } };

                if (clip.Loop)
                {
                    attributes.emplace_back("loopStart", Beats(0));
                    attributes.emplace_back("loopEnd", Beats(std::max<int64_t>(1, clip.Length)));
                }

                if (!clip.Name.empty())
                {
                    attributes.emplace_back("name", Escape(clip.Name));
                }

                if (clip.Color.has_value())
                {
                    attributes.emplace_back("color", HexColor(*clip.Color));
                }

                auto const lanes = clip.Lanes.size() - Unwritable(clip.Lanes) + fromEvents.Lanes.size();

                x.Begin("Clip", attributes);

                if (lanes > 0)
                {
                    x.Begin("Lanes", { { "id", NextId() } });
                    WriteNotes(x, clip, channel);

                    for (auto const& lane : clip.Lanes)
                    {
                        WritePoints(x, lane, channel);
                    }

                    for (auto const& lane : fromEvents.Lanes)
                    {
                        WritePoints(x, lane, channel);
                    }

                    x.End("Lanes");
                }
                else
                {
                    WriteNotes(x, clip, channel);
                }

                x.End("Clip");
            }

            void WriteNotes(_Inout_ XmlOut& x, _In_ Clip const& clip, _In_ int8_t channel)
            {
                x.Begin("Notes", { { "id", NextId() } });

                for (auto const& note : clip.Notes)
                {
                    // What's past the clip's end never plays.
                    if (note.Tick >= clip.Length)
                    {
                        continue;
                    }

                    x.Line(std::format("<Note time=\"{}\" duration=\"{}\" channel=\"{}\" key=\"{}\" vel=\"{:.6f}\" rel=\"{:.6f}\"/>",
                        Beats(note.Tick), Beats(std::max<int64_t>(1, note.Length)), (channel >= 0 ? channel : note.Channel) & 0x0F, note.Number & 0x7F,
                        static_cast<double>(note.Velocity) / 65535.0, static_cast<double>(note.ReleaseVelocity) / 65535.0));
                }

                x.End("Notes");
            }

            static char const* Expression(_In_ AutomationLane const& lane) noexcept
            {
                switch (lane.Target)
                {
                case LaneTarget::Controller: return "channelController";
                case LaneTarget::PitchBend: return "pitchBend";
                case LaneTarget::ChannelPressure: return "channelPressure";
                case LaneTarget::Program: return "programChange";
                default: return nullptr;
                }
            }

            // RPN and NRPN lanes: DAWproject has no target for them.
            static size_t Unwritable(_In_ std::vector<AutomationLane> const& lanes) noexcept
            {
                return static_cast<size_t>(std::count_if(lanes.begin(), lanes.end(), [](AutomationLane const& lane) { return Expression(lane) == nullptr; }));
            }

            struct FromEvents
            {
                std::vector<AutomationLane> Lanes{};
                size_t Skipped{ 0 };
            };

            // A recorded clip keeps its controllers, pitch bend, pressure and program changes as
            // messages. They go out as points that hold, one lane for each thing they change.
            // Anything else, such as system exclusive or a per-note message, has no place.
            FromEvents const& LanesFromEvents(_In_ Clip const& clip)
            {
                if (auto const found = m_fromEvents.find(clip.Id); found != m_fromEvents.end())
                {
                    return found->second;
                }

                FromEvents result{};
                std::map<uint32_t, size_t> laneFor{};

                for (auto const& event : clip.Events)
                {
                    // What's past the clip's end never plays.
                    if (event.Tick < 0 || event.Tick >= clip.Length)
                    {
                        continue;
                    }

                    auto const word0 = event.Words[0];
                    auto const type = word0 >> 28;
                    auto const status = (word0 >> 20) & 0x0F;
                    auto const channel = static_cast<uint8_t>((word0 >> 16) & 0x0F);

                    std::optional<LaneTarget> target{};
                    uint16_t number{ 0 };
                    uint32_t value{ 0 };

                    if (type == 0x2)
                    {
                        auto const data1 = (word0 >> 8) & 0x7F;
                        auto const data2 = word0 & 0x7F;

                        switch (status)
                        {
                        case 0xB: target = LaneTarget::Controller; number = static_cast<uint16_t>(data1); value = ScaleUp(data2, 7, 32); break;
                        case 0xE: target = LaneTarget::PitchBend; value = ScaleUp(data1 | (data2 << 7), 14, 32); break;
                        case 0xD: target = LaneTarget::ChannelPressure; value = ScaleUp(data1, 7, 32); break;
                        case 0xC: target = LaneTarget::Program; value = ScaleUp(data1, 7, 32); break;
                        default: break;
                        }
                    }
                    else if (type == 0x4 && event.WordCount >= 2)
                    {
                        auto const word1 = event.Words[1];

                        switch (status)
                        {
                        case 0xB: target = LaneTarget::Controller; number = static_cast<uint16_t>((word0 >> 8) & 0x7F); value = word1; break;
                        case 0xE: target = LaneTarget::PitchBend; value = word1; break;
                        case 0xD: target = LaneTarget::ChannelPressure; value = word1; break;
                        case 0xC: target = LaneTarget::Program; value = ScaleUp((word1 >> 24) & 0x7F, 7, 32); break;
                        default: break;
                        }
                    }

                    if (!target.has_value())
                    {
                        ++result.Skipped;
                        continue;
                    }

                    auto const key = (static_cast<uint32_t>(*target) << 16) | (static_cast<uint32_t>(channel) << 8) | number;
                    auto const [at, added] = laneFor.try_emplace(key, result.Lanes.size());

                    if (added)
                    {
                        AutomationLane lane{};
                        lane.Target = *target;
                        lane.Number = number;
                        lane.Channel = channel;
                        result.Lanes.push_back(std::move(lane));
                    }

                    result.Lanes[at->second].Points.push_back(LanePoint{ event.Tick, value, true });
                }

                return m_fromEvents.emplace(clip.Id, std::move(result)).first->second;
            }

            void WritePoints(_Inout_ XmlOut& x, _In_ AutomationLane const& lane, _In_ int8_t channel)
            {
                auto const expression = Expression(lane);

                if (expression == nullptr)
                {
                    return;
                }

                x.Begin("Points", { { "id", NextId() }, { "unit", "normalized" } });

                Attributes target{ { "expression", expression }, { "channel", std::to_string((channel >= 0 ? channel : lane.Channel) & 0x0F) } };

                if (lane.Target == LaneTarget::Controller)
                {
                    target.emplace_back("controller", std::to_string(lane.Number & 0x7F));
                }

                x.Empty("Target", target);

                // Ten places, so 32-bit values come back exactly.
                for (auto const& point : lane.Points)
                {
                    x.Line(std::format("<RealPoint time=\"{}\" value=\"{:.10f}\" interpolation=\"{}\"/>",
                        Beats(point.Tick), static_cast<double>(point.Value) / FullScale, point.Hold ? "hold" : "linear"));
                }

                x.End("Points");
            }

            void WriteScene(_Inout_ XmlOut& x, _In_ Scene const& scene, _In_ size_t index)
            {
                Attributes attributes{ { "id", NextId() } };

                if (!scene.Name.empty())
                {
                    attributes.emplace_back("name", Escape(scene.Name));
                }

                x.Begin("Scene", attributes);
                x.Begin("Lanes", { { "timeUnit", "beats" }, { "id", NextId() } });

                ForEachTrack(m_sequence, [&](Track const& track, size_t)
                {
                    if (track.IsFolder || index >= track.Slots.size() || track.Slots[index].empty())
                    {
                        return true;
                    }

                    if (auto const found = FindClip(m_sequence, track.Slots[index]); found != nullptr)
                    {
                        auto const& clip = Playable(*found);
                        x.Begin("ClipSlot", { { "track", m_trackIds[track.Id] }, { "hasStop", "true" }, { "id", NextId() } });
                        WriteClip(x, clip, 0, clip.Length, track.Destination.Channel);
                        x.End("ClipSlot");
                    }

                    return true;
                });

                x.End("Lanes");
                x.End("Scene");
            }

            Sequence const& m_sequence;
            DawProjectText const& m_text;
            DawProjectExport& m_result;
            uint64_t m_nextId{ 0 };
            std::unordered_map<std::wstring, std::string> m_trackIds{};
            std::set<std::wstring> m_counted{};
            std::unordered_map<std::wstring, Clip> m_rendered{};
            std::unordered_map<std::wstring, FromEvents> m_fromEvents{};
        };
    }

    _Use_decl_annotations_
    ImportedDawProject ImportDawProject(std::span<uint8_t const> file)
    {
        try
        {
            midiapp::ZipReader zip{};

            if (auto const opened = zip.Open(file, MaximumZipEntries); opened != midiapp::ZipStatus::Read)
            {
                return ImportedDawProject{ FromZipStatus(opened) };
            }

            return ImportFromZip(zip);
        }
        catch (std::bad_alloc const&)
        {
            return ImportedDawProject{ DawProjectStatus::TooLarge };
        }
        catch (...)
        {
            return ImportedDawProject{ DawProjectStatus::Damaged };
        }
    }

    _Use_decl_annotations_
    ImportedDawProject ImportDawProjectFile(std::filesystem::path const& path)
    {
        try
        {
            midiapp::ZipReader zip{};

            if (auto const opened = zip.OpenFile(path, MaximumZipEntries); opened != midiapp::ZipStatus::Read)
            {
                return ImportedDawProject{ FromZipStatus(opened) };
            }

            return ImportFromZip(zip);
        }
        catch (std::bad_alloc const&)
        {
            return ImportedDawProject{ DawProjectStatus::TooLarge };
        }
        catch (...)
        {
            return ImportedDawProject{ DawProjectStatus::Damaged };
        }
    }

    _Use_decl_annotations_
    DawProjectExport ExportDawProject(Sequence const& sequence, DawProjectText const& text)
    {
        DawProjectExport result{};

        try
        {
            Exporter exporter{ sequence, text, result };

            std::vector<midiapp::ZipEntry> entries{};
            entries.push_back(midiapp::ZipEntry{ L"project.xml", exporter.Project() });
            entries.push_back(midiapp::ZipEntry{ L"metadata.xml", exporter.Metadata() });

            result.Bytes = midiapp::BuildZip(entries, midiapp::ZipCompression::Deflate);
            result.Succeeded = !result.Bytes.empty();
        }
        catch (...)
        {
            result = DawProjectExport{};
        }

        return result;
    }
}
