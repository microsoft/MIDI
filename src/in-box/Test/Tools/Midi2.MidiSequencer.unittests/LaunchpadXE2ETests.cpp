// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXE2ETests.h"
#include "SequenceTestData.h"

#include "ChordTheory.h"
#include "HarmonyFollow.h"
#include "LaunchpadXProtocol.h"
#include "LaunchpadXSurface.h"
#include "PlaybackEngine.h"
#include "RecordingTake.h"
#include "RemoteControl.h"
#include "SequenceGenerator.h"
#include "SequenceModel.h"
#include "SequenceRender.h"

#include <sal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;
using namespace midisequencer::launchpad;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;
    constexpr uint64_t PlayPressedAt = 1000000;
    constexpr uint64_t MusicStartsAt = PlayPressedAt + 20000;

    uint64_t At(_In_ int64_t tick) noexcept
    {
        return MusicStartsAt + static_cast<uint64_t>(std::llround(static_cast<double>(tick) * 500000.0 / TicksPerQuarterNote));
    }

    constexpr uint32_t Midi1Word(
        _In_ uint8_t group,
        _In_ uint8_t status,
        _In_ uint8_t channel,
        _In_ uint8_t data1,
        _In_ uint8_t data2) noexcept
    {
        return 0x20000000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (static_cast<uint32_t>(status & 0x0Fu) << 20) |
            (static_cast<uint32_t>(channel & 0x0Fu) << 16) |
            (static_cast<uint32_t>(data1 & 0x7Fu) << 8) |
            static_cast<uint32_t>(data2 & 0x7Fu);
    }

    constexpr std::array<uint32_t, 2> Midi2NoteOnWords(
        _In_ uint8_t group,
        _In_ uint8_t channel,
        _In_ uint8_t note,
        _In_ uint16_t velocity16) noexcept
    {
        uint32_t const w0 = 0x40900000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (static_cast<uint32_t>(channel & 0x0Fu) << 16) |
            (static_cast<uint32_t>(note & 0x7Fu) << 8);
        uint32_t const w1 = static_cast<uint32_t>(velocity16) << 16;
        return { w0, w1 };
    }

    constexpr std::array<uint32_t, 2> Midi2NoteOffWords(
        _In_ uint8_t group,
        _In_ uint8_t channel,
        _In_ uint8_t note,
        _In_ uint16_t velocity16) noexcept
    {
        uint32_t const w0 = 0x40800000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (static_cast<uint32_t>(channel & 0x0Fu) << 16) |
            (static_cast<uint32_t>(note & 0x7Fu) << 8);
        uint32_t const w1 = static_cast<uint32_t>(velocity16) << 16;
        return { w0, w1 };
    }

    constexpr std::array<uint32_t, 2> Midi2PolyAftertouchWords(
        _In_ uint8_t group,
        _In_ uint8_t channel,
        _In_ uint8_t note,
        _In_ uint32_t pressure32) noexcept
    {
        uint32_t const w0 = 0x40A00000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (static_cast<uint32_t>(channel & 0x0Fu) << 16) |
            (static_cast<uint32_t>(note & 0x7Fu) << 8);
        return { w0, pressure32 };
    }

    constexpr std::array<uint32_t, 2> Midi2CcWords(
        _In_ uint8_t group,
        _In_ uint8_t channel,
        _In_ uint8_t cc,
        _In_ uint32_t value32) noexcept
    {
        uint32_t const w0 = 0x40B00000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (static_cast<uint32_t>(channel & 0x0Fu) << 16) |
            (static_cast<uint32_t>(cc & 0x7Fu) << 8);
        return { w0, value32 };
    }

    struct SentMessage
    {
        std::wstring Endpoint{};
        uint64_t Timestamp{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t Count{ 0 };
    };

    class TestEngineOutput final : public IEngineOutput
    {
    public:
        void Send(
            _In_ EndpointRef const& endpoint,
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept override
        {
            try
            {
                SentMessage message{ endpoint.Name, timestamp, {}, wordCount };
                std::copy_n(words, std::min<uint8_t>(wordCount, 4), message.Words.begin());

                std::scoped_lock guard{ m_lock };
                m_sent.push_back(std::move(message));
            }
            catch (...)
            {
            }
        }

        std::vector<SentMessage> All()
        {
            std::scoped_lock guard{ m_lock };
            return m_sent;
        }

        void Clear()
        {
            std::scoped_lock guard{ m_lock };
            m_sent.clear();
        }

    private:
        std::mutex m_lock{};
        std::vector<SentMessage> m_sent{};
    };

    struct EngineFixture
    {
        uint64_t Now{ PlayPressedAt };
        TestEngineOutput Output{};
        PlaybackEngine Engine{ Output, EngineClock{ [this]() { return Now; }, 1000000 } };
    };

    void RunUntil(_Inout_ EngineFixture& fixture, _In_ uint64_t endTime)
    {
        for (; fixture.Now < endTime; fixture.Now += 5000)
        {
            fixture.Engine.Sweep();
        }
    }

    std::vector<SentMessage> NoteOns(_In_ std::vector<SentMessage> const& messages)
    {
        std::vector<SentMessage> result{};
        for (auto const& message : messages)
        {
            auto const type = message.Words[0] >> 28;
            auto const status = (message.Words[0] >> 20) & 0x0Fu;
            if ((type == 0x4u || type == 0x2u) && status == 0x9u)
            {
                result.push_back(message);
            }
        }

        return result;
    }

    uint8_t NoteNumberFromMessage(_In_ SentMessage const& message) noexcept
    {
        return static_cast<uint8_t>((message.Words[0] >> 8) & 0x7Fu);
    }

    class TestLaunchpadOutput
    {
    public:
        bool SendUmpPackets(_In_ std::span<UmpPacket const> packets) noexcept
        {
            if (packets.empty())
            {
                return true;
            }

            if (!m_shouldSucceed)
            {
                return false;
            }

            try
            {
                std::scoped_lock guard{ m_lock };
                m_packets.insert(m_packets.end(), packets.begin(), packets.end());
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        void SetShouldSucceed(_In_ bool succeed) noexcept
        {
            m_shouldSucceed = succeed;
        }

        std::vector<UmpPacket> All()
        {
            std::scoped_lock guard{ m_lock };
            return m_packets;
        }

        void Clear()
        {
            std::scoped_lock guard{ m_lock };
            m_packets.clear();
        }

    private:
        std::mutex m_lock{};
        std::vector<UmpPacket> m_packets{};
        bool m_shouldSucceed{ true };
    };

    std::vector<std::vector<uint8_t>> DepacketizeSysEx7Messages(_In_ std::span<UmpPacket const> packets)
    {
        std::vector<std::vector<uint8_t>> messages{};
        std::vector<uint8_t> current{};
        bool inMessage{ false };

        for (auto const& packet : packets)
        {
            if (packet.wordCount != 2)
            {
                continue;
            }

            uint32_t const w0 = packet.words[0];
            uint32_t const w1 = packet.words[1];
            uint8_t const mt = static_cast<uint8_t>((w0 >> 28) & 0x0Fu);
            if (mt != 0x3u)
            {
                continue;
            }

            uint8_t const status = static_cast<uint8_t>((w0 >> 20) & 0x0Fu);
            uint8_t const count = static_cast<uint8_t>((w0 >> 16) & 0x0Fu);
            VERIFY_IS_TRUE(count <= 6);

            std::array<uint8_t, 6> const bytes{
                static_cast<uint8_t>((w0 >> 8) & 0xFFu),
                static_cast<uint8_t>(w0 & 0xFFu),
                static_cast<uint8_t>((w1 >> 24) & 0xFFu),
                static_cast<uint8_t>((w1 >> 16) & 0xFFu),
                static_cast<uint8_t>((w1 >> 8) & 0xFFu),
                static_cast<uint8_t>(w1 & 0xFFu),
            };

            if (status == 0)
            {
                current.assign(bytes.begin(), bytes.begin() + count);
                messages.push_back(std::move(current));
                current.clear();
                inMessage = false;
            }
            else if (status == 1)
            {
                current.assign(bytes.begin(), bytes.begin() + count);
                inMessage = true;
            }
            else if (status == 2 && inMessage)
            {
                current.insert(current.end(), bytes.begin(), bytes.begin() + count);
            }
            else if (status == 3 && inMessage)
            {
                current.insert(current.end(), bytes.begin(), bytes.begin() + count);
                messages.push_back(std::move(current));
                current.clear();
                inMessage = false;
            }
        }

        return messages;
    }

    bool ContainsSysExSequence(
        _In_ std::vector<std::vector<uint8_t>> const& messages,
        _In_ std::span<uint8_t const> expectedPayload)
    {
        return std::any_of(messages.begin(), messages.end(), [expectedPayload](std::vector<uint8_t> const& msg)
        {
            return msg.size() == expectedPayload.size() &&
                std::equal(msg.begin(), msg.end(), expectedPayload.begin());
        });
    }

    std::vector<UmpPacket> PacketizeFramedSysEx(
        _In_ uint8_t group,
        _In_ std::span<uint8_t const> framedSysEx)
    {
        if (framedSysEx.size() < 2 || framedSysEx.front() != 0xF0u || framedSysEx.back() != 0xF7u)
        {
            return {};
        }

        return PacketizeSysEx7(group, framedSysEx.subspan(1, framedSysEx.size() - 2));
    }

    InputEvent ParseSingleWord(_In_ uint32_t word)
    {
        InputEvent ev{};
        VERIFY_IS_TRUE(TryParseInputUmp(&word, 1, ev));
        return ev;
    }

    InputEvent ParseTwoWords(_In_ std::array<uint32_t, 2> const& words)
    {
        InputEvent ev{};
        VERIFY_IS_TRUE(TryParseInputUmp(words.data(), 2, ev));
        return ev;
    }

    bool TryParseSysExPackets(
        _In_ std::span<UmpPacket const> packets,
        _Out_ InputEvent& outEvent,
        _Inout_ SysEx7Accumulator& accumulator) noexcept
    {
        outEvent = InputEvent{};
        bool parsed{ false };
        for (auto const& packet : packets)
        {
            if (TryParseInputUmp(packet.words, packet.wordCount, outEvent, &accumulator))
            {
                parsed = true;
            }
        }
        return parsed;
    }

    LedSpec const* FindLedSpec(_In_ std::vector<LedSpec> const& specs, _In_ uint8_t index) noexcept
    {
        for (auto const& spec : specs)
        {
            if (spec.index == index)
            {
                return &spec;
            }
        }

        return nullptr;
    }

    bool LedSpecsEqual(_In_ LedSpec const& a, _In_ LedSpec const& b) noexcept
    {
        return a.index == b.index &&
            a.behavior == b.behavior &&
            a.colorA == b.colorA &&
            a.colorB == b.colorB &&
            a.rgb.r == b.rgb.r &&
            a.rgb.g == b.rgb.g &&
            a.rgb.b == b.rgb.b;
    }

    void VerifyAll81LedIndicesPresent(_In_ std::vector<LedSpec> const& specs)
    {
        VERIFY_ARE_EQUAL(size_t{ 81 }, specs.size());

        std::set<uint8_t> seen{};
        for (auto const& spec : specs)
        {
            VERIFY_IS_TRUE(seen.insert(spec.index).second);
        }

        for (uint8_t row = 1; row <= 8; ++row)
        {
            for (uint8_t col = 1; col <= 8; ++col)
            {
                uint8_t const pad = static_cast<uint8_t>(row * 10 + col);
                VERIFY_IS_TRUE(seen.contains(pad));
            }
        }

        for (uint8_t topDown = 0; topDown < 8; ++topDown)
        {
            VERIFY_IS_TRUE(seen.contains(SceneButtonIndex(topDown)));
        }

        for (uint8_t top = ButtonUp; top <= ButtonRecord; ++top)
        {
            VERIFY_IS_TRUE(seen.contains(top));
        }

        VERIFY_IS_TRUE(seen.contains(LogoLed));
    }

    Sequence BuildGridSequence(
        _In_ uint32_t trackCount,
        _In_ uint32_t sceneCount,
        _In_ bool includeFolders)
    {
        Sequence sequence{};
        sequence.Name = L"Launchpad Grid";

        for (uint32_t s = 0; s < sceneCount; ++s)
        {
            sequence.Scenes.push_back(Scene{ L"s-" + std::to_wstring(s), L"Scene " + std::to_wstring(s + 1), nullptr });
        }

        constexpr std::array<uint32_t, 4> trackColors{ 0xFF8A65u, 0x4FC3F7u, 0x81C784u, 0xBA68C8u };
        uint32_t created = 0;

        auto makePlayableTrack = [&](uint32_t idx)
        {
            Track track{};
            track.Id = L"t-" + std::to_wstring(idx);
            track.Name = L"Track " + std::to_wstring(idx + 1);
            track.Color = trackColors[idx % trackColors.size()];
            track.Destination.Endpoint = EndpointRef{ L"Synth", L"synth-id" };
            track.Destination.Group = 0;
            track.Destination.Channel = static_cast<int8_t>(idx % 16);
            track.Slots.resize(sceneCount);

            for (uint32_t s = 0; s < sceneCount; ++s)
            {
                std::wstring const clipId = L"c-" + std::to_wstring(idx) + L"-" + std::to_wstring(s);
                Clip clip{};
                clip.Id = clipId;
                clip.Name = L"Clip " + std::to_wstring(idx) + L"-" + std::to_wstring(s);
                clip.Length = Bar;
                clip.Loop = true;
                clip.Notes = { testdata::MakeNote(0, 480, static_cast<uint8_t>(48 + ((idx + s) % 24))) };
                sequence.Clips.push_back(std::move(clip));
                track.Slots[s] = clipId;
            }

            return track;
        };

        if (includeFolders && trackCount >= 4)
        {
            Track pinned = makePlayableTrack(created++);
            pinned.Pinned = true;
            sequence.Tracks.push_back(std::move(pinned));

            Track folder1{};
            folder1.Id = L"f-1";
            folder1.Name = L"Synths";
            folder1.IsFolder = true;
            folder1.Children.push_back(makePlayableTrack(created++));
            folder1.Children.push_back(makePlayableTrack(created++));
            sequence.Tracks.push_back(std::move(folder1));

            Track folder2{};
            folder2.Id = L"f-2";
            folder2.Name = L"Layers";
            folder2.IsFolder = true;
            folder2.Children.push_back(makePlayableTrack(created++));
            if (created < trackCount)
            {
                folder2.Children.push_back(makePlayableTrack(created++));
            }
            sequence.Tracks.push_back(std::move(folder2));
        }

        while (created < trackCount)
        {
            sequence.Tracks.push_back(makePlayableTrack(created++));
        }

        NormalizeSequence(sequence);
        return sequence;
    }

    Track const* FindTrackById(_In_ Sequence const& sequence, _In_ std::wstring_view trackId) noexcept
    {
        return FindTrack(sequence, trackId);
    }

    void DispatchClipActionsToEngine(
        _In_ Sequence const& sequence,
        _In_ std::vector<std::wstring> const& trackOrder,
        _In_ std::vector<SurfaceAction> const& actions,
        _Inout_ LaunchpadXSurface& surface,
        _Inout_ PlaybackEngine& engine,
        _In_ LaunchQuantize quantize,
        _Inout_ bool& recordingActive)
    {
        for (auto const& action : actions)
        {
            switch (action.kind)
            {
            case SurfaceActionKind::TriggerOrStopSlot:
            {
                if (action.trackIndex >= trackOrder.size())
                {
                    break;
                }

                std::wstring const& trackId = trackOrder[action.trackIndex];
                Track const* track = FindTrackById(sequence, trackId);
                if (!track || action.sceneIndex >= track->Slots.size())
                {
                    break;
                }

                std::wstring const& clipId = track->Slots[action.sceneIndex];
                auto const views = engine.LaunchState();
                auto const viewIt = std::find_if(views.begin(), views.end(), [&trackId](TrackLaunchView const& v)
                {
                    return v.TrackId == trackId;
                });

                bool const isPlayingThisSlot = (viewIt != views.end()) &&
                    viewIt->Plays(clipId, action.sceneIndex) &&
                    !viewIt->Pending;

                if (clipId.empty() || isPlayingThisSlot)
                {
                    engine.StopTrack(trackId, quantize);
                }
                else
                {
                    engine.LaunchClip(trackId, clipId, quantize, static_cast<int32_t>(action.sceneIndex));
                }
                break;
            }
            case SurfaceActionKind::LaunchScene:
            {
                std::vector<PlaybackEngine::SceneLaunch> launches{};
                for (auto const& trackId : trackOrder)
                {
                    Track const* track = FindTrackById(sequence, trackId);
                    std::wstring clipId{};
                    if (track && action.sceneIndex < track->Slots.size())
                    {
                        clipId = track->Slots[action.sceneIndex];
                    }
                    launches.push_back(PlaybackEngine::SceneLaunch{ trackId, std::move(clipId) });
                }

                engine.LaunchScene(launches, quantize, static_cast<int32_t>(action.sceneIndex));
                break;
            }
            case SurfaceActionKind::StopAll:
                engine.Stop();
                recordingActive = false;
                break;
            case SurfaceActionKind::ToggleRecord:
                recordingActive = !recordingActive;
                break;
            case SurfaceActionKind::SetViewport:
                surface.SetViewport(action.trackIndex, action.sceneIndex);
                break;
            default:
                break;
            }
        }
    }
}

void LaunchpadXE2ETests::T1_F1_ProtocolAndSysEx7Codec()
{
    Log::Comment(L"T1-F1-01..06: Protocol SysEx7, Device Inquiry, Bulk LED 03, Channel Voice LEDs, Input UMP 0x2/0x4");

    // T1-F1-01: Enter & Exit Programmer Mode
    {
        auto const enterPayload = BuildProgrammerModeSysEx(true);
        auto const exitPayload = BuildProgrammerModeSysEx(false);
        auto const enterPackets = PacketizeSysEx7(0, enterPayload);
        auto const exitPackets = PacketizeSysEx7(0, exitPayload);

        auto const enterMsgs = DepacketizeSysEx7Messages(enterPackets);
        auto const exitMsgs = DepacketizeSysEx7Messages(exitPackets);
        VERIFY_ARE_EQUAL(size_t{ 1 }, enterMsgs.size());
        VERIFY_ARE_EQUAL(size_t{ 1 }, exitMsgs.size());

        std::array<uint8_t, 7> const expectedEnter{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x01u };
        std::array<uint8_t, 7> const expectedExit{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x00u };
        VERIFY_IS_TRUE(ContainsSysExSequence(enterMsgs, expectedEnter));
        VERIFY_IS_TRUE(ContainsSysExSequence(exitMsgs, expectedExit));
    }

    // T1-F1-02: Select Programmer Layout (00 7F)
    {
        std::array<uint8_t, 7> const layoutPayload{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x00u, 0x7Fu };
        auto const packets = PacketizeSysEx7(0, layoutPayload);
        VERIFY_ARE_EQUAL(size_t{ 2 }, packets.size());
        VERIFY_ARE_EQUAL(uint32_t{ 0x1u }, (packets[0].words[0] >> 20) & 0x0Fu);
        VERIFY_ARE_EQUAL(uint32_t{ 0x6u }, (packets[0].words[0] >> 16) & 0x0Fu);
        VERIFY_ARE_EQUAL(uint32_t{ 0x3u }, (packets[1].words[0] >> 20) & 0x0Fu);
        VERIFY_ARE_EQUAL(uint32_t{ 0x1u }, (packets[1].words[0] >> 16) & 0x0Fu);

        auto const unpacked = DepacketizeSysEx7Messages(packets);
        VERIFY_ARE_EQUAL(size_t{ 1 }, unpacked.size());
        VERIFY_IS_TRUE(ContainsSysExSequence(unpacked, layoutPayload));
    }

    // T1-F1-03: Device Inquiry Request & App-mode Reply
    {
        auto const reqPackets = BuildDeviceInquiryPackets(0);
        auto const reqMsgs = DepacketizeSysEx7Messages(reqPackets);
        VERIFY_ARE_EQUAL(size_t{ 1 }, reqMsgs.size());
        std::array<uint8_t, 4> const expectedInquiry{ 0x7Eu, 0x7Fu, 0x06u, 0x01u };
        VERIFY_IS_TRUE(ContainsSysExSequence(reqMsgs, expectedInquiry));

        std::array<uint8_t, 17> const appReply{
            0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x20u, 0x29u,
            0x13u, 0x01u, 0x00u, 0x00u, 0x30u, 0x33u, 0x35u, 0x31u, 0xF7u
        };
        auto const replyPackets = PacketizeFramedSysEx(0, appReply);
        VERIFY_IS_TRUE(replyPackets.size() >= 2);

        InputEvent ev{};
        SysEx7Accumulator acc{};
        bool const parsed = TryParseSysExPackets(replyPackets, ev, acc);

        VERIFY_IS_TRUE(parsed);
        VERIFY_IS_TRUE(ev.kind == InputEventKind::DeviceInquiryReply);
        VERIFY_IS_FALSE(ev.isBootloader);
        VERIFY_ARE_EQUAL(uint8_t{ '0' }, ev.version[0]);
        VERIFY_ARE_EQUAL(uint8_t{ '3' }, ev.version[1]);
        VERIFY_ARE_EQUAL(uint8_t{ '5' }, ev.version[2]);
        VERIFY_ARE_EQUAL(uint8_t{ '1' }, ev.version[3]);
    }

    // T1-F1-04: Bulk LED SysEx 03 across Static / Flashing / Pulsing / RGB
    {
        RgbColor const rgb = RgbColor::FromRgb24(0xFF8A65u);
        VERIFY_ARE_EQUAL(uint8_t{ 0x7Fu }, rgb.r);
        VERIFY_ARE_EQUAL(uint8_t{ 0x45u }, rgb.g);
        VERIFY_ARE_EQUAL(uint8_t{ 0x32u }, rgb.b);

        std::array<LedSpec, 4> const specs{ {
            LedSpec{ 11, LedBehavior::StaticPalette, 21, 0, {} },
            LedSpec{ 12, LedBehavior::FlashingPalette, 5, 9, {} },
            LedSpec{ 13, LedBehavior::PulsingPalette, 45, 0, {} },
            LedSpec{ 14, LedBehavior::Rgb, 0, 0, rgb },
        } };

        auto const packets = BuildLedSysExPackets(0, specs);
        auto const msgs = DepacketizeSysEx7Messages(packets);
        VERIFY_ARE_EQUAL(size_t{ 1 }, msgs.size());

        auto const& payload = msgs[0];
        // Header (6 bytes) + 3 + 4 + 3 + 5 = 21 bytes
        VERIFY_ARE_EQUAL(size_t{ 21 }, payload.size());
        VERIFY_ARE_EQUAL(uint8_t{ 0x00u }, payload[0]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x20u }, payload[1]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x29u }, payload[2]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x02u }, payload[3]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x0Cu }, payload[4]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x03u }, payload[5]);

        for (uint8_t byte : payload)
        {
            VERIFY_IS_TRUE(byte <= 0x7Fu);
        }

        // Verify RGB entry at the tail (14, 0x7F, 0x45, 0x32)
        VERIFY_ARE_EQUAL(uint8_t{ 0x03u }, payload[16]);
        VERIFY_ARE_EQUAL(uint8_t{ 14u }, payload[17]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x7Fu }, payload[18]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x45u }, payload[19]);
        VERIFY_ARE_EQUAL(uint8_t{ 0x32u }, payload[20]);
    }

    // T1-F1-05: Channel Voice LED packet (0x2)
    {
        auto const padPacket = BuildChannelLedPacket(0, 11, true, LedBehavior::StaticPalette, 21);
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, padPacket.wordCount);
        VERIFY_ARE_EQUAL(uint32_t{ 0x20900B15u }, padPacket.words[0]);

        auto const ccPacket = BuildChannelLedPacket(0, 89, false, LedBehavior::PulsingPalette, 5);
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, ccPacket.wordCount);
        VERIFY_ARE_EQUAL(uint32_t{ 0x20B25905u }, ccPacket.words[0]);
    }

    // T1-F1-06: Type 0x2 and Type 0x4 input UMP parsing
    {
        auto const ev1Press = ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100));
        auto const ev1Rel = ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0));
        auto const ev1At = ParseSingleWord(Midi1Word(0, 0xA, 0, 11, 64));
        auto const ev1Btn = ParseSingleWord(Midi1Word(0, 0xB, 0, 95, 127));

        VERIFY_IS_TRUE(ev1Press.kind == InputEventKind::PadPress);
        VERIFY_ARE_EQUAL(uint8_t{ 11 }, ev1Press.index);
        VERIFY_ARE_EQUAL(uint8_t{ 100 }, ev1Press.value);
        VERIFY_IS_TRUE(ev1Rel.kind == InputEventKind::PadRelease);
        VERIFY_ARE_EQUAL(uint8_t{ 11 }, ev1Rel.index);
        VERIFY_IS_TRUE(ev1At.kind == InputEventKind::PadAftertouch);
        VERIFY_ARE_EQUAL(uint8_t{ 64 }, ev1At.value);
        VERIFY_IS_TRUE(ev1Btn.kind == InputEventKind::ButtonPress);
        VERIFY_ARE_EQUAL(uint8_t{ 95 }, ev1Btn.index);

        auto const ev2Press = ParseTwoWords(Midi2NoteOnWords(0, 0, 11, static_cast<uint16_t>(ScaleUp(100, 7, 16))));
        auto const ev2Rel = ParseTwoWords(Midi2NoteOffWords(0, 0, 11, 0));
        auto const ev2At = ParseTwoWords(Midi2PolyAftertouchWords(0, 0, 11, ScaleUp(64, 7, 32)));
        auto const ev2Btn = ParseTwoWords(Midi2CcWords(0, 0, 95, 0xFFFFFFFFu));

        VERIFY_IS_TRUE(ev2Press.kind == InputEventKind::PadPress);
        VERIFY_ARE_EQUAL(uint8_t{ 11 }, ev2Press.index);
        VERIFY_ARE_EQUAL(uint8_t{ 100 }, ev2Press.value);
        VERIFY_IS_TRUE(ev2Rel.kind == InputEventKind::PadRelease);
        VERIFY_IS_TRUE(ev2At.kind == InputEventKind::PadAftertouch);
        VERIFY_ARE_EQUAL(uint8_t{ 64 }, ev2At.value);
        VERIFY_IS_TRUE(ev2Btn.kind == InputEventKind::ButtonPress);
        VERIFY_ARE_EQUAL(uint8_t{ 95 }, ev2Btn.index);
    }
}

