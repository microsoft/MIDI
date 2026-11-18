// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXProtocol.h"

#include <algorithm>
#include <cwctype>

namespace midisequencer::launchpad
{
    namespace
    {
        [[nodiscard]] bool ContainsCaseInsensitive(_In_ std::wstring_view haystack, _In_ std::wstring_view needle) noexcept
        {
            if (needle.empty())
            {
                return true;
            }

            if (haystack.size() < needle.size())
            {
                return false;
            }

            auto const it = std::search(
                haystack.begin(),
                haystack.end(),
                needle.begin(),
                needle.end(),
                [](wchar_t lhs, wchar_t rhs) noexcept
                {
                    return std::towlower(static_cast<wint_t>(lhs)) == std::towlower(static_cast<wint_t>(rhs));
                });

            return it != haystack.end();
        }

        [[nodiscard]] UmpPacket MakeSysEx7Packet(
            _In_ uint8_t group,
            _In_ uint8_t status,
            _In_reads_opt_(byteCount) const uint8_t* bytes,
            _In_ uint8_t byteCount) noexcept
        {
            std::array<uint8_t, 6> padded{};
            if (bytes != nullptr)
            {
                for (uint8_t i = 0; i < byteCount && i < 6; ++i)
                {
                    padded[i] = bytes[i];
                }
            }

            UmpPacket packet{};
            packet.wordCount = 2;
            packet.words[0] =
                0x30000000u |
                (static_cast<uint32_t>(group & 0x0Fu) << 24) |
                (static_cast<uint32_t>(status & 0x0Fu) << 20) |
                (static_cast<uint32_t>(byteCount & 0x0Fu) << 16) |
                (static_cast<uint32_t>(padded[0]) << 8) |
                static_cast<uint32_t>(padded[1]);
            packet.words[1] =
                (static_cast<uint32_t>(padded[2]) << 24) |
                (static_cast<uint32_t>(padded[3]) << 16) |
                (static_cast<uint32_t>(padded[4]) << 8) |
                static_cast<uint32_t>(padded[5]);
            return packet;
        }

        [[nodiscard]] bool TryParseDeviceInquiryPayload(
            _In_ std::span<const uint8_t> payload,
            _Out_ InputEvent& outEvent) noexcept
        {
            outEvent = InputEvent{};

            // Reply format (between F0 and F7):
            // 7E <devId> 06 02 00 20 29 13 <01=App|11=Bootloader> <m0> <m1> <v0> <v1> <v2> <v3>
            if (payload.size() < 15)
            {
                return false;
            }

            if (payload[0] != 0x7Eu ||
                payload[2] != 0x06u ||
                payload[3] != 0x02u ||
                payload[4] != 0x00u ||
                payload[5] != 0x20u ||
                payload[6] != 0x29u ||
                payload[7] != 0x13u)
            {
                return false;
            }

            if (payload[8] != DeviceIdApplication && payload[8] != DeviceIdBootloader)
            {
                return false;
            }

            outEvent.kind = InputEventKind::DeviceInquiryReply;
            outEvent.index = payload[1];
            outEvent.value = payload[8];
            outEvent.isBootloader = (payload[8] == DeviceIdBootloader);
            outEvent.version[0] = payload[11];
            outEvent.version[1] = payload[12];
            outEvent.version[2] = payload[13];
            outEvent.version[3] = payload[14];
            return true;
        }
    }

    void SysEx7Accumulator::Reset() noexcept
    {
        for (auto& buf : m_groups)
        {
            buf.active = false;
            buf.payload.clear();
        }
    }

    _Use_decl_annotations_
    bool SysEx7Accumulator::Feed(
        uint32_t word0,
        uint32_t word1,
        uint8_t& outGroup,
        std::vector<uint8_t>& outPayload)
    {
        outGroup = 0;
        outPayload.clear();

        if ((word0 >> 28) != 0x3u)
        {
            return false;
        }

        auto const group = static_cast<uint8_t>((word0 >> 24) & 0x0Fu);
        auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0Fu);
        auto const byteCount = static_cast<uint8_t>((word0 >> 16) & 0x0Fu);

        auto& buf = m_groups[group];

        if (byteCount > 6)
        {
            buf.active = false;
            buf.payload.clear();
            return false;
        }

        std::array<uint8_t, 6> const bytes{
            static_cast<uint8_t>((word0 >> 8) & 0xFFu),
            static_cast<uint8_t>(word0 & 0xFFu),
            static_cast<uint8_t>((word1 >> 24) & 0xFFu),
            static_cast<uint8_t>((word1 >> 16) & 0xFFu),
            static_cast<uint8_t>((word1 >> 8) & 0xFFu),
            static_cast<uint8_t>(word1 & 0xFFu),
        };

        for (uint8_t i = 0; i < byteCount; ++i)
        {
            if ((bytes[i] & 0x80u) != 0u)
            {
                buf.active = false;
                buf.payload.clear();
                return false;
            }
        }

