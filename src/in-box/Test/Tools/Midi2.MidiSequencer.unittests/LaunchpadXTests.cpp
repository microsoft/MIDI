// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXTests.h"

#include "LaunchpadXProtocol.h"
#include "LaunchpadXSurface.h"

#include <algorithm>
#include <array>
#include <set>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;
using namespace midisequencer::launchpad;

namespace
{
    [[nodiscard]] std::vector<uint8_t> DepacketizeSysEx7(_In_ std::span<const UmpPacket> packets, _Out_ uint8_t& outGroup)
    {
        outGroup = 0;
        SysEx7Accumulator accumulator{};
        std::vector<uint8_t> assembled{};
        bool completed = false;

        for (auto const& packet : packets)
        {
            if (packet.wordCount < 2)
            {
                return {};
            }
            if (accumulator.Feed(packet.words[0], packet.words[1], outGroup, assembled))
            {
                completed = true;
            }
        }

        return completed ? assembled : std::vector<uint8_t>{};
    }

    [[nodiscard]] uint32_t Midi1Word(
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

    [[nodiscard]] std::array<uint32_t, 2> Midi2Words(
        _In_ uint8_t group,
        _In_ uint8_t status,
        _In_ uint8_t channel,
        _In_ uint8_t index1,
        _In_ uint8_t index2,
        _In_ uint32_t word1) noexcept
    {
        uint32_t const word0 =
            0x40000000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (static_cast<uint32_t>(status & 0x0Fu) << 20) |
            (static_cast<uint32_t>(channel & 0x0Fu) << 16) |
            (static_cast<uint32_t>(index1 & 0x7Fu) << 8) |
            static_cast<uint32_t>(index2 & 0x7Fu);
        return { word0, word1 };
    }

    [[nodiscard]] const LedSpec* FindLedSpec(_In_ const std::vector<LedSpec>& specs, _In_ uint8_t index) noexcept
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

    [[nodiscard]] SurfaceStateSnapshot MakeClipSnapshot(
        _In_ uint32_t tracks,
        _In_ uint32_t scenes)
    {
        SurfaceStateSnapshot snap{};
        snap.totalTracks = tracks;
        snap.totalScenes = scenes;
        snap.activeTrackColorRgb24 = 0x4080FFu;
        snap.gridClips.resize(static_cast<size_t>(tracks) * scenes);
        return snap;
    }
}

void LaunchpadXTests::ProgrammerModeAndLayoutSysExEncodeToValidSysEx7()
{
    auto const enterPayload = BuildProgrammerModeSysEx(true);
    std::vector<uint8_t> const expectedEnter{ 0x00, 0x20, 0x29, 0x02, 0x0C, 0x0E, 0x01 };
    VERIFY_IS_TRUE(enterPayload == expectedEnter);

    auto const exitPayload = BuildProgrammerModeSysEx(false);
    std::vector<uint8_t> const expectedExit{ 0x00, 0x20, 0x29, 0x02, 0x0C, 0x0E, 0x00 };
    VERIFY_IS_TRUE(exitPayload == expectedExit);

    auto const layoutPayload = BuildSelectProgrammerLayoutSysEx();
    std::vector<uint8_t> const expectedLayout{ 0x00, 0x20, 0x29, 0x02, 0x0C, 0x00, 0x7F };
    VERIFY_IS_TRUE(layoutPayload == expectedLayout);

    auto const enterPackets = BuildProgrammerModePackets(3, true);
    VERIFY_ARE_EQUAL(size_t{ 2 }, enterPackets.size());
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, enterPackets[0].wordCount);
    VERIFY_ARE_EQUAL(uint32_t{ 0x3u }, enterPackets[0].words[0] >> 28);
    VERIFY_ARE_EQUAL(uint32_t{ 3u }, (enterPackets[0].words[0] >> 24) & 0x0Fu);
    VERIFY_ARE_EQUAL(uint32_t{ 1u }, (enterPackets[0].words[0] >> 20) & 0x0Fu);
    VERIFY_ARE_EQUAL(uint32_t{ 6u }, (enterPackets[0].words[0] >> 16) & 0x0Fu);
    VERIFY_ARE_EQUAL(uint32_t{ 3u }, (enterPackets[1].words[0] >> 20) & 0x0Fu);
    VERIFY_ARE_EQUAL(uint32_t{ 1u }, (enterPackets[1].words[0] >> 16) & 0x0Fu);

    uint8_t group = 0;
    auto const decodedEnter = DepacketizeSysEx7(enterPackets, group);
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, group);
    VERIFY_IS_TRUE(decodedEnter == expectedEnter);

    auto const decodedLayout = DepacketizeSysEx7(BuildSelectProgrammerLayoutPackets(5), group);
    VERIFY_ARE_EQUAL(uint8_t{ 5 }, group);
    VERIFY_IS_TRUE(decodedLayout == expectedLayout);
}