void LaunchpadXE2ETests::T1_F2_ProgrammerModeGridAndButtons()
{
    Log::Comment(L"T1-F2-01..05: Programmer Mode 8x8 Grid, Scene Buttons 89..19, Top Row 91..98, Logo LED 99");

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};

    // T1-F2-01: Bottom-up 8x8 pad coordinate mapping via Note mode & Clip mode
    {
        std::vector<SurfaceAction> releases{};
        surface.SetMode(ControllerMode::Note, releases);
        surface.SetNoteConfig(NoteLayoutConfig{ 0, midiapp::ScaleKind::Major, 3, false, 5 }, releases);

        bool isRoot{ false };
        bool inScale{ false };
        auto const bl = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
        auto const br = surface.MapPadToMidiNote(0, 7, isRoot, inScale);
        auto const tl = surface.MapPadToMidiNote(7, 0, isRoot, inScale);
        auto const tr = surface.MapPadToMidiNote(7, 7, isRoot, inScale);
        VERIFY_IS_TRUE(bl.has_value() && br.has_value() && tl.has_value() && tr.has_value());

        // Press 11 (0,0), 18 (0,7), 81 (7,0), 88 (7,7) and verify exact mapping matches bottom-up grid
        SurfaceStateSnapshot snap{};
        std::array<std::pair<uint8_t, uint8_t>, 4> const corners{ {
            { uint8_t{ 11 }, *bl }, { uint8_t{ 18 }, *br }, { uint8_t{ 81 }, *tl }, { uint8_t{ 88 }, *tr }
        } };

        for (auto const& [pad, expectedNote] : corners)
        {
            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, pad, 90)), snap, actions);
            VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
            VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOn);
            VERIFY_ARE_EQUAL(expectedNote, actions[0].midiNote);

            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, pad, 0)), snap, actions);
            VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
            VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOff);
            VERIFY_ARE_EQUAL(expectedNote, actions[0].midiNote);
        }
    }

    // T1-F2-02: Right-column Scene buttons 89..19 top-to-bottom
    {
        VERIFY_ARE_EQUAL(uint8_t{ 89 }, SceneButtonIndex(0));
        VERIFY_ARE_EQUAL(uint8_t{ 19 }, SceneButtonIndex(7));

        std::vector<SurfaceAction> releases{};
        surface.SetMode(ControllerMode::Clip, releases);

        auto const doc = BuildGridSequence(8, 8, false);
        auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});

        for (uint8_t row = 0; row < 8; ++row)
        {
            uint8_t const cc = SceneButtonIndex(row);
            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, cc, 127)), snap, actions);
            VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
            VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::LaunchScene);
            VERIFY_ARE_EQUAL(static_cast<uint32_t>(row), actions[0].sceneIndex);

            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, cc, 0)), snap, actions);
            VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());
        }
    }

    // T1-F2-03 & T1-F2-04: Top-row Navigation (91..94) & Mode/Transport (95..98)
    {
        auto const doc = BuildGridSequence(10, 10, false);
        auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});

        EngineFixture dummyFixture{};
        bool recFlag{ false };

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonRight, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, RemoteTrackOrder(doc), actions, surface, dummyFixture.Engine, 0, recFlag);
        VERIFY_IS_TRUE(surface.TrackOffset() > 0);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonDown, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, RemoteTrackOrder(doc), actions, surface, dummyFixture.Engine, 0, recFlag);
        VERIFY_IS_TRUE(surface.SceneOffset() > 0);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonLeft, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, RemoteTrackOrder(doc), actions, surface, dummyFixture.Engine, 0, recFlag);
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.TrackOffset());

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonUp, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, RemoteTrackOrder(doc), actions, surface, dummyFixture.Engine, 0, recFlag);
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.SceneOffset());

        // 97 Stop, 98 Record, 96 Note, 95 Session
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonCustom, 127)), snap, actions);
        VERIFY_IS_TRUE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a) { return a.kind == SurfaceActionKind::StopAll; }));

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonRecord, 127)), snap, actions);
        VERIFY_IS_TRUE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a) { return a.kind == SurfaceActionKind::ToggleRecord; }));

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
        VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Note);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snap, actions);
        VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);
    }

    // T1-F2-05: Logo LED 99 included in 81-LED refresh with distinct Clip vs Note state
    {
        auto const doc = BuildGridSequence(2, 2, false);
        auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});

        std::vector<SurfaceAction> releases{};
        surface.SetMode(ControllerMode::Clip, releases);
        auto const clipLeds = surface.BuildLedFeedback(snap, true);
        VerifyAll81LedIndicesPresent(clipLeds);
        LedSpec const* clipLogo = FindLedSpec(clipLeds, LogoLed);
        VERIFY_IS_TRUE(clipLogo != nullptr);

        surface.SetMode(ControllerMode::Note, releases);
        auto const noteLeds = surface.BuildLedFeedback(snap, true);
        VerifyAll81LedIndicesPresent(noteLeds);
        LedSpec const* noteLogo = FindLedSpec(noteLeds, LogoLed);
        VERIFY_IS_TRUE(noteLogo != nullptr);
        VERIFY_IS_FALSE(LedSpecsEqual(*clipLogo, *noteLogo));
    }
}