        switch (status)
        {
        case 0:
            buf.active = false;
            buf.payload.clear();
            outGroup = group;
            outPayload.assign(bytes.begin(), bytes.begin() + byteCount);
            return true;

        case 1:
            buf.active = true;
            buf.payload.assign(bytes.begin(), bytes.begin() + byteCount);
            return false;

        case 2:
            if (!buf.active)
            {
                return false;
            }
            if (buf.payload.size() + byteCount > MaxPayloadBytes)
            {
                buf.active = false;
                buf.payload.clear();
                return false;
            }
            buf.payload.insert(buf.payload.end(), bytes.begin(), bytes.begin() + byteCount);
            return false;

        case 3:
            if (!buf.active)
            {
                return false;
            }
            if (buf.payload.size() + byteCount > MaxPayloadBytes)
            {
                buf.active = false;
                buf.payload.clear();
                return false;
            }
            buf.payload.insert(buf.payload.end(), bytes.begin(), bytes.begin() + byteCount);
            outGroup = group;
            outPayload = std::move(buf.payload);
            buf.active = false;
            buf.payload.clear();
            return true;

        default:
            buf.active = false;
            buf.payload.clear();
            return false;
        }
    }

    _Use_decl_annotations_
    std::vector<UmpPacket> PacketizeSysEx7(
        uint8_t group,
        std::span<const uint8_t> payloadWithoutFraming)
    {
        for (auto const byte : payloadWithoutFraming)
        {
            if ((byte & 0x80u) != 0u)
            {
                return {};
            }
        }

        auto const total = payloadWithoutFraming.size();
        if (total <= 6)
        {
            return {
                MakeSysEx7Packet(
                    group,
                    0,
                    total == 0 ? nullptr : payloadWithoutFraming.data(),
                    static_cast<uint8_t>(total))
            };
        }

        std::vector<UmpPacket> packets{};
        packets.reserve((total + 5) / 6);

        for (size_t offset = 0; offset < total; offset += 6)
        {
            auto const remaining = total - offset;
            auto const chunk = static_cast<uint8_t>(std::min<size_t>(remaining, 6));
            uint8_t status = 2;
            if (offset == 0)
            {
                status = 1;
            }
            else if (offset + chunk >= total)
            {
                status = 3;
            }

            packets.push_back(MakeSysEx7Packet(group, status, payloadWithoutFraming.data() + offset, chunk));
        }

        return packets;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> BuildProgrammerModeSysEx(bool enter)
    {
        std::vector<uint8_t> payload(SysExHeader.begin(), SysExHeader.end());
        payload.push_back(SysExCommandProgrammerMode);
        payload.push_back(enter ? EnterProgrammerMode[1] : ExitProgrammerMode[1]);
        return payload;
    }

    _Use_decl_annotations_
    std::vector<UmpPacket> BuildProgrammerModePackets(uint8_t group, bool enter)
    {
        auto const payload = BuildProgrammerModeSysEx(enter);
        return PacketizeSysEx7(group, payload);
    }

    std::vector<uint8_t> BuildSelectProgrammerLayoutSysEx()
    {
        std::vector<uint8_t> payload(SysExHeader.begin(), SysExHeader.end());
        payload.push_back(SelectProgrammerLayout[0]);
        payload.push_back(SelectProgrammerLayout[1]);
        return payload;
    }

    _Use_decl_annotations_
    std::vector<UmpPacket> BuildSelectProgrammerLayoutPackets(uint8_t group)
    {
        auto const payload = BuildSelectProgrammerLayoutSysEx();
        return PacketizeSysEx7(group, payload);
    }

    _Use_decl_annotations_
    std::vector<UmpPacket> BuildDeviceInquiryPackets(uint8_t group)
    {
        return PacketizeSysEx7(group, DeviceInquiryRequest);
    }

    _Use_decl_annotations_
    std::vector<UmpPacket> BuildLedSysExPackets(
        uint8_t group,
        std::span<const LedSpec> specs)
    {
        if (specs.empty())
        {
            return {};
        }

        std::vector<UmpPacket> packets{};

        for (size_t start = 0; start < specs.size(); start += MaxBulkLedSpecs)
        {
            auto const count = std::min<size_t>(specs.size() - start, MaxBulkLedSpecs);

            std::vector<uint8_t> payload{};
            payload.reserve(SysExHeader.size() + 1 + count * 5);
            payload.insert(payload.end(), SysExHeader.begin(), SysExHeader.end());
            payload.push_back(SysExCommandLedLighting);

            for (size_t i = 0; i < count; ++i)
            {
                auto const& spec = specs[start + i];
                auto const index = static_cast<uint8_t>(spec.index & 0x7Fu);

                switch (spec.behavior)
                {
                case LedBehavior::StaticPalette:
                    payload.push_back(0x00u);
                    payload.push_back(index);
                    payload.push_back(static_cast<uint8_t>(spec.colorA & 0x7Fu));
                    break;

                case LedBehavior::FlashingPalette:
                    payload.push_back(0x01u);
                    payload.push_back(index);
                    payload.push_back(static_cast<uint8_t>(spec.colorB & 0x7Fu));
                    payload.push_back(static_cast<uint8_t>(spec.colorA & 0x7Fu));
                    break;

                case LedBehavior::PulsingPalette:
                    payload.push_back(0x02u);
                    payload.push_back(index);
                    payload.push_back(static_cast<uint8_t>(spec.colorA & 0x7Fu));
                    break;

                case LedBehavior::Rgb:
                    payload.push_back(0x03u);
                    payload.push_back(index);
                    payload.push_back(static_cast<uint8_t>(spec.rgb.r & 0x7Fu));
                    payload.push_back(static_cast<uint8_t>(spec.rgb.g & 0x7Fu));
                    payload.push_back(static_cast<uint8_t>(spec.rgb.b & 0x7Fu));
                    break;
                }
            }

            auto chunkPackets = PacketizeSysEx7(group, payload);
            packets.insert(packets.end(), chunkPackets.begin(), chunkPackets.end());
        }

        return packets;
    }

    _Use_decl_annotations_
    UmpPacket BuildChannelLedPacket(
        uint8_t group,
        uint8_t index,
        bool isPad,
        LedBehavior behavior,
        uint8_t paletteColor) noexcept
    {
        uint8_t channel = 0;
        switch (behavior)
        {
        case LedBehavior::FlashingPalette:
            channel = 1;
            break;
        case LedBehavior::PulsingPalette:
            channel = 2;
            break;
        case LedBehavior::StaticPalette:
        case LedBehavior::Rgb:
        default:
            channel = 0;
            break;
        }

        uint32_t const status = isPad ? 0x9u : 0xBu;

        UmpPacket packet{};
        packet.wordCount = 1;
        packet.words[0] =
            0x20000000u |
            (static_cast<uint32_t>(group & 0x0Fu) << 24) |
            (status << 20) |
            (static_cast<uint32_t>(channel) << 16) |
            (static_cast<uint32_t>(index & 0x7Fu) << 8) |
            static_cast<uint32_t>(paletteColor & 0x7Fu);
        return packet;
    }

    _Use_decl_annotations_
    bool TryParseInputUmp(
        const uint32_t* words,
        uint8_t wordCount,
        InputEvent& outEvent,
        SysEx7Accumulator* accumulator) noexcept
    {
        outEvent = InputEvent{};

        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        auto const word0 = words[0];
        auto const messageType = word0 >> 28;

        if (messageType == 0x3u)
        {
            if (wordCount < 2)
            {
                return false;
            }

            try
            {
                SysEx7Accumulator localAccumulator{};
                auto* const activeAccumulator = accumulator != nullptr ? accumulator : &localAccumulator;
                uint8_t group = 0;
                std::vector<uint8_t> payload{};
                if (!activeAccumulator->Feed(words[0], words[1], group, payload))
                {
                    return false;
                }

                return TryParseDeviceInquiryPayload(payload, outEvent);
            }
            catch (...)
            {
                outEvent = InputEvent{};
                return false;
            }
        }

        if (messageType != 0x2u && messageType != 0x4u)
        {
            return false;
        }

        if (messageType == 0x4u && wordCount < 2)
        {
            return false;
        }

        auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0Fu);
        auto const index = static_cast<uint8_t>((word0 >> 8) & 0x7Fu);

        uint8_t value = 0;
        if (messageType == 0x2u)
        {
            value = static_cast<uint8_t>(word0 & 0x7Fu);
        }
        else
        {
            if (status == 0x9u || status == 0x8u)
            {
                auto const velocity16 = words[1] >> 16;
                if (velocity16 == 0u)
                {
                    value = 0;
                }
                else
                {
                    auto const scaled = static_cast<uint8_t>(velocity16 >> 9);
                    value = scaled == 0u ? 1u : scaled;
                }
            }
            else
            {
                value = static_cast<uint8_t>(words[1] >> 25);
            }
        }

        switch (status)
        {
        case 0x9u:
            if (!IsGridPadIndex(index))
            {
                return false;
            }
            outEvent.kind = (value == 0u) ? InputEventKind::PadRelease : InputEventKind::PadPress;
            outEvent.index = index;
            outEvent.value = value;
            return true;

        case 0x8u:
            if (!IsGridPadIndex(index))
            {
                return false;
            }
            outEvent.kind = InputEventKind::PadRelease;
            outEvent.index = index;
            outEvent.value = value;
            return true;

        case 0xAu:
            if (!IsGridPadIndex(index))
            {
                return false;
            }
            outEvent.kind = InputEventKind::PadAftertouch;
            outEvent.index = index;
            outEvent.value = value;
            return true;

        case 0xBu:
            if (!IsControlButtonIndex(index))
            {
                return false;
            }
            outEvent.kind = (value >= 64u) ? InputEventKind::ButtonPress : InputEventKind::ButtonRelease;
            outEvent.index = index;
            outEvent.value = value;
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool IsLaunchpadMidiEndpointName(std::wstring_view name) noexcept
    {
        if (name.empty())
        {
            return false;
        }

        if (ContainsCaseInsensitive(name, L"lpx daw"))
        {
            return false;
        }

        return ContainsCaseInsensitive(name, L"lpx midi") ||
            ContainsCaseInsensitive(name, L"launchpad x");
    }
}