void LaunchpadXTests::DeviceInquiryRequestAndReplyDecodeAppAndBootloader()
{
    auto const reqPackets = BuildDeviceInquiryPackets(2);
    VERIFY_ARE_EQUAL(size_t{ 1 }, reqPackets.size());
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, reqPackets[0].wordCount);
    VERIFY_ARE_EQUAL(uint32_t{ 0x32047E7Fu }, reqPackets[0].words[0]);
    VERIFY_ARE_EQUAL(uint32_t{ 0x06010000u }, reqPackets[0].words[1]);

    // App mode reply (13 01), 15 bytes -> 3 UMP Type 0x3 packets
    std::vector<uint8_t> const appReply{
        0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x13, 0x01, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04
    };
    auto const appPackets = PacketizeSysEx7(0, appReply);
    VERIFY_ARE_EQUAL(size_t{ 3 }, appPackets.size());

    SysEx7Accumulator acc{};
    InputEvent evt{};
    VERIFY_IS_FALSE(TryParseInputUmp(appPackets[0].words, appPackets[0].wordCount, evt, &acc));
    VERIFY_IS_FALSE(TryParseInputUmp(appPackets[1].words, appPackets[1].wordCount, evt, &acc));
    VERIFY_IS_TRUE(TryParseInputUmp(appPackets[2].words, appPackets[2].wordCount, evt, &acc));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::DeviceInquiryReply), static_cast<uint8_t>(evt.kind));
    VERIFY_IS_FALSE(evt.isBootloader);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, evt.version[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, evt.version[1]);
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, evt.version[2]);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, evt.version[3]);

    // Bootloader mode reply (13 11)
    std::vector<uint8_t> bootReply = appReply;
    bootReply[8] = 0x11;
    bootReply[11] = 0x00;
    bootReply[14] = 0x09;
    auto const bootPackets = PacketizeSysEx7(1, bootReply);
    VERIFY_ARE_EQUAL(size_t{ 3 }, bootPackets.size());
    VERIFY_IS_FALSE(TryParseInputUmp(bootPackets[0].words, bootPackets[0].wordCount, evt, &acc));
    VERIFY_IS_FALSE(TryParseInputUmp(bootPackets[1].words, bootPackets[1].wordCount, evt, &acc));
    VERIFY_IS_TRUE(TryParseInputUmp(bootPackets[2].words, bootPackets[2].wordCount, evt, &acc));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::DeviceInquiryReply), static_cast<uint8_t>(evt.kind));
    VERIFY_IS_TRUE(evt.isBootloader);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, evt.version[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 9 }, evt.version[3]);

    // Non-Novation manufacturer ID should be rejected
    std::vector<uint8_t> otherMfg = appReply;
    otherMfg[5] = 0x42;
    auto const otherPackets = PacketizeSysEx7(0, otherMfg);
    bool parsedOther = false;
    for (auto const& pkt : otherPackets)
    {
        if (TryParseInputUmp(pkt.words, pkt.wordCount, evt, &acc))
        {
            parsedOther = true;
        }
    }
    VERIFY_IS_FALSE(parsedOther);

    // Truncated reply should be rejected
    std::vector<uint8_t> shortReply(appReply.begin(), appReply.begin() + 10);
    auto const shortPackets = PacketizeSysEx7(0, shortReply);
    bool parsedShort = false;
    for (auto const& pkt : shortPackets)
    {
        if (TryParseInputUmp(pkt.words, pkt.wordCount, evt, &acc))
        {
            parsedShort = true;
        }
    }
    VERIFY_IS_FALSE(parsedShort);
}

void LaunchpadXTests::BulkLedSysExSupportsAllTypesClampsRgbAndChunksAt81()
{
    auto const color = RgbColor::FromRgb24(0xFF8001u);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, color.r);
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, color.g);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, color.b);

    VERIFY_IS_TRUE(BuildLedSysExPackets(0, {}).empty());

    std::vector<LedSpec> mixed{
        LedSpec{ 11, LedBehavior::StaticPalette, 21, 0, {} },
        LedSpec{ 12, LedBehavior::FlashingPalette, 5, 9, {} },
        LedSpec{ 13, LedBehavior::PulsingPalette, 45, 0, {} },
        LedSpec{ 14, LedBehavior::Rgb, 0, 0, RgbColor{ 127, 64, 1 } },
    };

    auto const packets = BuildLedSysExPackets(4, mixed);
    uint8_t group = 0;
    auto const payload = DepacketizeSysEx7(packets, group);
    VERIFY_ARE_EQUAL(uint8_t{ 4 }, group);

    std::vector<uint8_t> const expected{
        0x00, 0x20, 0x29, 0x02, 0x0C, 0x03,
        0x00, 11, 21,
        0x01, 12, 9, 5,
        0x02, 13, 45,
        0x03, 14, 127, 64, 1
    };
    VERIFY_IS_TRUE(payload == expected);

    // Chunking boundary: 81 specs fit in 1 SysEx message; 82 specs split into 2 SysEx messages
    std::vector<LedSpec> specs81(81, LedSpec{ 11, LedBehavior::Rgb, 0, 0, RgbColor{ 10, 20, 30 } });
    auto const packets81 = BuildLedSysExPackets(0, specs81);
    size_t sysexEndCount81 = 0;
    for (auto const& pkt : packets81)
    {
        auto const st = (pkt.words[0] >> 20) & 0x0Fu;
        if (st == 0 || st == 3)
        {
            ++sysexEndCount81;
        }
    }
    VERIFY_ARE_EQUAL(size_t{ 1 }, sysexEndCount81);

    std::vector<LedSpec> specs82(82, LedSpec{ 11, LedBehavior::Rgb, 0, 0, RgbColor{ 10, 20, 30 } });
    auto const packets82 = BuildLedSysExPackets(0, specs82);
    size_t sysexEndCount82 = 0;
    for (auto const& pkt : packets82)
    {
        auto const st = (pkt.words[0] >> 20) & 0x0Fu;
        if (st == 0 || st == 3)
        {
            ++sysexEndCount82;
        }
    }
    VERIFY_ARE_EQUAL(size_t{ 2 }, sysexEndCount82);
}