void LaunchpadXE2ETests::T1_F3_ClipModeSurfaceAndNavigation()
{
    Log::Comment(L"T1-F3-01..07: Clip Mode Launch, Stop/Toggle, Scene Launch, Scroll, Stop/Record LEDs, Differential Refresh, RemoteTrackOrder");

    // T1-F3-01 & T1-F3-02: Clip launch and stop/toggle
    {
        auto doc = BuildGridSequence(2, 2, false);
        // Make slot (0, 1) empty
        doc.Tracks[0].Slots[1].clear();
        NormalizeSequence(doc);

        EngineFixture f{};
        f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
        f.Engine.Play(0);
        RunUntil(f, At(240));

        LaunchpadXSurface surface{};
        auto const order = RemoteTrackOrder(doc);
        bool recording{ false };
        std::vector<SurfaceAction> actions{};

        // Pad 81 (top row = scene 0, col 1 = track 0)
        auto snap0 = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 120)), snap0, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::TriggerOrStopSlot);
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, actions[0].trackIndex);
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, actions[0].sceneIndex);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        RunUntil(f, At(480));

        // Pad 72 (row 1 = scene 1, col 2 = track 1)
        actions.clear();
        auto snap1 = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 72, 120)), snap1, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::TriggerOrStopSlot);
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, actions[0].trackIndex);
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, actions[0].sceneIndex);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        RunUntil(f, At(720));

        auto const playingViews = f.Engine.LaunchState();
        VERIFY_IS_TRUE(playingViews[0].Plays(doc.Tracks[0].Slots[0], 0));
        VERIFY_IS_TRUE(playingViews[1].Plays(doc.Tracks[1].Slots[1], 1));

        // T1-F3-02: Press playing slot 81 again to stop track 0
        actions.clear();
        auto snap2 = BuildSurfaceSnapshot(doc, playingViews, L"t-0", 0, true, false, false, L"", 0, {});
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 120)), snap2, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        RunUntil(f, At(960));
        VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Mode == TrackPlayMode::Stopped);
    }

    // T1-F3-03..06: Scene launch, viewport nav, Stop/Record LEDs, Differential LED states
    {
        auto doc = BuildGridSequence(10, 10, false);
        LaunchpadXSurface surface{};
        std::vector<TrackLaunchView> views(10);
        for (uint32_t i = 0; i < 10; ++i)
        {
            views[i].TrackId = doc.Tracks[i].Id;
        }
        // Track 0 playing slot 0, Track 1 pending slot 1, Track 2 recording slot 2, Track 3 empty slot 0
        views[0].Mode = TrackPlayMode::Clip;
        views[0].ClipId = doc.Tracks[0].Slots[0];
        views[0].Slot = 0;
        views[1].Pending = true;
        views[1].PendingMode = TrackPlayMode::Clip;
        views[1].PendingClipId = doc.Tracks[1].Slots[1];
        views[1].PendingSlot = 1;
        doc.Tracks[3].Slots[0].clear();

        auto snap = BuildSurfaceSnapshot(doc, views, L"t-2", 2, true, true, true, L"t-2", 2, {});
        auto const fullLeds = surface.BuildLedFeedback(snap, true);
        VerifyAll81LedIndicesPresent(fullLeds);

        // Unchanged snapshot produces empty differential update
        auto const emptyDiff = surface.BuildLedFeedback(snap, false);
        VERIFY_ARE_EQUAL(size_t{ 0 }, emptyDiff.size());

        // Distinct states: playing pad 81, pending pad 72, recording pad 63, empty pad 84, filled idle pad 82
        LedSpec const* playingLed = FindLedSpec(fullLeds, 81);
        LedSpec const* pendingLed = FindLedSpec(fullLeds, 72);
        LedSpec const* recordingLed = FindLedSpec(fullLeds, 63);
        LedSpec const* emptyLed = FindLedSpec(fullLeds, 84);
        LedSpec const* idleLed = FindLedSpec(fullLeds, 82);
        VERIFY_IS_TRUE(playingLed && pendingLed && recordingLed && emptyLed && idleLed);
        VERIFY_IS_FALSE(LedSpecsEqual(*playingLed, *idleLed));
        VERIFY_IS_FALSE(LedSpecsEqual(*pendingLed, *idleLed));
        VERIFY_IS_FALSE(LedSpecsEqual(*recordingLed, *idleLed));
        VERIFY_IS_FALSE(LedSpecsEqual(*emptyLed, *idleLed));

        // Mutate 1 slot -> minimal diff
        views[0].Mode = TrackPlayMode::Stopped;
        views[0].ClipId.clear();
        views[0].Slot = -1;
        auto snapMutated = BuildSurfaceSnapshot(doc, views, L"t-2", 2, true, true, true, L"t-2", 2, {});
        auto const diff = surface.BuildLedFeedback(snapMutated, false);
        VERIFY_IS_TRUE(!diff.empty() && diff.size() <= 4);
        VERIFY_IS_TRUE(FindLedSpec(diff, 81) != nullptr);

        // T1-F3-04 & T1-F3-05: Nav LEDs change on scroll (0,0) -> (1,1), and 97/98 LEDs reflect transport state
        surface.SetViewport(1, 1);
        auto const scrolledLeds = surface.BuildLedFeedback(snapMutated, true);
        VerifyAll81LedIndicesPresent(scrolledLeds);
        LedSpec const* upAtOrigin = FindLedSpec(fullLeds, ButtonUp);
        LedSpec const* upScrolled = FindLedSpec(scrolledLeds, ButtonUp);
        VERIFY_IS_TRUE(upAtOrigin && upScrolled);
        VERIFY_IS_FALSE(LedSpecsEqual(*upAtOrigin, *upScrolled));

        std::vector<SurfaceAction> navActions{};
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonCustom, 127)), snapMutated, navActions);
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonRecord, 127)), snapMutated, navActions);
        VERIFY_IS_TRUE(std::any_of(navActions.begin(), navActions.end(), [](SurfaceAction const& a) { return a.kind == SurfaceActionKind::StopAll; }));
        VERIFY_IS_TRUE(std::any_of(navActions.begin(), navActions.end(), [](SurfaceAction const& a) { return a.kind == SurfaceActionKind::ToggleRecord; }));
    }

    // T1-F3-07: BuildSurfaceSnapshot excludes folders via RemoteTrackOrder
    {
        auto const sample = testdata::SampleSequence();
        auto const order = RemoteTrackOrder(sample);
        auto const snap = BuildSurfaceSnapshot(sample, {}, order.front(), 0, false, false, false, L"", 0, {});
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(order.size()), snap.totalTracks);
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(sample.Scenes.size()), snap.totalScenes);
    }
}

