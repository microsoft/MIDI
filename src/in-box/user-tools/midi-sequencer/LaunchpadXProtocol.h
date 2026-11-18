// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Novation Launchpad X Programmer Mode protocol codec, UMP Type 0x3 SysEx7 packetizer/depacketizer,
// Type 0x2/0x4 Channel Voice input parser, and endpoint name matcher.
// Pure C++20 with no pch.h, no XAML, and no WinRT dependencies.

#include <sal.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace midisequencer::launchpad
{
    inline constexpr uint8_t GridMin = 11;
    inline constexpr uint8_t GridMax = 88;
    inline constexpr uint8_t GridRows = 8;
    inline constexpr uint8_t GridCols = 8;
    inline constexpr uint8_t PadCount = GridRows * GridCols;
    inline constexpr uint8_t TotalLedCount = 81;
    inline constexpr uint8_t MaxBulkLedSpecs = 81;

    inline constexpr uint8_t ButtonUp = 91;
    inline constexpr uint8_t ButtonDown = 92;
    inline constexpr uint8_t ButtonLeft = 93;
    inline constexpr uint8_t ButtonRight = 94;
    inline constexpr uint8_t ButtonSession = 95;
    inline constexpr uint8_t ButtonNote = 96;
    inline constexpr uint8_t ButtonCustom = 97;
    inline constexpr uint8_t ButtonRecord = 98;
    inline constexpr uint8_t LogoLed = 99;

    inline constexpr std::array<uint8_t, 5> SysExHeader{ 0x00, 0x20, 0x29, 0x02, 0x0C };
    inline constexpr std::array<uint8_t, 2> EnterProgrammerMode{ 0x0E, 0x01 };
    inline constexpr std::array<uint8_t, 2> ExitProgrammerMode{ 0x0E, 0x00 };
    inline constexpr std::array<uint8_t, 2> SelectProgrammerLayout{ 0x00, 0x7F };
    inline constexpr std::array<uint8_t, 4> DeviceInquiryRequest{ 0x7E, 0x7F, 0x06, 0x01 };

    inline constexpr uint8_t SysExCommandSelectLayout = 0x00;
    inline constexpr uint8_t SysExCommandLedLighting = 0x03;
    inline constexpr uint8_t SysExCommandProgrammerMode = 0x0E;
    inline constexpr uint8_t LayoutProgrammer = 0x7F;
    inline constexpr uint8_t DeviceIdApplication = 0x01;
    inline constexpr uint8_t DeviceIdBootloader = 0x11;

    [[nodiscard]] constexpr uint8_t SceneButtonIndex(_In_ uint8_t topDownRow) noexcept
    {
        return topDownRow < GridRows ? static_cast<uint8_t>((GridRows - topDownRow) * 10 + 9) : 0;
    }

    [[nodiscard]] constexpr bool IsGridPadIndex(_In_ uint8_t index) noexcept
    {
        auto const row = index / 10;
        auto const col = index % 10;
        return row >= 1 && row <= GridRows && col >= 1 && col <= GridCols;
    }

    [[nodiscard]] constexpr uint8_t GridPadIndex(_In_ uint8_t rowBottomUp0To7, _In_ uint8_t colLeftRight0To7) noexcept
    {
        if (rowBottomUp0To7 >= GridRows || colLeftRight0To7 >= GridCols)
        {
            return 0;
        }

        return static_cast<uint8_t>((rowBottomUp0To7 + 1) * 10 + (colLeftRight0To7 + 1));
    }

    [[nodiscard]] constexpr bool PadRowColFromIndex(
        _In_ uint8_t index,
        _Out_ uint8_t& outRowBottomUp0To7,
        _Out_ uint8_t& outColLeftRight0To7) noexcept
    {
        outRowBottomUp0To7 = 0;
        outColLeftRight0To7 = 0;

        if (!IsGridPadIndex(index))
        {
            return false;
        }

        outRowBottomUp0To7 = static_cast<uint8_t>(index / 10 - 1);
        outColLeftRight0To7 = static_cast<uint8_t>(index % 10 - 1);
        return true;
    }

    [[nodiscard]] constexpr bool IsSceneButtonIndex(_In_ uint8_t index) noexcept
    {
        return index >= 19 && index <= 89 && (index % 10) == 9;
    }

    [[nodiscard]] constexpr bool IsTopRowButtonIndex(_In_ uint8_t index) noexcept
    {
        return index >= ButtonUp && index <= ButtonRecord;
    }

    [[nodiscard]] constexpr bool IsControlButtonIndex(_In_ uint8_t index) noexcept
    {
        return IsSceneButtonIndex(index) || IsTopRowButtonIndex(index);
    }

    [[nodiscard]] constexpr bool IsLedIndex(_In_ uint8_t index) noexcept
    {
        return IsGridPadIndex(index) || IsControlButtonIndex(index) || index == LogoLed;
    }

    struct RgbColor
    {
        uint8_t r{ 0 };
        uint8_t g{ 0 };
        uint8_t b{ 0 };

        [[nodiscard]] static constexpr RgbColor FromRgb24(_In_ uint32_t rgb) noexcept
        {
            auto const clamp7 = [](uint32_t channel8) noexcept -> uint8_t
            {
                auto const scaled = channel8 >> 1;
                return static_cast<uint8_t>(scaled > 0x7Fu ? 0x7Fu : scaled);
            };

            return {
                clamp7((rgb >> 16) & 0xFFu),
                clamp7((rgb >> 8) & 0xFFu),
                clamp7(rgb & 0xFFu)
            };
        }

        bool operator==(RgbColor const&) const = default;
    };

    enum class LedBehavior : uint8_t
    {
        StaticPalette = 0,
        FlashingPalette = 1,
        PulsingPalette = 2,
        Rgb = 3,
    };

    struct LedSpec
    {
        uint8_t index{ 0 };
        LedBehavior behavior{ LedBehavior::StaticPalette };
        uint8_t colorA{ 0 };
        uint8_t colorB{ 0 };
        RgbColor rgb{};

        bool operator==(LedSpec const&) const = default;
    };

    enum class InputEventKind : uint8_t
    {
        PadPress,
        PadRelease,
        PadAftertouch,
        ButtonPress,
        ButtonRelease,
        DeviceInquiryReply,
    };

    struct InputEvent
    {
        InputEventKind kind{ InputEventKind::PadPress };
        uint8_t index{ 0 };
        uint8_t value{ 0 };
        bool isBootloader{ false };
        uint8_t version[4]{};

        bool operator==(InputEvent const&) const = default;
    };

    struct UmpPacket
    {
        uint32_t words[4]{};
        uint8_t wordCount{ 0 };

        bool operator==(UmpPacket const&) const = default;
    };

    class SysEx7Accumulator
    {
    public:
        static constexpr size_t MaxPayloadBytes = 4096;

        void Reset() noexcept;
        [[nodiscard]] bool Feed(
            _In_ uint32_t word0,
            _In_ uint32_t word1,
            _Out_ uint8_t& outGroup,
            _Out_ std::vector<uint8_t>& outPayload);

    private:
        struct GroupBuffer
        {
            bool active{ false };
            std::vector<uint8_t> payload{};
        };

        std::array<GroupBuffer, 16> m_groups{};
    };

    [[nodiscard]] std::vector<UmpPacket> PacketizeSysEx7(
        _In_ uint8_t group,
        _In_ std::span<const uint8_t> payloadWithoutFraming);

    [[nodiscard]] std::vector<uint8_t> BuildProgrammerModeSysEx(_In_ bool enter);
    [[nodiscard]] std::vector<UmpPacket> BuildProgrammerModePackets(_In_ uint8_t group, _In_ bool enter);
    [[nodiscard]] std::vector<uint8_t> BuildSelectProgrammerLayoutSysEx();
    [[nodiscard]] std::vector<UmpPacket> BuildSelectProgrammerLayoutPackets(_In_ uint8_t group);
    [[nodiscard]] std::vector<UmpPacket> BuildDeviceInquiryPackets(_In_ uint8_t group);
    [[nodiscard]] std::vector<UmpPacket> BuildLedSysExPackets(
        _In_ uint8_t group,
        _In_ std::span<const LedSpec> specs);
    [[nodiscard]] UmpPacket BuildChannelLedPacket(
        _In_ uint8_t group,
        _In_ uint8_t index,
        _In_ bool isPad,
        _In_ LedBehavior behavior,
        _In_ uint8_t paletteColor) noexcept;

    [[nodiscard]] bool TryParseInputUmp(
        _In_reads_opt_(wordCount) const uint32_t* words,
        _In_ uint8_t wordCount,
        _Out_ InputEvent& outEvent,
        _Inout_opt_ SysEx7Accumulator* accumulator = nullptr) noexcept;

    [[nodiscard]] bool IsLaunchpadMidiEndpointName(_In_ std::wstring_view name) noexcept;
}

namespace seq = ::midisequencer;