void LaunchpadXTests::ChannelVoiceLedPacketsEncodeChannelsAndStatuses()
{
    auto const padStatic = BuildChannelLedPacket(2, 11, true, LedBehavior::StaticPalette, 21);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, padStatic.wordCount);
    VERIFY_ARE_EQUAL(Midi1Word(2, 0x9, 0, 11, 21), padStatic.words[0]);

    auto const padFlash = BuildChannelLedPacket(2, 12, true, LedBehavior::FlashingPalette, 5);
    VERIFY_ARE_EQUAL(Midi1Word(2, 0x9, 1, 12, 5), padFlash.words[0]);

    auto const padPulse = BuildChannelLedPacket(2, 13, true, LedBehavior::PulsingPalette, 45);
    VERIFY_ARE_EQUAL(Midi1Word(2, 0x9, 2, 13, 45), padPulse.words[0]);

    auto const btnStatic = BuildChannelLedPacket(7, 95, false, LedBehavior::StaticPalette, 127);
    VERIFY_ARE_EQUAL(Midi1Word(7, 0xB, 0, 95, 127), btnStatic.words[0]);

    auto const btnFlash = BuildChannelLedPacket(7, 98, false, LedBehavior::FlashingPalette, 5);
    VERIFY_ARE_EQUAL(Midi1Word(7, 0xB, 1, 98, 5), btnFlash.words[0]);

    auto const btnPulse = BuildChannelLedPacket(7, 89, false, LedBehavior::PulsingPalette, 13);
    VERIFY_ARE_EQUAL(Midi1Word(7, 0xB, 2, 89, 13), btnPulse.words[0]);
}

void LaunchpadXTests::SysEx7RoundTripHandlesEmptyShortMultiPacketAndMalformed()
{
    for (size_t len : { size_t{ 0 }, size_t{ 1 }, size_t{ 6 }, size_t{ 7 }, size_t{ 12 }, size_t{ 13 }, size_t{ 410 } })
    {
        std::vector<uint8_t> payload(len);
        for (size_t i = 0; i < len; ++i)
        {
            payload[i] = static_cast<uint8_t>(i & 0x7Fu);
        }

        auto const packets = PacketizeSysEx7(6, payload);
        VERIFY_IS_FALSE(packets.empty());

        uint8_t outGroup = 0;
        auto const decoded = DepacketizeSysEx7(packets, outGroup);
        VERIFY_ARE_EQUAL(uint8_t{ 6 }, outGroup);
        VERIFY_IS_TRUE(decoded == payload);
    }

    // Reject payload containing non-7-bit byte
    std::vector<uint8_t> const invalidPayload{ 0x00, 0x80, 0x01 };
    VERIFY_IS_TRUE(PacketizeSysEx7(0, invalidPayload).empty());

    // Accumulator ignores non-Type-0x3, orphan Continue/End, resets on new Start, and rejects >7-bit bytes
    SysEx7Accumulator acc{};
    uint8_t group = 0;
    std::vector<uint8_t> out{};

    VERIFY_IS_FALSE(acc.Feed(Midi1Word(0, 0x9, 0, 11, 100), 0, group, out));
    VERIFY_IS_FALSE(acc.Feed(0x30210100u, 0, group, out)); // Continue without Start
    VERIFY_IS_FALSE(acc.Feed(0x30310100u, 0, group, out)); // End without Start

    // Start, then malformed >7-bit Continue drops state
    VERIFY_IS_FALSE(acc.Feed(0x30160102u, 0x03040506u, group, out));
    VERIFY_IS_FALSE(acc.Feed(0x30218000u, 0, group, out));
    VERIFY_IS_FALSE(acc.Feed(0x30310700u, 0, group, out));

    // Unexpected Start resets prior incomplete Start cleanly
    VERIFY_IS_FALSE(acc.Feed(0x30160102u, 0x03040506u, group, out));
    VERIFY_IS_FALSE(acc.Feed(0x30121112u, 0, group, out));
    VERIFY_IS_TRUE(acc.Feed(0x30311300u, 0, group, out));
    std::vector<uint8_t> const expectedReset{ 0x11, 0x12, 0x13 };
    VERIFY_IS_TRUE(out == expectedReset);
}

void LaunchpadXTests::InputParserHandlesMidi1AndMidi2PadsButtonsAndReleases()
{
    InputEvent evt{};

    // MIDI 1.0 Type 0x2
    uint32_t w = Midi1Word(0, 0x9, 0, 11, 100);
    VERIFY_IS_TRUE(TryParseInputUmp(&w, 1, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadPress), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, evt.index);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, evt.value);

    w = Midi1Word(0, 0x9, 0, 11, 0);
    VERIFY_IS_TRUE(TryParseInputUmp(&w, 1, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadRelease), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, evt.index);

    w = Midi1Word(0, 0x8, 0, 88, 0);
    VERIFY_IS_TRUE(TryParseInputUmp(&w, 1, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadRelease), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 88 }, evt.index);

    w = Midi1Word(0, 0xA, 0, 44, 85);
    VERIFY_IS_TRUE(TryParseInputUmp(&w, 1, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadAftertouch), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 44 }, evt.index);
    VERIFY_ARE_EQUAL(uint8_t{ 85 }, evt.value);

    w = Midi1Word(0, 0xB, 0, 95, 127);
    VERIFY_IS_TRUE(TryParseInputUmp(&w, 1, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::ButtonPress), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 95 }, evt.index);

    w = Midi1Word(0, 0xB, 0, 95, 0);
    VERIFY_IS_TRUE(TryParseInputUmp(&w, 1, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::ButtonRelease), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 95 }, evt.index);

    // Out-of-grid notes and out-of-range buttons rejected
    for (uint8_t badNote : { uint8_t{ 0 }, uint8_t{ 10 }, uint8_t{ 19 }, uint8_t{ 99 }, uint8_t{ 127 } })
    {
        w = Midi1Word(0, 0x9, 0, badNote, 100);
        VERIFY_IS_FALSE(TryParseInputUmp(&w, 1, evt));
    }

    // MIDI 2.0 Type 0x4
    auto m2 = Midi2Words(0, 0x9, 0, 11, 0, 0x80000000u);
    VERIFY_IS_TRUE(TryParseInputUmp(m2.data(), 2, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadPress), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, evt.index);
    VERIFY_IS_TRUE(evt.value > 0);

    // Small non-zero 16-bit velocity still produces PadPress >= 1
    m2 = Midi2Words(0, 0x9, 0, 11, 0, 0x00010000u);
    VERIFY_IS_TRUE(TryParseInputUmp(m2.data(), 2, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadPress), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, evt.value);

    m2 = Midi2Words(0, 0x9, 0, 11, 0, 0x00000000u);
    VERIFY_IS_TRUE(TryParseInputUmp(m2.data(), 2, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadRelease), static_cast<uint8_t>(evt.kind));

    m2 = Midi2Words(0, 0x8, 0, 88, 0, 0x00000000u);
    VERIFY_IS_TRUE(TryParseInputUmp(m2.data(), 2, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadRelease), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 88 }, evt.index);

    m2 = Midi2Words(0, 0xA, 0, 44, 0, 0x80000000u);
    VERIFY_IS_TRUE(TryParseInputUmp(m2.data(), 2, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::PadAftertouch), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 64 }, evt.value);

    m2 = Midi2Words(0, 0xB, 0, 96, 0, 0xFFFFFFFFu);
    VERIFY_IS_TRUE(TryParseInputUmp(m2.data(), 2, evt));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(InputEventKind::ButtonPress), static_cast<uint8_t>(evt.kind));
    VERIFY_ARE_EQUAL(uint8_t{ 96 }, evt.index);

    // Defensive null / truncated checks
    VERIFY_IS_FALSE(TryParseInputUmp(nullptr, 1, evt));
    VERIFY_IS_FALSE(TryParseInputUmp(m2.data(), 1, evt));
}