void LaunchpadXE2ETests::T1_F4_NoteModeScaleLayout()
{
    Log::Comment(L"T1-F4-01..06: Chromatic Isomorphic Layout, InScaleOnly, Press/Aftertouch/Release, LEDs, Octave Shift, Unison");

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);

    // T1-F4-01: Chromatic isomorphic +5 semitones/row
    {
        NoteLayoutConfig cfg{ 0, midiapp::ScaleKind::Major, 3, false, 5 };
        surface.SetNoteConfig(cfg, actions);
        bool isRoot{ false };
        bool inScale{ false };

        auto const n00 = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
        VERIFY_IS_TRUE(n00.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, *n00);
        VERIFY_IS_TRUE(isRoot);
        VERIFY_IS_TRUE(inScale);

        auto const n01 = surface.MapPadToMidiNote(0, 1, isRoot, inScale);
        VERIFY_IS_TRUE(n01.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 61 }, *n01);
        VERIFY_IS_FALSE(isRoot);
        VERIFY_IS_FALSE(inScale);

        auto const n10 = surface.MapPadToMidiNote(1, 0, isRoot, inScale);
        VERIFY_IS_TRUE(n10.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 65 }, *n10);
        VERIFY_IS_FALSE(isRoot);
        VERIFY_IS_TRUE(inScale);
    }

    // T1-F4-02: inScaleOnly across Major, MinorPentatonic, Blues
    {
        std::array<midiapp::ScaleKind, 3> const kinds{
            midiapp::ScaleKind::Major,
            midiapp::ScaleKind::MinorPentatonic,
            midiapp::ScaleKind::Blues
        };

        for (auto const kind : kinds)
        {
            surface.SetNoteConfig(NoteLayoutConfig{ 0, kind, 3, true, 5 }, actions);
            auto const expectedNotes = ScaleNotes(0, kind, 60, 127);
            VERIFY_IS_TRUE(expectedNotes.size() >= 8);

            for (uint8_t col = 0; col < 8; ++col)
            {
                bool isRoot{ false };
                bool inScale{ false };
                auto const note = surface.MapPadToMidiNote(0, col, isRoot, inScale);
                VERIFY_IS_TRUE(note.has_value());
                VERIFY_IS_TRUE(inScale);
                VERIFY_ARE_EQUAL(expectedNotes[col], *note);
            }
        }
    }

    // T1-F4-03 & T1-F4-04: Note-mode press, aftertouch, release & active LED differential feedback
    {
        NoteLayoutConfig const cfg{ 0, midiapp::ScaleKind::Major, 3, false, 5 };
        surface.SetNoteConfig(cfg, actions);
        SurfaceStateSnapshot snap{};
        snap.noteConfig = cfg;

        auto const idleLeds = surface.BuildLedFeedback(snap, true);
        VerifyAll81LedIndicesPresent(idleLeds);
        LedSpec const* rootPad = FindLedSpec(idleLeds, 11);
        LedSpec const* outScalePad = FindLedSpec(idleLeds, 12);
        LedSpec const* scalePad = FindLedSpec(idleLeds, 13);
        VERIFY_IS_TRUE(rootPad && outScalePad && scalePad);
        VERIFY_IS_FALSE(LedSpecsEqual(*rootPad, *scalePad));
        VERIFY_IS_FALSE(LedSpecsEqual(*scalePad, *outScalePad));

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOn);
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
        VERIFY_ARE_EQUAL(uint8_t{ 100 }, actions[0].velocity);

        auto const activeDiff = surface.BuildLedFeedback(snap, false);
        LedSpec const* active11 = FindLedSpec(activeDiff, 11);
        VERIFY_IS_TRUE(active11 != nullptr);
        VERIFY_IS_FALSE(LedSpecsEqual(*active11, *rootPad));

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xA, 0, 11, 75)), snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitPolyAftertouch);
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
        VERIFY_ARE_EQUAL(uint8_t{ 75 }, actions[0].velocity);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0)), snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOff);
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
    }

    // T1-F4-05: Octave shift Up (91) / Down (92)
    {
        NoteLayoutConfig cfg{ 0, midiapp::ScaleKind::Major, 3, false, 5 };
        surface.SetNoteConfig(cfg, actions);
        SurfaceStateSnapshot snap{};
        snap.noteConfig = cfg;

        std::vector<SurfaceAction> releases{};
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonUp, 127)), snap, actions);
        for (auto const& a : actions)
        {
            if (a.kind == SurfaceActionKind::ShiftOctave)
            {
                cfg.octave += a.octaveDelta;
                snap.noteConfig = cfg;
                surface.SetNoteConfig(cfg, releases);
            }
        }

        bool isRoot{ false };
        bool inScale{ false };
        auto const shiftedUp = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
        VERIFY_IS_TRUE(shiftedUp.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 72 }, *shiftedUp);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonDown, 127)), snap, actions);
        for (auto const& a : actions)
        {
            if (a.kind == SurfaceActionKind::ShiftOctave)
            {
                cfg.octave += a.octaveDelta;
                snap.noteConfig = cfg;
                surface.SetNoteConfig(cfg, releases);
            }
        }

        auto const shiftedBack = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
        VERIFY_IS_TRUE(shiftedBack.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, *shiftedBack);
    }

    // T1-F4-06: Unison duplicate pitch pads (16 and 21 both map to F4 = 65)
    {
        NoteLayoutConfig const cfg{ 0, midiapp::ScaleKind::Major, 3, false, 5 };
        surface.SetNoteConfig(cfg, actions);
        SurfaceStateSnapshot snap{};
        snap.noteConfig = cfg;

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 16, 100)), snap, actions);
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 21, 100)), snap, actions);

        // The second press starts 65 over: on, then off and on again, so ons and offs pair up.
        VERIFY_ARE_EQUAL(size_t{ 3 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOn);
        VERIFY_IS_TRUE(actions[1].kind == SurfaceActionKind::EmitNoteOff);
        VERIFY_IS_TRUE(actions[2].kind == SurfaceActionKind::EmitNoteOn);
        VERIFY_ARE_EQUAL(uint8_t{ 65 }, actions[0].midiNote);
        VERIFY_ARE_EQUAL(uint8_t{ 65 }, actions[1].midiNote);
        VERIFY_ARE_EQUAL(uint8_t{ 65 }, actions[2].midiNote);

        // Release pad 16 while still holding pad 21 -> ref-counted unison keeps note 65 active
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 16, 0)), snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());
        std::vector<SurfaceAction> remainingReleases{};
        surface.SetMode(ControllerMode::Clip, remainingReleases);
        VERIFY_ARE_EQUAL(size_t{ 1 }, remainingReleases.size());
        VERIFY_IS_TRUE(remainingReleases[0].kind == SurfaceActionKind::EmitNoteOff);
        VERIFY_ARE_EQUAL(uint8_t{ 65 }, remainingReleases[0].midiNote);
    }
}

void LaunchpadXE2ETests::T1_F5_ClipNoteModeSwitching()
{
    Log::Comment(L"T1-F5-01..05: Clip <-> Note Mode Switching via CC 95/96 and SetMode without Exiting Programmer Mode");

    LaunchpadXSurface surface{};
    TestLaunchpadOutput output{};
    SurfaceStateSnapshot snap{};
    std::vector<SurfaceAction> actions{};

    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);

    // T1-F5-01: Clip -> Note via CC 96
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Note);
    VERIFY_IS_TRUE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a)
    {
        return a.kind == SurfaceActionKind::SwitchMode && a.targetMode == ControllerMode::Note;
    }));

    auto const noteLeds = surface.BuildLedFeedback(snap, false);
    VerifyAll81LedIndicesPresent(noteLeds);
    VERIFY_IS_TRUE(output.SendUmpPackets(BuildLedSysExPackets(0, noteLeds)));

    // T1-F5-04: Idempotent CC 96 while already in Note mode
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Note);

    // T1-F5-03: Hold 3 pads in Note mode then SetMode(Clip) releases all 3
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 13, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 15, 100)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 3 }, actions.size());

    std::vector<SurfaceAction> releases{};
    surface.SetMode(ControllerMode::Clip, releases);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);
    VERIFY_ARE_EQUAL(size_t{ 3 }, releases.size());
    for (auto const& rel : releases)
    {
        VERIFY_IS_TRUE(rel.kind == SurfaceActionKind::EmitNoteOff);
    }

    // Physical release of 11, 13, 15 in Clip mode must emit zero stray EmitNoteOffs
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 13, 0)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 15, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());

    // T1-F5-02: Note -> Clip via CC 95
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Note);
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);
    auto const clipLeds = surface.BuildLedFeedback(snap, false);
    VerifyAll81LedIndicesPresent(clipLeds);
    VERIFY_IS_TRUE(output.SendUmpPackets(BuildLedSysExPackets(0, clipLeds)));

    // T1-F5-05: Zero 0E 00 Exit Programmer Mode packets emitted during mode transitions
    auto const msgs = DepacketizeSysEx7Messages(output.All());
    std::array<uint8_t, 7> const exitSysEx{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x00u };
    VERIFY_IS_FALSE(ContainsSysExSequence(msgs, exitSysEx));
}

void LaunchpadXE2ETests::T1_F6_EndpointTransportContracts()
{
    Log::Comment(L"T1-F6-01..05: Endpoint Name Heuristic, Connect/Disconnect Handshake, Failure Tolerance, Multi-thread Contract");

    // T1-F6-01: Endpoint name heuristic
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"LPX MIDI"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"MIDIIN2 (LPX MIDI)"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"MIDIOUT2 (LPX MIDI)"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"Launchpad X LPX MIDI"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"LPX DAW"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"TR-8S"));

    // T1-F6-02 & T1-F6-03: Connect handshake & clean disconnect via output sink
    TestLaunchpadOutput output{};
    LaunchpadXSurface surface{};
    SurfaceStateSnapshot snap{};

    VERIFY_IS_TRUE(output.SendUmpPackets(BuildDeviceInquiryPackets(0)));
    VERIFY_IS_TRUE(output.SendUmpPackets(PacketizeSysEx7(0, BuildProgrammerModeSysEx(true))));
    VERIFY_IS_TRUE(output.SendUmpPackets(BuildLedSysExPackets(0, surface.BuildLedFeedback(snap, true))));
    VERIFY_IS_TRUE(output.SendUmpPackets(PacketizeSysEx7(0, BuildProgrammerModeSysEx(false))));

    auto const msgs = DepacketizeSysEx7Messages(output.All());
    std::array<uint8_t, 4> const inquiry{ 0x7Eu, 0x7Fu, 0x06u, 0x01u };
    std::array<uint8_t, 7> const enter{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x01u };
    std::array<uint8_t, 7> const exit{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x00u };
    VERIFY_IS_TRUE(ContainsSysExSequence(msgs, inquiry));
    VERIFY_IS_TRUE(ContainsSysExSequence(msgs, enter));
    VERIFY_IS_TRUE(ContainsSysExSequence(msgs, exit));

    // T1-F6-04: Failure tolerance when sink returns false
    output.Clear();
    output.SetShouldSucceed(false);
    VERIFY_IS_FALSE(output.SendUmpPackets(BuildDeviceInquiryPackets(0)));
    output.SetShouldSucceed(true);
    VERIFY_IS_TRUE(output.SendUmpPackets(PacketizeSysEx7(0, BuildProgrammerModeSysEx(true))));
    VERIFY_IS_TRUE(!output.All().empty());

    // T1-F6-05: Concurrent producer/reader contract verification (live MidiSession hot-plug covered in Hardware Checklist)
    output.Clear();
    std::thread worker([&output]()
    {
        for (int i = 0; i < 8; ++i)
        {
            auto const pkt = BuildChannelLedPacket(0, static_cast<uint8_t>(11 + i), true, LedBehavior::StaticPalette, 21);
            std::array<UmpPacket, 1> const spanBuf{ pkt };
            (void)output.SendUmpPackets(spanBuf);
        }
    });
    worker.join();
    VERIFY_ARE_EQUAL(size_t{ 8 }, output.All().size());
}

void LaunchpadXE2ETests::T1_F7_SequencerRuntimeIntegration()
{
    Log::Comment(L"T1-F7-01..06: PlaybackEngine Clip/Scene/Stop, RecordingTake Note+PolyAT, SongKeyAt, Frame Sync, Slot Record");

    // T1-F7-01 & T1-F7-05: Clip/Scene/Stop & frame-by-frame bar boundary LED transition
    {
        auto doc = BuildGridSequence(3, 2, false);
        auto const order = RemoteTrackOrder(doc);
        EngineFixture f{};
        f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
        f.Engine.Play(0);
        RunUntil(f, At(240));

        LaunchpadXSurface surface{};
        bool recording{ false };
        std::vector<SurfaceAction> actions{};

        auto snap0 = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});
        (void)surface.BuildLedFeedback(snap0, true);

        // Press pad 81 with 1-bar quantize (-1) -> Pending until bar 1
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 127)), snap0, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, -1, recording);

        auto const pendingSnap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});
        auto const pendingDiff = surface.BuildLedFeedback(pendingSnap, false);
        VERIFY_IS_TRUE(FindLedSpec(pendingDiff, 81) != nullptr);

        RunUntil(f, At(Bar + 240));
        auto const playingSnap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});
        auto const playingDiff = surface.BuildLedFeedback(playingSnap, false);
        VERIFY_IS_TRUE(FindLedSpec(playingDiff, 81) != nullptr);
        VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Plays(doc.Tracks[0].Slots[0], 0));

        // Scene 79 (topDownRow 1 = scene 1)
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, SceneButtonIndex(1), 127)), playingSnap, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        RunUntil(f, At(Bar + 480));
        for (auto const& v : f.Engine.LaunchState())
        {
            VERIFY_ARE_EQUAL(int32_t{ 1 }, v.Slot);
        }

        // StopAll (97)
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonCustom, 127)), playingSnap, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        VERIFY_IS_FALSE(f.Engine.IsPlaying());
    }

    // T1-F7-02 & T1-F7-04: Note-mode single-path UMP conversion & RecordingTake capture
    {
        LaunchpadXSurface surface{};
        std::vector<SurfaceAction> actions{};
        surface.SetMode(ControllerMode::Note, actions);
        SurfaceStateSnapshot snap{};

        RecordingTake take{ 0 };
        auto feedNoteSurfaceEvent = [&](uint8_t pad, uint8_t vel, uint8_t atVal, int64_t startTick, int64_t endTick)
        {
            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, pad, vel)), snap, actions);
            VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
            Note note{};
            note.Number = actions[0].midiNote;
            note.Velocity = static_cast<uint16_t>(ScaleUp(actions[0].velocity, 7, 16));
            uint32_t words[2]{};
            BuildNoteOn(note, 0, words);
            take.Add(startTick, words, 2);

            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xA, 0, pad, atVal)), snap, actions);
            auto const atWords = Midi2PolyAftertouchWords(0, 0, actions[0].midiNote, ScaleUp(actions[0].velocity, 7, 32));
            take.Add(startTick + 120, atWords.data(), 2);

            actions.clear();
            surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, pad, 0)), snap, actions);
            BuildNoteOff(note, 0, words);
            take.Add(endTick, words, 2);
        };

        feedNoteSurfaceEvent(11, 100, 80, 0, 480);
        auto const clip = take.FinishLoop(Bar, L"c-rec", L"Take", L"");
        VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Notes.size());
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, clip.Notes[0].Number);
        VERIFY_ARE_EQUAL(int64_t{ 0 }, clip.Notes[0].Tick);
        VERIFY_ARE_EQUAL(int64_t{ 480 }, clip.Notes[0].Length);
        VERIFY_IS_TRUE(!clip.Events.empty());
    }

    // T1-F7-03: SongKeyAt dynamic root/scale update
    {
        Sequence seq = BuildGridSequence(1, 1, false);
        HarmonyTrack harmony{};
        harmony.Keys = {
            KeyChange{ 0, 0, midiapp::ScaleKind::Major },
            KeyChange{ Bar * 4, 6, midiapp::ScaleKind::Dorian }
        };
        seq.Harmony = std::move(harmony);

        auto const k0 = SongKeyAt(seq, 0);
        auto const k4 = SongKeyAt(seq, Bar * 4);
        VERIFY_IS_TRUE(k0.has_value() && k4.has_value());

        LaunchpadXSurface surface{};
        std::vector<SurfaceAction> releases{};
        surface.SetMode(ControllerMode::Note, releases);
        surface.SetNoteConfig(NoteLayoutConfig{ k0->Root, k0->Scale, 3, false, 5 }, releases);

        bool isRoot{ false };
        bool inScale{ false };
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, *surface.MapPadToMidiNote(0, 0, isRoot, inScale));

        surface.SetNoteConfig(NoteLayoutConfig{ k4->Root, k4->Scale, 3, false, 5 }, releases);
        VERIFY_ARE_EQUAL(uint8_t{ 66 }, *surface.MapPadToMidiNote(0, 0, isRoot, inScale));
    }

    // T1-F7-06: Record toggle 98 & slotRecording snapshot LED state
    {
        auto const doc = BuildGridSequence(2, 2, false);
        LaunchpadXSurface surface{};
        auto const snapIdle = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});
        auto const snapRec = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, true, true, true, L"t-0", 0, {});
        auto const ledsIdle = surface.BuildLedFeedback(snapIdle, true);
        auto const ledsRec = surface.BuildLedFeedback(snapRec, true);
        VERIFY_IS_FALSE(LedSpecsEqual(*FindLedSpec(ledsIdle, ButtonRecord), *FindLedSpec(ledsRec, ButtonRecord)));
        VERIFY_IS_FALSE(LedSpecsEqual(*FindLedSpec(ledsIdle, 81), *FindLedSpec(ledsRec, 81)));
    }
}