void LaunchpadXTests::EndpointNameMatcherMatchesLpxMidiAndRejectsLpxDaw()
{
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"LPX MIDI"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"MIDIIN2 (LPX MIDI)"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"MIDIOUT2 (LPX MIDI)"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"Novation Launchpad X LPX MIDI"));
    VERIFY_IS_TRUE(IsLaunchpadMidiEndpointName(L"Launchpad X"));

    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"LPX DAW"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"Launchpad X LPX DAW"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"MIDIIN (LPX DAW)"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L"General MIDI Synth"));
    VERIFY_IS_FALSE(IsLaunchpadMidiEndpointName(L""));
}

void LaunchpadXTests::GridAndSceneCoordinateHelpersMatchProgrammerSpec()
{
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, GridMin);
    VERIFY_ARE_EQUAL(uint8_t{ 88 }, GridMax);
    VERIFY_ARE_EQUAL(uint8_t{ 89 }, SceneButtonIndex(0));
    VERIFY_ARE_EQUAL(uint8_t{ 79 }, SceneButtonIndex(1));
    VERIFY_ARE_EQUAL(uint8_t{ 19 }, SceneButtonIndex(7));
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, SceneButtonIndex(8));

    VERIFY_ARE_EQUAL(uint8_t{ 91 }, ButtonUp);
    VERIFY_ARE_EQUAL(uint8_t{ 92 }, ButtonDown);
    VERIFY_ARE_EQUAL(uint8_t{ 93 }, ButtonLeft);
    VERIFY_ARE_EQUAL(uint8_t{ 94 }, ButtonRight);
    VERIFY_ARE_EQUAL(uint8_t{ 95 }, ButtonSession);
    VERIFY_ARE_EQUAL(uint8_t{ 96 }, ButtonNote);
    VERIFY_ARE_EQUAL(uint8_t{ 97 }, ButtonCustom);
    VERIFY_ARE_EQUAL(uint8_t{ 98 }, ButtonRecord);
    VERIFY_ARE_EQUAL(uint8_t{ 99 }, LogoLed);

    std::set<uint8_t> allIndices{};
    for (uint8_t row = 0; row < GridRows; ++row)
    {
        for (uint8_t col = 0; col < GridCols; ++col)
        {
            auto const idx = GridPadIndex(row, col);
            VERIFY_IS_TRUE(IsGridPadIndex(idx));
            VERIFY_IS_TRUE(idx <= 127);
            uint8_t r = 0;
            uint8_t c = 0;
            VERIFY_IS_TRUE(PadRowColFromIndex(idx, r, c));
            VERIFY_ARE_EQUAL(row, r);
            VERIFY_ARE_EQUAL(col, c);
            allIndices.insert(idx);
        }

        auto const sceneIdx = SceneButtonIndex(row);
        VERIFY_IS_TRUE(IsSceneButtonIndex(sceneIdx));
        VERIFY_IS_TRUE(sceneIdx <= 127);
        allIndices.insert(sceneIdx);
    }

    for (uint8_t top = ButtonUp; top <= ButtonRecord; ++top)
    {
        VERIFY_IS_TRUE(IsTopRowButtonIndex(top));
        allIndices.insert(top);
    }

    allIndices.insert(LogoLed);
    VERIFY_ARE_EQUAL(size_t{ 81 }, allIndices.size());
}

void LaunchpadXTests::ClipModePadPressTriggersSlotsAndIgnoresOutOfBounds()
{
    LaunchpadXSurface surface{};
    auto const snap = MakeClipSnapshot(3, 4);
    std::vector<SurfaceAction> actions{};

    // Top-left pad 81 -> track 0, scene 0
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 81, 100, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::TriggerOrStopSlot), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, actions[0].trackIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, actions[0].sceneIndex);

    // Row 5 (topDownRow = 3), col 3 -> pad 53 -> track 2, scene 3
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 53, 100, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, actions[0].trackIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, actions[0].sceneIndex);

    // Out-of-bounds track (pad 84 -> track 3) and out-of-bounds scene (pad 11 -> scene 7)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 84, 100, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 100, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());

    // Pad release does nothing in Clip mode
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadRelease, 81, 0, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
}