void LaunchpadXE2ETests::T1_F8_SettingsConfigInvariants()
{
    Log::Comment(L"T1-F8-01..05: Default NoteLayoutConfig Invariants, Config Update Release/Refresh, Mode Sync Contract");

    NoteLayoutConfig const defaults{};
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, defaults.rootNote);
    VERIFY_IS_TRUE(defaults.scale == midiapp::ScaleKind::Major);
    VERIFY_ARE_EQUAL(3, defaults.octave);
    VERIFY_IS_FALSE(defaults.inScaleOnly);
    VERIFY_ARE_EQUAL(uint8_t{ 5 }, defaults.rowStepSemitones);

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);

    bool isRoot{ false };
    bool inScale{ false };
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, *surface.MapPadToMidiNote(0, 0, isRoot, inScale));
    auto const topRight = surface.MapPadToMidiNote(7, 7, isRoot, inScale);
    VERIFY_IS_TRUE(topRight.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 102 }, *topRight);

    // Hold pad 11 and update config -> held note releases cleanly & 81 LEDs refresh
    SurfaceStateSnapshot snap{};
    snap.noteConfig = defaults;
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 110)), snap, actions);

    NoteLayoutConfig const updated{ 7, midiapp::ScaleKind::NaturalMinor, 2, true, 5 };
    std::vector<SurfaceAction> releases{};
    surface.SetNoteConfig(updated, releases);
    VERIFY_ARE_EQUAL(size_t{ 1 }, releases.size());
    VERIFY_IS_TRUE(releases[0].kind == SurfaceActionKind::EmitNoteOff);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, releases[0].midiNote);

    snap.noteConfig = updated;
    auto const refreshed = surface.BuildLedFeedback(snap, false);
    VerifyAll81LedIndicesPresent(refreshed);
}

void LaunchpadXE2ETests::T2_F1_ProtocolBoundariesAndMalformedUmp()
{
    Log::Comment(L"T2-F1-01..07: 7-bit RGB Clamp, >81 LED Chunking, Malformed UMP, Invalid SysEx7, Vel-0 Release, Bootloader, Group 15");

    // T2-F1-01: 7-bit clamping & >0x7F protection
    {
        auto const clamped = RgbColor::FromRgb24(0xFFFFFFu);
        VERIFY_ARE_EQUAL(uint8_t{ 127 }, clamped.r);
        VERIFY_ARE_EQUAL(uint8_t{ 127 }, clamped.g);
        VERIFY_ARE_EQUAL(uint8_t{ 127 }, clamped.b);

        std::array<uint8_t, 8> const badPayload{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x03u, 0x80u, 0xFFu };
        auto const packets = PacketizeSysEx7(0, badPayload);
        VERIFY_IS_TRUE(packets.empty());
    }

    // T2-F1-02: > 81 LED chunking boundary (0, 1, 81, 82, 163)
    {
        VERIFY_ARE_EQUAL(size_t{ 0 }, DepacketizeSysEx7Messages(BuildLedSysExPackets(0, {})).size());

        std::vector<LedSpec> specs1(1, LedSpec{ 11, LedBehavior::StaticPalette, 21, 0, {} });
        VERIFY_ARE_EQUAL(size_t{ 1 }, DepacketizeSysEx7Messages(BuildLedSysExPackets(0, specs1)).size());

        std::vector<LedSpec> specs81(81, LedSpec{ 11, LedBehavior::StaticPalette, 21, 0, {} });
        VERIFY_ARE_EQUAL(size_t{ 1 }, DepacketizeSysEx7Messages(BuildLedSysExPackets(0, specs81)).size());

        std::vector<LedSpec> specs82(82, LedSpec{ 11, LedBehavior::StaticPalette, 21, 0, {} });
        VERIFY_ARE_EQUAL(size_t{ 2 }, DepacketizeSysEx7Messages(BuildLedSysExPackets(0, specs82)).size());

        std::vector<LedSpec> specs163(163, LedSpec{ 11, LedBehavior::StaticPalette, 21, 0, {} });
        VERIFY_ARE_EQUAL(size_t{ 3 }, DepacketizeSysEx7Messages(BuildLedSysExPackets(0, specs163)).size());
    }

    // T2-F1-03: Malformed / truncated / oversized UMP words
    {
        InputEvent ev{};
        VERIFY_IS_FALSE(TryParseInputUmp(nullptr, 0, ev));
        uint32_t const zeroWord[1]{ 0x20900B64u };
        VERIFY_IS_FALSE(TryParseInputUmp(zeroWord, 0, ev));
        uint32_t const trunc4[1]{ 0x40900B00u };
        VERIFY_IS_FALSE(TryParseInputUmp(trunc4, 1, ev));
        uint32_t const trunc3[1]{ 0x30047E7Fu };
        VERIFY_IS_FALSE(TryParseInputUmp(trunc3, 1, ev));
        uint32_t const unknownType[1]{ 0xF0000000u };
        VERIFY_IS_FALSE(TryParseInputUmp(unknownType, 1, ev));
        uint32_t const tooMany[5]{ 0x20900B64u, 0, 0, 0, 0 };
        (void)TryParseInputUmp(tooMany, 5, ev);
    }

    // T2-F1-04: Invalid SysEx7 framing & recovery
    {
        InputEvent ev{};
        SysEx7Accumulator acc{};
        // byteCount=7 (>6) invalid, orphan continue (status=2), orphan end (status=3)
        std::array<uint32_t, 2> const badCount{ 0x30170000u, 0x00000000u };
        VERIFY_IS_FALSE(TryParseInputUmp(badCount.data(), 2, ev, &acc));
        std::array<uint32_t, 2> const orphanCont{ 0x30221122u, 0x00000000u };
        VERIFY_IS_FALSE(TryParseInputUmp(orphanCont.data(), 2, ev, &acc));
        std::array<uint32_t, 2> const orphanEnd{ 0x30321122u, 0x00000000u };
        VERIFY_IS_FALSE(TryParseInputUmp(orphanEnd.data(), 2, ev, &acc));

        // Recover on next valid App-mode Device Inquiry
        std::array<uint8_t, 17> const validReply{
            0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x20u, 0x29u,
            0x13u, 0x01u, 0x00u, 0x00u, 0x30u, 0x33u, 0x35u, 0x31u, 0xF7u
        };
        bool const recovered = TryParseSysExPackets(PacketizeFramedSysEx(0, validReply), ev, acc);
        VERIFY_IS_TRUE(recovered);
        VERIFY_IS_TRUE(ev.kind == InputEventKind::DeviceInquiryReply);
    }

    // T2-F1-05: 0x90 vel-0 vs 0x80 release
    {
        auto const ev1Zero = ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 0));
        auto const ev1Off = ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 64));
        VERIFY_IS_TRUE(ev1Zero.kind == InputEventKind::PadRelease);
        VERIFY_IS_TRUE(ev1Off.kind == InputEventKind::PadRelease);

        auto const ev2Off = ParseTwoWords(Midi2NoteOffWords(0, 0, 11, 0x8000u));
        VERIFY_IS_TRUE(ev2Off.kind == InputEventKind::PadRelease);
    }

    // T2-F1-06: Bootloader Device Inquiry reply (13 11)
    {
        std::array<uint8_t, 17> const bootReply{
            0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x20u, 0x29u,
            0x13u, 0x11u, 0x00u, 0x00u, 0x30u, 0x31u, 0x30u, 0x30u, 0xF7u
        };
        InputEvent ev{};
        SysEx7Accumulator acc{};
        bool const parsed = TryParseSysExPackets(PacketizeFramedSysEx(0, bootReply), ev, acc);
        VERIFY_IS_TRUE(parsed);
        VERIFY_IS_TRUE(ev.kind == InputEventKind::DeviceInquiryReply);
        VERIFY_IS_TRUE(ev.isBootloader);
        VERIFY_ARE_EQUAL(uint8_t{ '0' }, ev.version[0]);
        VERIFY_ARE_EQUAL(uint8_t{ '1' }, ev.version[1]);
    }

    // T2-F1-07: Group 15 preservation & empty payload
    {
        // An empty SysEx (F0 F7) is still one complete packet, carrying no bytes.
        auto const empty = PacketizeSysEx7(15, {});
        VERIFY_ARE_EQUAL(size_t{ 1 }, empty.size());
        VERIFY_ARE_EQUAL(uint32_t{ 0x3F000000u }, empty[0].words[0]);
        std::array<LedSpec, 1> const one{ { LedSpec{ 11, LedBehavior::StaticPalette, 5, 0, {} } } };
        auto const pkts = BuildLedSysExPackets(15, one);
        VERIFY_IS_TRUE(!pkts.empty());
        VERIFY_ARE_EQUAL(uint32_t{ 0xFu }, (pkts[0].words[0] >> 24) & 0x0Fu);
    }
}

void LaunchpadXE2ETests::T2_F2_CoordinateAndButtonBoundaries()
{
    Log::Comment(L"T2-F2-01..05: Invalid Pads, Unmapped CCs, SceneButtonIndex Bounds, Button Release, Non-Pad Aftertouch");

    LaunchpadXSurface surface{};
    auto const doc = BuildGridSequence(4, 4, false);
    auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});
    std::vector<SurfaceAction> actions{};

    // T2-F2-01: Non-grid pad note numbers outside 11..88
    std::array<uint8_t, 8> const invalidPads{ 0, 10, 19, 20, 89, 90, 100, 127 };
    for (uint8_t pad : invalidPads)
    {
        actions.clear();
        surface.HandleInput(InputEvent{ InputEventKind::PadPress, pad, 100, false, {} }, snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());
    }

    // T2-F2-02: Unmapped CC numbers
    std::array<uint8_t, 4> const unmappedCcs{ 0, 9, LogoLed, 127 };
    for (uint8_t cc : unmappedCcs)
    {
        // The parser rejects a CC that isn't a Launchpad X button, so it never reaches the surface.
        uint32_t const word = Midi1Word(0, 0xB, 0, cc, 127);
        InputEvent ev{};
        VERIFY_IS_FALSE(TryParseInputUmp(&word, 1, ev));
    }
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);

    // T2-F2-03: SceneButtonIndex boundary
    VERIFY_ARE_EQUAL(uint8_t{ 89 }, SceneButtonIndex(0));
    VERIFY_ARE_EQUAL(uint8_t{ 19 }, SceneButtonIndex(7));
    uint8_t const oobSceneBtn = SceneButtonIndex(8);
    VERIFY_IS_TRUE(oobSceneBtn <= 89);

    // T2-F2-04: Button release produces zero actions
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);

    // T2-F2-05: Poly aftertouch on non-pad coordinates
    std::vector<SurfaceAction> releases{};
    surface.SetMode(ControllerMode::Note, releases);
    std::array<uint8_t, 3> const nonPadAt{ 91, 89, 10 };
    for (uint8_t idx : nonPadAt)
    {
        actions.clear();
        surface.HandleInput(InputEvent{ InputEventKind::PadAftertouch, idx, 80, false, {} }, snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());
    }
}

void LaunchpadXE2ETests::T2_F3_ClipModeViewportAndEmptyBounds()
{
    Log::Comment(L"T2-F3-01..06: Empty Sequence, OOB Viewport, (0,0) & (max,max) Scroll Bounds, Truncated Slots, Folder-Only");

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};

    // T2-F3-01: Empty sequence
    {
        Sequence emptySeq{};
        auto const snap = BuildSurfaceSnapshot(emptySeq, {}, L"", 0, false, false, false, L"", 0, {});
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, snap.totalTracks);
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, snap.totalScenes);

        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 100)), snap, actions);
        VERIFY_IS_FALSE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a)
        {
            return a.kind == SurfaceActionKind::TriggerOrStopSlot;
        }));
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, 89, 127)), snap, actions);
        VERIFY_IS_FALSE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a)
        {
            return a.kind == SurfaceActionKind::LaunchScene;
        }));
        VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));
    }

    // T2-F3-02, T2-F3-03, T2-F3-04: Viewport bounds & scroll clamping
    {
        auto const doc = BuildGridSequence(12, 12, false);
        auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});

        surface.SetViewport(0, 0);
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonUp, 127)), snap, actions);
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonLeft, 127)), snap, actions);
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.TrackOffset());
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.SceneOffset());

        surface.SetViewport(100, 100);
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 100)), snap, actions);
        VERIFY_IS_FALSE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a)
        {
            return a.kind == SurfaceActionKind::TriggerOrStopSlot && a.trackIndex < 12 && a.sceneIndex < 12;
        }));
        VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));

        surface.SetViewport(4, 4);
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonRight, 127)), snap, actions);
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonDown, 127)), snap, actions);
        VERIFY_IS_TRUE(surface.TrackOffset() <= 11);
        VERIFY_IS_TRUE(surface.SceneOffset() <= 11);
    }

    // T2-F3-05: Truncated Slots vector
    {
        auto doc = BuildGridSequence(2, 4, false);
        doc.Tracks[1].Slots.resize(1);
        auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, false, false, false, L"", 0, {});
        VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));
    }

    // T2-F3-06: Folder-only sequence
    {
        Sequence foldersOnly{};
        Track f1{};
        f1.Id = L"f-1";
        f1.IsFolder = true;
        Track f2{};
        f2.Id = L"f-2";
        f2.IsFolder = true;
        foldersOnly.Tracks = { f1, f2 };
        VERIFY_ARE_EQUAL(size_t{ 0 }, RemoteTrackOrder(foldersOnly).size());
        auto const snap = BuildSurfaceSnapshot(foldersOnly, {}, L"", 0, false, false, false, L"", 0, {});
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, snap.totalTracks);
    }
}

void LaunchpadXE2ETests::T2_F4_NoteModeOctaveScaleAndReleaseBounds()
{
    Log::Comment(L"T2-F4-01..06: Extreme Octaves -2/+8, Unknown ScaleKind, Zero RowStep, Octave Clamping, Orphan Release");

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);
    bool isRoot{ false };
    bool inScale{ false };

    // T2-F4-01: Octave -2 lower clamping
    surface.SetNoteConfig(NoteLayoutConfig{ 0, midiapp::ScaleKind::Major, -2, false, 5 }, actions);
    for (uint8_t r = 0; r < 8; ++r)
    {
        for (uint8_t c = 0; c < 8; ++c)
        {
            auto const n = surface.MapPadToMidiNote(r, c, isRoot, inScale);
            if (n.has_value())
            {
                VERIFY_IS_TRUE(*n <= 127);
            }
        }
    }

    // T2-F4-02: Octave +8 upper clamping
    surface.SetNoteConfig(NoteLayoutConfig{ 11, midiapp::ScaleKind::Chromatic, 8, false, 5 }, actions);
    auto const topRight = surface.MapPadToMidiNote(7, 7, isRoot, inScale);
    if (topRight.has_value())
    {
        VERIFY_IS_TRUE(*topRight <= 127);
    }

    // T2-F4-03: Unknown ScaleKind & root >= 12
    surface.SetNoteConfig(NoteLayoutConfig{ 25, static_cast<midiapp::ScaleKind>(99), 3, true, 5 }, actions);
    auto const fallbackNote = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
    VERIFY_IS_TRUE(fallbackNote.has_value());
    VERIFY_IS_TRUE(*fallbackNote <= 127);

    // T2-F4-04: Zero rowStepSemitones
    surface.SetNoteConfig(NoteLayoutConfig{ 0, midiapp::ScaleKind::Major, 3, false, 0 }, actions);
    auto const zeroStep = surface.MapPadToMidiNote(1, 0, isRoot, inScale);
    VERIFY_IS_TRUE(!zeroStep.has_value() || *zeroStep <= 127);

    // T2-F4-05: 20x Up / 20x Down octave clamping
    NoteLayoutConfig cfg{ 0, midiapp::ScaleKind::Major, 3, false, 5 };
    surface.SetNoteConfig(cfg, actions);
    SurfaceStateSnapshot snap{};
    snap.noteConfig = cfg;
    for (int i = 0; i < 20; ++i)
    {
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonUp, 127)), snap, actions);
    }
    auto const highNote = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
    VERIFY_IS_TRUE(!highNote.has_value() || *highNote <= 127);

    for (int i = 0; i < 40; ++i)
    {
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonDown, 127)), snap, actions);
    }
    auto const lowNote = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
    VERIFY_IS_TRUE(!lowNote.has_value() || *lowNote <= 127);

    // T2-F4-06: Orphan release & 0x90 velocity-0 release
    surface.SetNoteConfig(NoteLayoutConfig{ 0, midiapp::ScaleKind::Major, 3, false, 5 }, actions);
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOn);

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOff);
}

void LaunchpadXE2ETests::T2_F5_ModeSwitchRapidAndMidHoldBounds()
{
    Log::Comment(L"T2-F5-01..05: Rapid Toggles, Cross-Mode Pad Release, Mid-Hold SetNoteConfig, Empty Grid Full Refresh");

    LaunchpadXSurface surface{};
    SurfaceStateSnapshot snap{};
    std::vector<SurfaceAction> actions{};

    // T2-F5-01 & T2-F5-02: Hold 4 pads in Note mode -> rapid 95 -> 96 -> 95 -> release in Clip mode
    surface.SetMode(ControllerMode::Note, actions);
    actions.clear();
    for (uint8_t pad : std::array<uint8_t, 4>{ 11, 22, 33, 44 })
    {
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, pad, 100)), snap, actions);
    }
    VERIFY_ARE_EQUAL(size_t{ 4 }, actions.size());

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snap, actions);
    size_t const noteOffCount = static_cast<size_t>(std::count_if(actions.begin(), actions.end(), [](SurfaceAction const& a)
    {
        return a.kind == SurfaceActionKind::EmitNoteOff;
    }));
    VERIFY_ARE_EQUAL(size_t{ 4 }, noteOffCount);

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);

    // Physical release of pad 11 in Clip mode does nothing
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());

    // T2-F5-03: Press pad 81 in Clip -> switch to Note -> release pad 81
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 100)), snap, actions);
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 81, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());

    // T2-F5-04: SetNoteConfig while holding pads 11, 12
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 12, 100)), snap, actions);
    std::vector<SurfaceAction> releases{};
    surface.SetNoteConfig(NoteLayoutConfig{ 2, midiapp::ScaleKind::NaturalMinor, 3, false, 5 }, releases);
    VERIFY_ARE_EQUAL(size_t{ 2 }, releases.size());

    // T2-F5-05: Full 81-LED refresh guaranteed on mode switch even on empty grid
    (void)surface.BuildLedFeedback(snap, true);
    surface.SetMode(ControllerMode::Clip, releases);
    auto const afterSwitch = surface.BuildLedFeedback(snap, false);
    VerifyAll81LedIndicesPresent(afterSwitch);
}

void LaunchpadXE2ETests::T2_F6_EndpointNameAndInquiryRejections()
{
    Log::Comment(L"T2-F6-01..05: LPX DAW Rejection, Non-Novation Inquiry, Timeout Contract, Empty Span, Case Variants");

    // T2-F6-01 & T2-F6-05: Wrong port names & case variants
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"LPX DAW"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"MIDIIN (LPX DAW)"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"MIDIOUT (LPX DAW)"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"MIDIIN2 (LPX DAW)"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L""));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"lpx midi"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"Novation Launchpad X (LPX MIDI)"));

    // T2-F6-02: Non-Launchpad Device Inquiry reply (Sequential Circuits 00 00 01)
    std::array<uint8_t, 17> const nonLaunchpadReply{
        0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x00u, 0x01u,
        0x13u, 0x01u, 0x00u, 0x00u, 0x30u, 0x31u, 0x30u, 0x30u, 0xF7u
    };
    InputEvent ev{};
    SysEx7Accumulator acc{};
    bool const parsedAsLaunchpad = TryParseSysExPackets(PacketizeFramedSysEx(0, nonLaunchpadReply), ev, acc);
    VERIFY_IS_FALSE(parsedAsLaunchpad);

    // T2-F6-03 & T2-F6-04: Missing inquiry reply & empty SendUmpPackets span
    TestLaunchpadOutput output{};
    VERIFY_IS_TRUE(output.SendUmpPackets({}));
    VERIFY_ARE_EQUAL(size_t{ 0 }, output.All().size());
}

void LaunchpadXE2ETests::T2_F7_RuntimeNullTrackAndKeyFallbacks()
{
    Log::Comment(L"T2-F7-01..05: Zero Tracks, Missing SlotRecording TrackId, Stale LaunchState, Null SongKeyAt, Channel -1");

    // T2-F7-01: Note mode with 0 tracks produces EmitNoteOn cleanly
    {
        Sequence emptyDoc{};
        LaunchpadXSurface surface{};
        std::vector<SurfaceAction> actions{};
        surface.SetMode(ControllerMode::Note, actions);
        auto const snap = BuildSurfaceSnapshot(emptyDoc, {}, L"", 0, false, false, false, L"", 0, {});
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOn);
    }

    // T2-F7-02 & T2-F7-03: Unknown slotRecordingTrackId & stale TrackLaunchView
    {
        auto const doc = BuildGridSequence(2, 2, false);
        TrackLaunchView stale{};
        stale.TrackId = L"deleted-track";
        stale.Mode = TrackPlayMode::Clip;
        stale.Slot = 0;

        auto const snap = BuildSurfaceSnapshot(doc, { stale }, L"t-0", 0, false, true, true, L"missing-id", 0, {});
        LaunchpadXSurface surface{};
        VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));
    }

    // T2-F7-04: SongKeyAt returns nullopt when no Harmony track exists
    {
        auto const doc = BuildGridSequence(1, 1, false);
        VERIFY_IS_FALSE(SongKeyAt(doc, 0).has_value());
    }

    // T2-F7-05: Destination.Channel == -1 clamps to valid channel 0..15 in BuildNoteOn
    {
        auto doc = BuildGridSequence(1, 1, false);
        doc.Tracks[0].Destination.Channel = -1;
        Note note{};
        note.Number = 60;
        note.Velocity = 0x8000u;
        note.Channel = static_cast<uint8_t>(doc.Tracks[0].Destination.Channel >= 0 && doc.Tracks[0].Destination.Channel <= 15
            ? doc.Tracks[0].Destination.Channel
            : 0);
        uint32_t words[2]{};
        BuildNoteOn(note, doc.Tracks[0].Destination.Group, words);
        VERIFY_ARE_EQUAL(uint32_t{ 0x4u }, words[0] >> 28);
        VERIFY_ARE_EQUAL(uint32_t{ 0u }, (words[0] >> 16) & 0x0Fu);
    }
}

void LaunchpadXE2ETests::T2_F8_ConfigValidationAndLongStrings()
{
    Log::Comment(L"T2-F8-01..05: Extreme Octave/Root, Empty Endpoint, Invalid ControllerMode, Unknown ScaleKind, Long Unicode Names");

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> releases{};

    // T2-F8-01 & T2-F8-04: Extreme octave/root & unknown ScaleKind
    surface.SetNoteConfig(NoteLayoutConfig{ 255, static_cast<midiapp::ScaleKind>(255), -99, false, 5 }, releases);
    bool isRoot{ false };
    bool inScale{ false };
    auto const lowClamp = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
    VERIFY_IS_TRUE(!lowClamp.has_value() || *lowClamp <= 127);

    surface.SetNoteConfig(NoteLayoutConfig{ 255, static_cast<midiapp::ScaleKind>(255), 99, false, 5 }, releases);
    auto const highClamp = surface.MapPadToMidiNote(7, 7, isRoot, inScale);
    VERIFY_IS_TRUE(!highClamp.has_value() || *highClamp <= 127);

    // T2-F8-02: Empty endpoint string
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L""));

    // T2-F8-03: Invalid ControllerMode cast
    surface.SetMode(static_cast<ControllerMode>(99), releases);
    auto const fallbackLeds = surface.BuildLedFeedback(SurfaceStateSnapshot{}, true);
    VerifyAll81LedIndicesPresent(fallbackLeds);

    // T2-F8-05: >1024-char and non-ASCII endpoint names
    std::wstring const longName(1500, L'A');
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(longName));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"Novation \u266B (LPX MIDI)"));
}

void LaunchpadXE2ETests::T3_01_ClipAndPlaybackQuantizedLaunch()
{
    Log::Comment(L"T3-01: Clip + PlaybackEngine 1-bar quantized launch and stop");
    auto doc = BuildGridSequence(1, 2, false);
    auto const order = RemoteTrackOrder(doc);
    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    RunUntil(f, At(240));

    LaunchpadXSurface surface{};
    bool recording{ false };
    std::vector<SurfaceAction> actions{};

    auto snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});
    (void)surface.BuildLedFeedback(snap, true);

    // Press pad 81 (slot 0) with -1 bar quantize
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, -1, recording);
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Pending);

    RunUntil(f, At(Bar + 240));
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Plays(doc.Tracks[0].Slots[0], 0));

    // Press pad 81 again to queue stop at bar 2
    actions.clear();
    snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, -1, recording);
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Pending);
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].PendingMode == TrackPlayMode::Stopped);

    RunUntil(f, At(Bar * 2 + 240));
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Mode == TrackPlayMode::Stopped);
}

void LaunchpadXE2ETests::T3_02_ClipAndPlaybackOverwritePendingLaunch()
{
    Log::Comment(L"T3-02: Overwriting pending clip launch before beat boundary");
    auto doc = BuildGridSequence(1, 2, false);
    auto const order = RemoteTrackOrder(doc);
    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    RunUntil(f, At(120));

    LaunchpadXSurface surface{};
    bool recording{ false };
    std::vector<SurfaceAction> actions{};

    f.Engine.LaunchClip(L"t-0", doc.Tracks[0].Slots[0], 0, 0);
    RunUntil(f, At(240));

    auto snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});
    (void)surface.BuildLedFeedback(snap, true);

    // Queue slot 1 (pad 71) on beat 960
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 71, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 960, recording);
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Queues(doc.Tracks[0].Slots[1], 1));

    // Re-trigger slot 0 (pad 81) before beat boundary
    actions.clear();
    snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 960, recording);
    RunUntil(f, At(960 + 120));
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Plays(doc.Tracks[0].Slots[0], 0));
}

void LaunchpadXE2ETests::T3_03_ClipAndSlotRecordingLifecycle()
{
    Log::Comment(L"T3-03: Clip + SlotRecording empty -> recording -> playing lifecycle");
    auto doc = BuildGridSequence(1, 1, false);
    doc.Tracks[0].Slots[0].clear();
    doc.Clips.clear();
    NormalizeSequence(doc);

    LaunchpadXSurface surface{};
    auto const snapRec = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, true, true, true, L"t-0", 0, {});
    auto const recLeds = surface.BuildLedFeedback(snapRec, true);
    LedSpec const* recSlotLed = FindLedSpec(recLeds, 81);
    LedSpec const* recBtnLed = FindLedSpec(recLeds, ButtonRecord);
    VERIFY_IS_TRUE(recSlotLed && recBtnLed);

    RecordingTake take{ 0 };
    uint32_t words[2]{};
    BuildNoteOn(testdata::MakeNote(0, 480, 60), 0, words);
    take.Add(0, words, 2);
    BuildNoteOff(testdata::MakeNote(0, 480, 60), 0, words);
    take.Add(480, words, 2);
    auto recordedClip = take.FinishLoop(Bar, L"c-slot-0", L"Recorded", L"");
    doc.Clips.push_back(recordedClip);
    doc.Tracks[0].Slots[0] = recordedClip.Id;
    NormalizeSequence(doc);

    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    f.Engine.LaunchClipAt(L"t-0", recordedClip.Id, 0, 0);
    RunUntil(f, At(Bar));

    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Plays(recordedClip.Id, 0));
    auto const ons = NoteOns(f.Output.All());
    VERIFY_IS_TRUE(!ons.empty());
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, NoteNumberFromMessage(ons[0]));
}