void LaunchpadXTests::ClipModeSceneButtonsMapTopDown89To19()
{
    LaunchpadXSurface surface{};
    auto const snap = MakeClipSnapshot(4, 4);
    std::vector<SurfaceAction> actions{};

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, 89, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::LaunchScene), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, actions[0].sceneIndex);

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, 69, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, actions[0].sceneIndex);

    surface.SetViewport(0, 2);
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, 79, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, actions[0].sceneIndex);

    // Scene 4 out of bounds
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, 69, 127, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());

    // Release ignored
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonRelease, 89, 0, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
}

void LaunchpadXTests::ClipModeHugeViewportOffsetNeverWrapsAround()
{
    // An offset near the top of uint32_t would wrap around to a small index if it were added
    // before the bounds check, and a pad would then trigger a real slot.
    LaunchpadXSurface surface{};
    auto const snap = MakeClipSnapshot(4, 4);
    std::vector<SurfaceAction> actions{};

    surface.SetViewport(UINT32_MAX - 2u, UINT32_MAX - 2u);

    for (uint8_t const pad : { uint8_t{ 81 }, uint8_t{ 84 }, uint8_t{ 11 }, uint8_t{ 88 } })
    {
        actions.clear();
        surface.HandleInput(InputEvent{ InputEventKind::PadPress, pad, 100, false, {} }, snap, actions);
        VERIFY_IS_TRUE(actions.empty());
    }

    for (uint8_t const scene : { uint8_t{ 89 }, uint8_t{ 59 }, uint8_t{ 19 } })
    {
        actions.clear();
        surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, scene, 127, false, {} }, snap, actions);
        VERIFY_IS_TRUE(actions.empty());
    }

    // Every pad and scene button is dark, and there's nowhere further to scroll.
    auto const leds = surface.BuildLedFeedback(snap, true);
    VERIFY_ARE_EQUAL(size_t{ 81 }, leds.size());
    for (uint8_t const index : { uint8_t{ 81 }, uint8_t{ 11 }, uint8_t{ 89 }, uint8_t{ 19 }, ButtonDown, ButtonRight })
    {
        auto const* led = FindLedSpec(leds, index);
        VERIFY_IS_TRUE(led != nullptr);
        VERIFY_IS_TRUE(led->rgb == RgbColor{});
    }
}

void LaunchpadXTests::ClipModeNavigationButtonsScrollViewportWithClamping()
{
    LaunchpadXSurface surface{};
    auto const snap = MakeClipSnapshot(10, 10);
    std::vector<SurfaceAction> actions{};

    // Clamped at 0 for Left/Up
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonLeft, 127, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.TrackOffset());

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonUp, 127, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, surface.SceneOffset());

    // Scroll Right and Down
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonRight, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::SetViewport), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, surface.TrackOffset());

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonDown, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, surface.SceneOffset());

    // Clamp at max (10 - 8 = 2)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonRight, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, surface.TrackOffset());
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonRight, 127, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, surface.TrackOffset());

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonDown, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, surface.SceneOffset());
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonDown, 127, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, surface.SceneOffset());
}

void LaunchpadXTests::ClipModeStopAndRecordButtonsEmitExpectedActions()
{
    LaunchpadXSurface surface{};
    auto const snap = MakeClipSnapshot(2, 2);
    std::vector<SurfaceAction> actions{};

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonCustom, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::StopAll), static_cast<uint8_t>(actions[0].kind));

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonRecord, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::ToggleRecord), static_cast<uint8_t>(actions[0].kind));

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonRelease, ButtonCustom, 0, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonRelease, ButtonRecord, 0, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());
}