void LaunchpadXE2ETests::T3_04_NoteAndSongKeyAtDynamicKeyChange()
{
    Log::Comment(L"T3-04: Note Mode + SongKeyAt harmonic change while holding pad 11");
    Sequence seq = BuildGridSequence(1, 1, false);
    seq.Harmony = HarmonyTrack{
        { KeyChange{ 0, 0, midiapp::ScaleKind::Major }, KeyChange{ Bar * 2, 6, midiapp::ScaleKind::NaturalMinor } },
        {},
        false,
        nullptr
    };

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);

    auto const k0 = SongKeyAt(seq, 0);
    VERIFY_IS_TRUE(k0.has_value());
    NoteLayoutConfig cfg0{ k0->Root, k0->Scale, 3, false, 5 };
    surface.SetNoteConfig(cfg0, actions);

    SurfaceStateSnapshot snap{};
    snap.noteConfig = cfg0;
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);

    auto const k2 = SongKeyAt(seq, Bar * 2);
    VERIFY_IS_TRUE(k2.has_value());
    NoteLayoutConfig cfg2{ k2->Root, k2->Scale, 3, false, 5 };
    std::vector<SurfaceAction> releases{};
    surface.SetNoteConfig(cfg2, releases);
    VERIFY_ARE_EQUAL(size_t{ 1 }, releases.size());
    VERIFY_IS_TRUE(releases[0].kind == SurfaceActionKind::EmitNoteOff);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, releases[0].midiNote);

    snap.noteConfig = cfg2;
    auto const newLeds = surface.BuildLedFeedback(snap, false);
    VerifyAll81LedIndicesPresent(newLeds);

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    VERIFY_ARE_EQUAL(uint8_t{ 66 }, actions[0].midiNote);
}

void LaunchpadXE2ETests::T3_05_NoteAndSongKeyAtInScaleOctaveShift()
{
    Log::Comment(L"T3-05: Note Mode + SongKeyAt G Blues + inScaleOnly + Octave Up");
    Sequence seq = BuildGridSequence(1, 1, false);
    seq.Harmony = HarmonyTrack{
        { KeyChange{ 0, 7, midiapp::ScaleKind::Blues } },
        {},
        false,
        nullptr
    };

    auto const k0 = SongKeyAt(seq, 0);
    VERIFY_IS_TRUE(k0.has_value());

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);
    NoteLayoutConfig cfg{ k0->Root, k0->Scale, 3, true, 5 };
    surface.SetNoteConfig(cfg, actions);

    SurfaceStateSnapshot snap{};
    snap.noteConfig = cfg;
    std::vector<SurfaceAction> releases{};
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonUp, 127)), snap, actions);
    for (auto const& a : actions)
    {
        if (a.kind == SurfaceActionKind::ShiftOctave)
        {
            cfg.octave += a.octaveDelta;
            surface.SetNoteConfig(cfg, releases);
        }
    }

    // G5=79, Bb5=82, C6=84
    std::array<uint8_t, 3> const expected{ 79, 82, 84 };
    for (uint8_t i = 0; i < 3; ++i)
    {
        bool isRoot{ false };
        bool inScale{ false };
        auto const note = surface.MapPadToMidiNote(0, i, isRoot, inScale);
        VERIFY_IS_TRUE(note.has_value());
        VERIFY_IS_TRUE(inScale);
        VERIFY_ARE_EQUAL(expected[i], *note);
    }
}

void LaunchpadXE2ETests::T3_06_ModeSwitchWithHeldNotesAndActivePlayback()
{
    Log::Comment(L"T3-06: Mode switch Note -> Clip while holding 3 pads and clip playing");
    auto doc = BuildGridSequence(1, 1, false);
    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    f.Engine.LaunchClip(L"t-0", doc.Tracks[0].Slots[0], 0, 0);
    RunUntil(f, At(240));

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);
    SurfaceStateSnapshot snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 14, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 22, 100)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 3 }, actions.size());

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snap, actions);
    size_t const offCount = static_cast<size_t>(std::count_if(actions.begin(), actions.end(), [](SurfaceAction const& a)
    {
        return a.kind == SurfaceActionKind::EmitNoteOff;
    }));
    VERIFY_ARE_EQUAL(size_t{ 3 }, offCount);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);
    VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, false));

    RunUntil(f, At(Bar));
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Plays(doc.Tracks[0].Slots[0], 0));
}

void LaunchpadXE2ETests::T3_07_ModeSwitchDuringSlotRecording()
{
    Log::Comment(L"T3-07: Clip -> Note -> Clip during active slot recording");
    auto doc = BuildGridSequence(1, 1, false);
    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    RecordingTake take{ 0 };

    auto snapClip = BuildSurfaceSnapshot(doc, {}, L"t-0", 0, true, true, true, L"t-0", 0, {});
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snapClip, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Note);

    // Hold pad 11 in Note mode and record into take
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 110)), snapClip, actions);
    Note n{};
    n.Number = actions[0].midiNote;
    n.Velocity = static_cast<uint16_t>(ScaleUp(actions[0].velocity, 7, 16));
    uint32_t words[2]{};
    BuildNoteOn(n, 0, words);
    take.Add(120, words, 2);

    // Switch back to Clip while still holding pad 11
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snapClip, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);
    VERIFY_IS_TRUE(std::any_of(actions.begin(), actions.end(), [](SurfaceAction const& a)
    {
        return a.kind == SurfaceActionKind::EmitNoteOff && a.midiNote == 60;
    }));

    BuildNoteOff(n, 0, words);
    take.Add(360, words, 2);
    auto const clip = take.FinishLoop(Bar, L"c-take", L"SlotTake", L"");
    VERIFY_ARE_EQUAL(size_t{ 1 }, clip.Notes.size());
    VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snapClip, false));
}

void LaunchpadXE2ETests::T3_08_ViewportScrollAndOffScreenSceneLaunch()
{
    Log::Comment(L"T3-08: Viewport Scroll (4,2) + Scene Launch affecting off-screen tracks");
    auto doc = BuildGridSequence(12, 10, true);
    auto const order = RemoteTrackOrder(doc);
    VERIFY_ARE_EQUAL(size_t{ 12 }, order.size());

    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    RunUntil(f, At(240));

    LaunchpadXSurface surface{};
    surface.SetViewport(4, 2);
    auto snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[4], 2, true, false, false, L"", 0, {});

    bool recording{ false };
    std::vector<SurfaceAction> actions{};
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, SceneButtonIndex(0), 127)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::LaunchScene);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, actions[0].sceneIndex);

    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    RunUntil(f, At(480));

    auto const views = f.Engine.LaunchState();
    VERIFY_ARE_EQUAL(size_t{ 12 }, views.size());
    for (auto const& v : views)
    {
        VERIFY_ARE_EQUAL(int32_t{ 2 }, v.Slot);
    }
}

void LaunchpadXE2ETests::T3_09_ViewportScrollAndPartialBoundaryWindow()
{
    Log::Comment(L"T3-09: Partial 10x3 Sequence at Viewport (4,0)");
    auto doc = BuildGridSequence(10, 3, false);
    LaunchpadXSurface surface{};
    surface.SetViewport(4, 0);
    auto const snap = BuildSurfaceSnapshot(doc, {}, L"t-4", 0, false, false, false, L"", 0, {});

    // Pad 81 (top row = scene 0, col 1 = track 4) is valid
    std::vector<SurfaceAction> actions{};
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 127)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::TriggerOrStopSlot);
    VERIFY_ARE_EQUAL(uint32_t{ 4 }, actions[0].trackIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, actions[0].sceneIndex);

    // Pad 38 (topDownRow=5 -> scene 5 >= 3, col 7 -> track 11 >= 10) is unbacked
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 38, 127)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());
}

void LaunchpadXE2ETests::T3_10_SysEx7AndChannelVoiceMixedFeedback()
{
    Log::Comment(L"T3-10: Mixed Bulk SysEx 03 + Channel Voice LEDs with Interleaved Type 0x4 & 0x2 Inputs");
    TestLaunchpadOutput output{};
    LaunchpadXSurface surface{};
    SurfaceStateSnapshot snap{};

    auto const sysexPackets = BuildLedSysExPackets(0, surface.BuildLedFeedback(snap, true));
    auto const cvFlash = BuildChannelLedPacket(0, 81, true, LedBehavior::FlashingPalette, 13);
    auto const cvPulse = BuildChannelLedPacket(0, ButtonRecord, false, LedBehavior::PulsingPalette, 5);

    VERIFY_IS_TRUE(output.SendUmpPackets(sysexPackets));
    std::array<UmpPacket, 2> const cvSpan{ cvFlash, cvPulse };
    VERIFY_IS_TRUE(output.SendUmpPackets(cvSpan));

    auto const msgs = DepacketizeSysEx7Messages(output.All());
    VERIFY_ARE_EQUAL(size_t{ 1 }, msgs.size());

    auto const evA = ParseTwoWords(Midi2NoteOnWords(0, 0, 11, 0xC000u));
    auto const evB = ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127));
    auto const evC = ParseTwoWords(Midi2NoteOffWords(0, 0, 11, 0));
    VERIFY_IS_TRUE(evA.kind == InputEventKind::PadPress);
    VERIFY_IS_TRUE(evB.kind == InputEventKind::ButtonPress);
    VERIFY_IS_TRUE(evC.kind == InputEventKind::PadRelease);
}

void LaunchpadXE2ETests::T3_11_NoteModePolyAftertouchAndTrackMuteSolo()
{
    Log::Comment(L"T3-11: Note Mode + PolyAftertouch while Track is Muted");
    auto doc = BuildGridSequence(1, 1, false);
    doc.Tracks[0].Muted = true;

    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    f.Engine.LaunchClip(L"t-0", doc.Tracks[0].Slots[0], 0, 0);
    RunUntil(f, At(Bar));
    VERIFY_ARE_EQUAL(size_t{ 0 }, NoteOns(f.Output.All()).size());

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);
    auto const snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), L"t-0", 0, true, false, false, L"", 0, {});

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 90)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xA, 0, 11, 85)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 3 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::EmitNoteOn);
    VERIFY_IS_TRUE(actions[1].kind == SurfaceActionKind::EmitPolyAftertouch);
    VERIFY_IS_TRUE(actions[2].kind == SurfaceActionKind::EmitNoteOff);
}

void LaunchpadXE2ETests::T3_12_DeviceInquiryModeSwitchAndDiffCache()
{
    Log::Comment(L"T3-12: Device Inquiry -> 81-LED Initial -> 1-LED Diff -> ModeSwitch 81-LED Reset");
    auto doc = BuildGridSequence(2, 2, false);
    LaunchpadXSurface surface{};
    TestLaunchpadOutput output{};

    std::array<uint8_t, 17> const reply{
        0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x20u, 0x29u,
        0x13u, 0x01u, 0x00u, 0x00u, 0x30u, 0x33u, 0x35u, 0x31u, 0xF7u
    };
    InputEvent ev{};
    SysEx7Accumulator acc{};
    VERIFY_IS_TRUE(TryParseSysExPackets(PacketizeFramedSysEx(0, reply), ev, acc));
    VERIFY_IS_TRUE(ev.kind == InputEventKind::DeviceInquiryReply && !ev.isBootloader);

    std::vector<TrackLaunchView> views(2);
    views[0].TrackId = L"t-0";
    views[1].TrackId = L"t-1";

    auto const snap1 = BuildSurfaceSnapshot(doc, views, L"t-0", 0, false, false, false, L"", 0, {});
    auto const initial81 = surface.BuildLedFeedback(snap1, true);
    VERIFY_ARE_EQUAL(size_t{ 81 }, initial81.size());
    VERIFY_IS_TRUE(output.SendUmpPackets(BuildLedSysExPackets(0, initial81)));

    views[0].Mode = TrackPlayMode::Clip;
    views[0].ClipId = doc.Tracks[0].Slots[0];
    views[0].Slot = 0;
    auto const snap2 = BuildSurfaceSnapshot(doc, views, L"t-0", 0, true, false, false, L"", 0, {});
    auto const diff1 = surface.BuildLedFeedback(snap2, false);
    VERIFY_IS_TRUE(!diff1.empty() && diff1.size() <= 4);

    std::vector<SurfaceAction> releases{};
    surface.SetMode(ControllerMode::Note, releases);
    auto const modeSwitch81 = surface.BuildLedFeedback(snap2, false);
    VERIFY_ARE_EQUAL(size_t{ 81 }, modeSwitch81.size());
    auto const sysexMsgs = DepacketizeSysEx7Messages(BuildLedSysExPackets(0, modeSwitch81));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sysexMsgs.size());
}