void LaunchpadXTests::ClipModeLedFeedbackEncodesStatesAndDiffsIncrementally()
{
    LaunchpadXSurface surface{};
    auto snap = MakeClipSnapshot(3, 2);
    // Scene 0: track 0 filled, track 1 playing, track 2 pending
    snap.gridClips[0] = ClipPadSnapshot{ true, false, false, false, 0xFF8A65u };
    snap.gridClips[1] = ClipPadSnapshot{ true, true, false, false, 0x4080FFu };
    snap.gridClips[2] = ClipPadSnapshot{ true, false, true, false, 0x4080FFu };
    // Scene 1: track 0 recording, track 1 empty, track 2 filled
    snap.gridClips[3] = ClipPadSnapshot{ true, false, false, true, 0x4080FFu };
    snap.gridClips[4] = ClipPadSnapshot{ false, false, false, false, 0x4080FFu };
    snap.gridClips[5] = ClipPadSnapshot{ true, false, false, false, 0x20C040u };

    auto const full = surface.BuildLedFeedback(snap, true);
    VERIFY_ARE_EQUAL(size_t{ 81 }, full.size());

    auto const* filledLed = FindLedSpec(full, 81);
    auto const* playingLed = FindLedSpec(full, 82);
    auto const* pendingLed = FindLedSpec(full, 83);
    auto const* recordingLed = FindLedSpec(full, 71);
    auto const* emptyLed = FindLedSpec(full, 72);
    auto const* outOfBoundsLed = FindLedSpec(full, 11);
    auto const* sessionLed = FindLedSpec(full, ButtonSession);
    auto const* rightNavLed = FindLedSpec(full, ButtonRight);

    VERIFY_IS_TRUE(filledLed != nullptr);
    VERIFY_IS_TRUE(playingLed != nullptr);
    VERIFY_IS_TRUE(pendingLed != nullptr);
    VERIFY_IS_TRUE(recordingLed != nullptr);
    VERIFY_IS_TRUE(emptyLed != nullptr);
    VERIFY_IS_TRUE(outOfBoundsLed != nullptr);
    VERIFY_IS_TRUE(sessionLed != nullptr);
    VERIFY_IS_TRUE(rightNavLed != nullptr);

    VERIFY_ARE_EQUAL(static_cast<uint8_t>(LedBehavior::Rgb), static_cast<uint8_t>(filledLed->behavior));
    VERIFY_IS_TRUE(filledLed->rgb == RgbColor::FromRgb24(0xFF8A65u));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(LedBehavior::PulsingPalette), static_cast<uint8_t>(playingLed->behavior));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(LedBehavior::FlashingPalette), static_cast<uint8_t>(pendingLed->behavior));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(LedBehavior::FlashingPalette), static_cast<uint8_t>(recordingLed->behavior));
    VERIFY_IS_TRUE(pendingLed->colorA != recordingLed->colorA);
    VERIFY_IS_TRUE(emptyLed->rgb != outOfBoundsLed->rgb);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, outOfBoundsLed->rgb.r);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, rightNavLed->rgb.r);
    VERIFY_IS_TRUE(sessionLed->rgb.g > 0);

    // No changes -> 0 differential LEDs
    VERIFY_IS_TRUE(surface.BuildLedFeedback(snap, false).empty());

    // Mutate only slot (track 0, scene 0) -> pad 81 updates alone
    snap.gridClips[0].playing = true;
    auto const diff = surface.BuildLedFeedback(snap, false);
    VERIFY_ARE_EQUAL(size_t{ 1 }, diff.size());
    VERIFY_ARE_EQUAL(uint8_t{ 81 }, diff[0].index);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(LedBehavior::PulsingPalette), static_cast<uint8_t>(diff[0].behavior));
}

void LaunchpadXTests::NoteModeChromaticAndScaleLayoutsMapPitchesAndRoots()
{
    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> releaseActions{};
    surface.SetMode(ControllerMode::Note, releaseActions);

    NoteLayoutConfig config{};
    config.rootNote = 0; // C
    config.scale = midiapp::ScaleKind::Major;
    config.octave = 3; // C3 = 60
    config.inScaleOnly = false;
    config.rowStepSemitones = 5;
    surface.SetNoteConfig(config, releaseActions);

    bool isRoot = false;
    bool inScale = false;
    auto n00 = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
    VERIFY_IS_TRUE(n00.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, *n00);
    VERIFY_IS_TRUE(isRoot);
    VERIFY_IS_TRUE(inScale);

    auto n01 = surface.MapPadToMidiNote(0, 1, isRoot, inScale);
    VERIFY_IS_TRUE(n01.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 61 }, *n01);
    VERIFY_IS_FALSE(isRoot);
    VERIFY_IS_FALSE(inScale);

    auto n10 = surface.MapPadToMidiNote(1, 0, isRoot, inScale);
    VERIFY_IS_TRUE(n10.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 65 }, *n10);
    VERIFY_IS_FALSE(isRoot);
    VERIFY_IS_TRUE(inScale);

    // In-scale-only mode across Major, NaturalMinor, and MinorPentatonic
    for (auto scale : { midiapp::ScaleKind::Major, midiapp::ScaleKind::NaturalMinor, midiapp::ScaleKind::MinorPentatonic })
    {
        config.scale = scale;
        config.inScaleOnly = true;
        config.octave = 3;
        surface.SetNoteConfig(config, releaseActions);

        auto const rootPad = surface.MapPadToMidiNote(0, 0, isRoot, inScale);
        VERIFY_IS_TRUE(rootPad.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 60 }, *rootPad);
        VERIFY_IS_TRUE(isRoot);
        VERIFY_IS_TRUE(inScale);

        for (uint8_t r = 0; r < GridRows; ++r)
        {
            uint8_t prevColNote = 0;
            for (uint8_t c = 0; c < GridCols; ++c)
            {
                auto const note = surface.MapPadToMidiNote(r, c, isRoot, inScale);
                VERIFY_IS_TRUE(note.has_value());
                VERIFY_IS_TRUE(inScale);
                if (c > 0)
                {
                    VERIFY_IS_TRUE(*note > prevColNote);
                }
                prevColNote = *note;
            }
        }
    }

    // High octave clamps > 127 to std::nullopt
    config.scale = midiapp::ScaleKind::Major;
    config.inScaleOnly = true;
    config.octave = 7; // C7 = 108
    surface.SetNoteConfig(config, releaseActions);
    VERIFY_IS_TRUE(surface.MapPadToMidiNote(0, 0, isRoot, inScale).has_value());
    VERIFY_IS_FALSE(surface.MapPadToMidiNote(7, 7, isRoot, inScale).has_value());
}

void LaunchpadXTests::NoteModePadPressReleaseAndAftertouchTrackActiveNotes()
{
    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);

    NoteLayoutConfig config{};
    config.rootNote = 0;
    config.scale = midiapp::ScaleKind::Major;
    config.octave = 3;
    config.inScaleOnly = false;
    config.rowStepSemitones = 5;
    surface.SetNoteConfig(config, actions);

    auto const snap = MakeClipSnapshot(2, 2);

    // Press pad 11 (C3 = 60)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 96, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOn), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
    VERIFY_ARE_EQUAL(uint8_t{ 96 }, actions[0].velocity);

    // Aftertouch on held pad 11
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadAftertouch, 11, 70, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitPolyAftertouch), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
    VERIFY_ARE_EQUAL(uint8_t{ 70 }, actions[0].velocity);

    // Aftertouch on unpressed pad 12 -> ignored safely
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadAftertouch, 12, 80, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());

    // Release pad 11 -> EmitNoteOff(60)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadRelease, 11, 0, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOff), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);

    // Overlapping isomorphic pitch: pad 16 (row 0, col 5 -> 65) and pad 21 (row 1, col 0 -> 65)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 16, 100, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(uint8_t{ 65 }, actions[0].midiNote);

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 21, 110, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(uint8_t{ 65 }, actions[0].midiNote);

    // Releasing pad 16 must NOT kill note 65 while pad 21 is still held
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadRelease, 16, 0, false, {} }, snap, actions);
    VERIFY_IS_TRUE(actions.empty());

    // Releasing pad 21 emits EmitNoteOff(65)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadRelease, 21, 0, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOff), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 65 }, actions[0].midiNote);
}

void LaunchpadXTests::NoteModeOctaveShiftReleasesHeldNotesAndClampsBounds()
{
    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);

    auto const snap = MakeClipSnapshot(1, 1);

    // Hold pad 11 (note 60 at octave 3)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 90, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);

    // ButtonUp shifts octave +1 after releasing held note 60
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonUp, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(size_t{ 2 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOff), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::ShiftOctave), static_cast<uint8_t>(actions[1].kind));
    VERIFY_ARE_EQUAL(1, actions[1].octaveDelta);
    VERIFY_ARE_EQUAL(4, surface.NoteConfig().octave);

    // Pressing pad 11 now plays C4 (72)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 90, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(uint8_t{ 72 }, actions[0].midiNote);

    // Shift down twice -> octave 2 (C2 = 48)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonDown, 127, false, {} }, snap, actions);
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonDown, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(2, surface.NoteConfig().octave);

    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 90, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(uint8_t{ 48 }, actions[0].midiNote);

    // Clamp bounds [-2, 8]
    for (int i = 0; i < 12; ++i)
    {
        actions.clear();
        surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonDown, 127, false, {} }, snap, actions);
    }
    VERIFY_ARE_EQUAL(LaunchpadXSurface::MinOctave, surface.NoteConfig().octave);

    for (int i = 0; i < 20; ++i)
    {
        actions.clear();
        surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonUp, 127, false, {} }, snap, actions);
    }
    VERIFY_ARE_EQUAL(LaunchpadXSurface::MaxOctave, surface.NoteConfig().octave);
}

void LaunchpadXTests::NoteModeLedFeedbackHighlightsRootScaleAndPressedPads()
{
    LaunchpadXSurface surface{};
    std::vector<SurfaceAction> actions{};
    surface.SetMode(ControllerMode::Note, actions);

    NoteLayoutConfig config{};
    config.rootNote = 0;
    config.scale = midiapp::ScaleKind::Major;
    config.octave = 3;
    config.inScaleOnly = false;
    config.rowStepSemitones = 5;
    surface.SetNoteConfig(config, actions);

    auto const snap = MakeClipSnapshot(1, 1);
    auto const full = surface.BuildLedFeedback(snap, true);
    VERIFY_ARE_EQUAL(size_t{ 81 }, full.size());

    auto const* rootLed = FindLedSpec(full, 11);      // C3 (root)
    auto const* outScaleLed = FindLedSpec(full, 12);  // C#3 (out of scale)
    auto const* inScaleLed = FindLedSpec(full, 13);   // D3 (in scale)
    auto const* noteBtnLed = FindLedSpec(full, ButtonNote);

    VERIFY_IS_TRUE(rootLed != nullptr && outScaleLed != nullptr && inScaleLed != nullptr && noteBtnLed != nullptr);
    VERIFY_IS_TRUE(rootLed->rgb != inScaleLed->rgb);
    VERIFY_IS_TRUE(inScaleLed->rgb != outScaleLed->rgb);
    VERIFY_IS_TRUE(noteBtnLed->rgb.b > 0);

    // Press pad 11 -> differential update highlights pad 11 brightly
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 100, false, {} }, snap, actions);
    auto const pressDiff = surface.BuildLedFeedback(snap, false);
    VERIFY_ARE_EQUAL(size_t{ 1 }, pressDiff.size());
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, pressDiff[0].index);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, pressDiff[0].rgb.r);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, pressDiff[0].rgb.g);
    VERIFY_ARE_EQUAL(uint8_t{ 127 }, pressDiff[0].rgb.b);

    // Release pad 11 -> differential update restores original root color
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadRelease, 11, 0, false, {} }, snap, actions);
    auto const releaseDiff = surface.BuildLedFeedback(snap, false);
    VERIFY_ARE_EQUAL(size_t{ 1 }, releaseDiff.size());
    VERIFY_ARE_EQUAL(uint8_t{ 11 }, releaseDiff[0].index);
    VERIFY_IS_TRUE(releaseDiff[0].rgb == rootLed->rgb);
}