void LaunchpadXE2ETests::T4_01_LiveClipLaunchingSet()
{
    Log::Comment(L"T4-01: End-to-End Live Clip-Launching Set (4x4 Sequence)");
    auto doc = BuildGridSequence(4, 4, false);
    auto const order = RemoteTrackOrder(doc);
    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));

    TestLaunchpadOutput lpOut{};
    LaunchpadXSurface surface{};
    bool recording{ false };

    // Connect handshake + initial 81 LEDs
    VERIFY_IS_TRUE(lpOut.SendUmpPackets(BuildDeviceInquiryPackets(0)));
    VERIFY_IS_TRUE(lpOut.SendUmpPackets(PacketizeSysEx7(0, BuildProgrammerModeSysEx(true))));
    auto snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, false, recording, false, L"", 0, {});
    VERIFY_IS_TRUE(lpOut.SendUmpPackets(BuildLedSysExPackets(0, surface.BuildLedFeedback(snap, true))));

    // Press Scene 89 (Scene 0)
    f.Engine.Play(0);
    RunUntil(f, At(120));
    std::vector<SurfaceAction> actions{};
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, SceneButtonIndex(0), 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    RunUntil(f, At(480));

    for (auto const& v : f.Engine.LaunchState())
    {
        VERIFY_ARE_EQUAL(int32_t{ 0 }, v.Slot);
        VERIFY_IS_TRUE(v.Mode == TrackPlayMode::Clip);
    }

    snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});
    VERIFY_IS_TRUE(lpOut.SendUmpPackets(BuildLedSysExPackets(0, surface.BuildLedFeedback(snap, false))));

    // Queue Bass (track 1) Scene 1 (pad 72) with -1 bar quantize
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 72, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, -1, recording);
    VERIFY_IS_TRUE(f.Engine.LaunchState()[1].Pending);

    RunUntil(f, At(Bar + 480));
    VERIFY_ARE_EQUAL(int32_t{ 1 }, f.Engine.LaunchState()[1].Slot);

    // Stop Bass (72), then StopAll (97)
    actions.clear();
    snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[1], 1, true, recording, false, L"", 0, {});
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 72, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    RunUntil(f, At(Bar + 720));
    VERIFY_IS_TRUE(f.Engine.LaunchState()[1].Mode == TrackPlayMode::Stopped);

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonCustom, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    VERIFY_IS_FALSE(f.Engine.IsPlaying());
    VERIFY_IS_TRUE(!NoteOns(f.Output.All()).empty());
    VERIFY_IS_TRUE(!DepacketizeSysEx7Messages(lpOut.All()).empty());
}

void LaunchpadXE2ETests::T4_02_ScaleLockedMelodyRecordingAndPlayback()
{
    Log::Comment(L"T4-02: End-to-End Scale-Locked D Dorian Melody Recording & Playback");
    Sequence doc = BuildGridSequence(1, 1, false);
    doc.Tracks[0].Slots[0].clear();
    doc.Clips.clear();
    doc.Harmony = HarmonyTrack{
        { KeyChange{ 0, 2, midiapp::ScaleKind::Dorian } },
        {},
        false,
        nullptr
    };
    NormalizeSequence(doc);

    auto const k0 = SongKeyAt(doc, 0);
    VERIFY_IS_TRUE(k0.has_value());

    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), SurfaceStateSnapshot{}, actions);
    NoteLayoutConfig const cfg{ k0->Root, k0->Scale, 4, true, 5 };
    surface.SetNoteConfig(cfg, actions);

    SurfaceStateSnapshot snap{};
    snap.noteConfig = cfg;
    auto const leds = surface.BuildLedFeedback(snap, true);
    VerifyAll81LedIndicesPresent(leds);

    // Record 4-note melody on pads 11..14 with poly aftertouch into RecordingTake
    RecordingTake take{ 0 };
    std::vector<uint8_t> recordedPitches{};
    for (uint8_t i = 0; i < 4; ++i)
    {
        uint8_t const pad = static_cast<uint8_t>(11 + i);
        int64_t const startTick = static_cast<int64_t>(i) * TicksPerQuarterNote;
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, pad, 100)), snap, actions);
        VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
        recordedPitches.push_back(actions[0].midiNote);

        Note note{};
        note.Number = actions[0].midiNote;
        note.Velocity = static_cast<uint16_t>(ScaleUp(actions[0].velocity, 7, 16));
        uint32_t words[2]{};
        BuildNoteOn(note, 0, words);
        take.Add(startTick, words, 2);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xA, 0, pad, 64)), snap, actions);
        auto const atWords = Midi2PolyAftertouchWords(0, 0, actions[0].midiNote, ScaleUp(64, 7, 32));
        take.Add(startTick + 120, atWords.data(), 2);

        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, pad, 0)), snap, actions);
        BuildNoteOff(note, 0, words);
        take.Add(startTick + 480, words, 2);
    }

    auto const expectedDorian = ScaleNotes(2, midiapp::ScaleKind::Dorian, 74, 127);
    for (size_t i = 0; i < 4; ++i)
    {
        VERIFY_ARE_EQUAL(expectedDorian[i], recordedPitches[i]);
    }

    auto recordedClip = take.FinishLoop(Bar, L"c-dorian", L"Dorian Take", L"");
    VERIFY_ARE_EQUAL(size_t{ 4 }, recordedClip.Notes.size());
    VERIFY_ARE_EQUAL(size_t{ 4 }, recordedClip.Events.size());
    doc.Clips.push_back(recordedClip);
    doc.Tracks[0].Slots[0] = recordedClip.Id;
    NormalizeSequence(doc);

    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    f.Engine.LaunchClipAt(L"t-0", recordedClip.Id, 0, 0);
    RunUntil(f, At(Bar));

    // The engine schedules ahead, so the loop's next first note may already be queued for At(Bar).
    // Count only the first pass.
    auto ons = NoteOns(f.Output.All());
    std::erase_if(ons, [](SentMessage const& m) { return m.Timestamp >= At(Bar); });
    VERIFY_ARE_EQUAL(size_t{ 4 }, ons.size());
    for (size_t i = 0; i < 4; ++i)
    {
        VERIFY_ARE_EQUAL(recordedPitches[i], NoteNumberFromMessage(ons[i]));
    }
}

void LaunchpadXE2ETests::T4_03_MidSetClipNotePerformanceSwitching()
{
    Log::Comment(L"T4-03: Mid-Set Clip <-> Note Performance Switching Without Stuck Notes");
    auto doc = BuildGridSequence(2, 2, false);
    auto const order = RemoteTrackOrder(doc);
    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    f.Engine.LaunchClip(order[0], doc.Tracks[0].Slots[0], 0, 0);
    f.Engine.LaunchClip(order[1], doc.Tracks[1].Slots[0], 0, 0);
    RunUntil(f, At(Bar));

    LaunchpadXSurface surface{};
    bool recording{ false };
    std::vector<SurfaceAction> actions{};
    auto snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});

    // Enter Note mode and hold chord 11, 13, 15
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonNote, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Note);
    VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, false));

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 13, 100)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 15, 100)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 3 }, actions.size());

    // Switch back to Clip while still holding 11, 13, 15
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonSession, 127)), snap, actions);
    VERIFY_IS_TRUE(surface.Mode() == ControllerMode::Clip);
    size_t const released = static_cast<size_t>(std::count_if(actions.begin(), actions.end(), [](SurfaceAction const& a)
    {
        return a.kind == SurfaceActionKind::EmitNoteOff;
    }));
    VERIFY_ARE_EQUAL(size_t{ 3 }, released);

    // Physical releases in Clip mode do not launch/stop clips
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 11, 0)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 13, 0)), snap, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x8, 0, 15, 0)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());

    // Stop track 0 via pad 81, then StopAll via 97
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 81, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    RunUntil(f, At(Bar + 240));
    VERIFY_IS_TRUE(f.Engine.LaunchState()[0].Mode == TrackPlayMode::Stopped);

    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonCustom, 127)), snap, actions);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    VERIFY_IS_FALSE(f.Engine.IsPlaying());
}

void LaunchpadXE2ETests::T4_04_MultiTrackPagedSessionNavigation()
{
    Log::Comment(L"T4-04: Multi-Track 12x12 Paged Session Navigation with Folders & Pinned Track");
    auto doc = BuildGridSequence(12, 12, true);
    auto const order = RemoteTrackOrder(doc);
    VERIFY_ARE_EQUAL(size_t{ 12 }, order.size());

    EngineFixture f{};
    f.Engine.SetSequence(std::make_shared<Sequence const>(doc));
    f.Engine.Play(0);
    RunUntil(f, At(120));

    LaunchpadXSurface surface{};
    bool recording{ false };
    auto snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});
    VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));

    // Scroll Right x4 and Down x4 to (4, 4)
    std::vector<SurfaceAction> actions{};
    for (int i = 0; i < 4; ++i)
    {
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonRight, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonDown, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    }
    VERIFY_ARE_EQUAL(uint32_t{ 4 }, surface.TrackOffset());
    VERIFY_ARE_EQUAL(uint32_t{ 4 }, surface.SceneOffset());

    // Pad 18 (bottom-right of 8x8) -> track 11, scene 11
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 18, 127)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::TriggerOrStopSlot);
    VERIFY_ARE_EQUAL(uint32_t{ 11 }, actions[0].trackIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 11 }, actions[0].sceneIndex);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    RunUntil(f, At(240));
    // LaunchState() follows the document's track list, which includes folders, so look the
    // track up by id rather than by its position in RemoteTrackOrder.
    {
        auto const views = f.Engine.LaunchState();
        auto const view = std::find_if(views.begin(), views.end(), [&order](TrackLaunchView const& v) { return v.TrackId == order[11]; });
        VERIFY_IS_TRUE(view != views.end());
        VERIFY_ARE_EQUAL(int32_t{ 11 }, view->Slot);
    }

    // Scene button 19 (bottom right-column button = local row 7 -> global scene 11)
    actions.clear();
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, SceneButtonIndex(7), 127)), snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_IS_TRUE(actions[0].kind == SurfaceActionKind::LaunchScene);
    VERIFY_ARE_EQUAL(uint32_t{ 11 }, actions[0].sceneIndex);
    DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    RunUntil(f, At(480));

    // Scroll back to (0, 0) and verify playing state preserved across off-screen tracks
    for (int i = 0; i < 4; ++i)
    {
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonLeft, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
        actions.clear();
        surface.HandleInput(ParseSingleWord(Midi1Word(0, 0xB, 0, ButtonUp, 127)), snap, actions);
        DispatchClipActionsToEngine(doc, order, actions, surface, f.Engine, 0, recording);
    }
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.TrackOffset());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.SceneOffset());
    snap = BuildSurfaceSnapshot(doc, f.Engine.LaunchState(), order[0], 0, true, recording, false, L"", 0, {});

    // Everything launched is off screen at (0, 0), so the pads already show the right frame and the
    // differential update is empty. A forced refresh still repaints all 81 LEDs.
    VERIFY_ARE_EQUAL(size_t{ 0 }, surface.BuildLedFeedback(snap, false).size());
    VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));
}

void LaunchpadXE2ETests::T4_05_ReconnectBootloaderAndErrorRecovery()
{
    Log::Comment(L"T4-05: Bootloader Detection, App Reconnect, Malformed UMP Burst, Resync & Clean Exit 0E 00");
    TestLaunchpadOutput output{};
    LaunchpadXSurface surface{};
    SurfaceStateSnapshot snap{};

    // 1. Bootloader Device Inquiry
    std::array<uint8_t, 17> const bootReply{
        0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x20u, 0x29u,
        0x13u, 0x11u, 0x00u, 0x00u, 0x30u, 0x31u, 0x30u, 0x30u, 0xF7u
    };
    InputEvent ev{};
    SysEx7Accumulator acc{};
    VERIFY_IS_TRUE(TryParseSysExPackets(PacketizeFramedSysEx(0, bootReply), ev, acc));
    VERIFY_IS_TRUE(ev.kind == InputEventKind::DeviceInquiryReply && ev.isBootloader);

    // 2. Reconnect to App mode -> 0E 01 + 81-LED sync
    std::array<uint8_t, 17> const appReply{
        0xF0u, 0x7Eu, 0x00u, 0x06u, 0x02u, 0x00u, 0x20u, 0x29u,
        0x13u, 0x01u, 0x00u, 0x00u, 0x30u, 0x33u, 0x35u, 0x31u, 0xF7u
    };
    VERIFY_IS_TRUE(TryParseSysExPackets(PacketizeFramedSysEx(0, appReply), ev, acc));
    VERIFY_IS_TRUE(ev.kind == InputEventKind::DeviceInquiryReply && !ev.isBootloader);
    VERIFY_IS_TRUE(output.SendUmpPackets(PacketizeSysEx7(0, BuildProgrammerModeSysEx(true))));
    VERIFY_IS_TRUE(output.SendUmpPackets(BuildLedSysExPackets(0, surface.BuildLedFeedback(snap, true))));

    // 3. Inject malformed UMP burst & out-of-range events
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);
    surface.HandleInput(ParseSingleWord(Midi1Word(0, 0x9, 0, 11, 100)), snap, actions);

    uint32_t const trunc4[1]{ 0x40900B00u };
    VERIFY_IS_FALSE(TryParseInputUmp(trunc4, 1, ev));
    std::array<uint32_t, 2> const orphanEnd{ 0x30331122u, 0x33000000u };
    VERIFY_IS_FALSE(TryParseInputUmp(orphanEnd.data(), 2, ev));
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, LogoLed, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 0 }, actions.size());

    // 4. Forced 81-LED resync & clean disconnect releasing held note + sending 0E 00
    VerifyAll81LedIndicesPresent(surface.BuildLedFeedback(snap, true));
    std::vector<SurfaceAction> releases{};
    surface.SetMode(ControllerMode::Clip, releases);
    VERIFY_ARE_EQUAL(size_t{ 1 }, releases.size());
    VERIFY_IS_TRUE(releases[0].kind == SurfaceActionKind::EmitNoteOff);
    VERIFY_IS_TRUE(output.SendUmpPackets(PacketizeSysEx7(0, BuildProgrammerModeSysEx(false))));

    auto const msgs = DepacketizeSysEx7Messages(output.All());
    std::array<uint8_t, 7> const enter{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x01u };
    std::array<uint8_t, 7> const exit{ 0x00u, 0x20u, 0x29u, 0x02u, 0x0Cu, 0x0Eu, 0x00u };
    VERIFY_IS_TRUE(ContainsSysExSequence(msgs, enter));
    VERIFY_IS_TRUE(ContainsSysExSequence(msgs, exit));
}