void LaunchpadXTests::ModeSwitchingViaButtonsAndApiReleasesNotesAndRepaints81Leds()
{
    LaunchpadXSurface surface{};
    auto const snap = MakeClipSnapshot(2, 2);
    std::vector<SurfaceAction> actions{};

    // Switch Clip -> Note via CC 96
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonNote, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(ControllerMode::Note), static_cast<uint8_t>(surface.Mode()));
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::SwitchMode), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(ControllerMode::Note), static_cast<uint8_t>(actions[0].targetMode));

    auto const noteLeds = surface.BuildLedFeedback(snap, false);
    VERIFY_ARE_EQUAL(size_t{ 81 }, noteLeds.size());

    // Hold two pads (11 = 60, 12 = 61)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 100, false, {} }, snap, actions);
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 12, 100, false, {} }, snap, actions);

    // Switch Note -> Clip via CC 95 -> releases both held notes then emits SwitchMode(Clip)
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::ButtonPress, ButtonSession, 127, false, {} }, snap, actions);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(ControllerMode::Clip), static_cast<uint8_t>(surface.Mode()));
    VERIFY_ARE_EQUAL(size_t{ 3 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOff), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOff), static_cast<uint8_t>(actions[1].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 61 }, actions[1].midiNote);
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::SwitchMode), static_cast<uint8_t>(actions[2].kind));
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(ControllerMode::Clip), static_cast<uint8_t>(actions[2].targetMode));

    // Mode switch forces full 81-LED repaint even when forceFullRefresh == false
    auto const clipLeds = surface.BuildLedFeedback(snap, false);
    VERIFY_ARE_EQUAL(size_t{ 81 }, clipLeds.size());
    auto const* sessionLed = FindLedSpec(clipLeds, ButtonSession);
    auto const* noteLed = FindLedSpec(clipLeds, ButtonNote);
    VERIFY_IS_TRUE(sessionLed != nullptr && noteLed != nullptr);
    VERIFY_IS_TRUE(sessionLed->rgb.g > noteLed->rgb.g);

    // Switch via SetMode API while holding a note
    surface.SetMode(ControllerMode::Note, actions);
    VERIFY_IS_TRUE(actions.empty());
    actions.clear();
    surface.HandleInput(InputEvent{ InputEventKind::PadPress, 11, 100, false, {} }, snap, actions);
    surface.SetMode(ControllerMode::Clip, actions);
    VERIFY_ARE_EQUAL(size_t{ 1 }, actions.size());
    VERIFY_ARE_EQUAL(static_cast<uint8_t>(SurfaceActionKind::EmitNoteOff), static_cast<uint8_t>(actions[0].kind));
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, actions[0].midiNote);
}

void LaunchpadXTests::BuildSurfaceSnapshotMapsSequenceLaunchViewsAndFolders()
{
    Sequence doc{};
    doc.Scenes.resize(3);
    doc.Scenes[0].Id = L"s0";
    doc.Scenes[1].Id = L"s1";
    doc.Scenes[2].Id = L"s2";

    Track folder{};
    folder.Id = L"folder1";
    folder.IsFolder = true;

    Track child0{};
    child0.Id = L"t0";
    child0.Color = 0xAA1122u;
    child0.Slots = { L"clip0", L"", L"" };

    Track child1{};
    child1.Id = L"t1";
    child1.Color = 0x123456u;
    child1.Slots = { L"", L"clip1", L"" };

    folder.Children.push_back(child0);
    folder.Children.push_back(child1);

    Track pinned{};
    pinned.Id = L"t2";
    pinned.Pinned = true;
    pinned.Color = 0x336699u;
    pinned.Slots = { L"", L"", L"" };

    doc.Tracks.push_back(folder);
    doc.Tracks.push_back(pinned);

    TrackLaunchView view0{};
    view0.TrackId = L"t0";
    view0.Mode = TrackPlayMode::Clip;
    view0.ClipId = L"clip0";
    view0.Slot = 0;

    TrackLaunchView view1{};
    view1.TrackId = L"t1";
    view1.Pending = true;
    view1.PendingMode = TrackPlayMode::Clip;
    view1.PendingClipId = L"clip1";
    view1.PendingSlot = 1;

    NoteLayoutConfig noteCfg{};
    noteCfg.rootNote = 2;
    noteCfg.scale = midiapp::ScaleKind::Dorian;
    noteCfg.octave = 4;

    auto const snap = BuildSurfaceSnapshot(
        doc,
        { view0, view1 },
        L"t1",
        1,
        true,
        false,
        true,
        L"t2",
        2,
        noteCfg,
        true);

    VERIFY_ARE_EQUAL(uint32_t{ 3 }, snap.totalTracks);
    VERIFY_ARE_EQUAL(uint32_t{ 3 }, snap.totalScenes);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, snap.selectedTrackIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, snap.selectedSceneIndex);
    VERIFY_ARE_EQUAL(uint32_t{ 0x123456u }, snap.activeTrackColorRgb24);
    VERIFY_IS_TRUE(snap.transportPlaying);
    VERIFY_IS_FALSE(snap.transportRecording);
    VERIFY_IS_TRUE(snap.selectedTrackArmed);
    VERIFY_IS_TRUE(snap.noteConfig == noteCfg);
    VERIFY_ARE_EQUAL(size_t{ 9 }, snap.gridClips.size());

    // Scene 0, Track 0: clip0 playing
    VERIFY_IS_TRUE(snap.gridClips[0].hasClip);
    VERIFY_IS_TRUE(snap.gridClips[0].playing);
    VERIFY_IS_FALSE(snap.gridClips[0].pending);
    VERIFY_ARE_EQUAL(uint32_t{ 0xAA1122u }, snap.gridClips[0].trackColorRgb24);

    // Scene 1, Track 1: clip1 pending
    VERIFY_IS_TRUE(snap.gridClips[1 * 3 + 1].hasClip);
    VERIFY_IS_FALSE(snap.gridClips[1 * 3 + 1].playing);
    VERIFY_IS_TRUE(snap.gridClips[1 * 3 + 1].pending);
    VERIFY_ARE_EQUAL(uint32_t{ 0x123456u }, snap.gridClips[1 * 3 + 1].trackColorRgb24);

    // Scene 2, Track 2: slot recording
    VERIFY_IS_FALSE(snap.gridClips[2 * 3 + 2].hasClip);
    VERIFY_IS_TRUE(snap.gridClips[2 * 3 + 2].recording);
    VERIFY_ARE_EQUAL(uint32_t{ 0x336699u }, snap.gridClips[2 * 3 + 2].trackColorRgb24);
}
