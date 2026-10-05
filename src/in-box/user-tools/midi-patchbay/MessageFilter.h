// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pure: no XAML, no resources and no precompiled header, so the unit tests compile it exactly as
// it ships. The names and summaries a customer reads are in MessageText.h.

#include <sal.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace midipatchbay
{
    namespace json = ::winrt::Windows::Data::Json;

    // UMP message types, as they appear in the first nibble of the first word.
    enum class UmpMessageType : uint8_t
    {
        Utility = 0x0,
        System = 0x1,
        Midi1ChannelVoice = 0x2,
        DataSysEx7 = 0x3,
        Midi2ChannelVoice = 0x4,
        Data128 = 0x5,
        FlexData = 0xD,
        Stream = 0xF,
    };

    constexpr size_t MessageTypeCount = 16;
    constexpr size_t ChannelCount = 16;

    // The channel voice status nibbles, which MIDI 1.0 and MIDI 2.0 share for the first seven.
    constexpr size_t ChannelVoiceStatusCount = 16;

    // System status bytes this app lets a customer pick out individually. Anything in message
    // type 1 that is not in here rides on the message type switch alone.
    enum class SystemMessage : uint8_t
    {
        TimeCode = 0xF1,
        SongPosition = 0xF2,
        SongSelect = 0xF3,
        TuneRequest = 0xF6,
        TimingClock = 0xF8,
        Start = 0xFA,
        Continue = 0xFB,
        Stop = 0xFC,
        ActiveSensing = 0xFE,
        Reset = 0xFF,
    };

    constexpr uint8_t SystemMessageList[] =
    {
        0xF1, 0xF2, 0xF3, 0xF6, 0xF8, 0xFA, 0xFB, 0xFC, 0xFE, 0xFF,
    };

    constexpr size_t SystemMessageCount = std::size(SystemMessageList);

    constexpr uint8_t LowestNote = 0;
    constexpr uint8_t HighestNote = 127;

    // What one connection lets through.
    //
    // Deliberately a plain value with no UI and no WinRT: the routing engine evaluates it on the
    // callback thread for every message, and a future API would expose this shape.
    //
    // Every set is an ALLOW set and starts full, so a filter that has never been touched passes
    // everything and costs one bool test.
    struct MessageFilter
    {
        // The one flag the hot path checks first.
        bool IsActive{ false };

        std::array<bool, MessageTypeCount> MessageTypes{};
        std::array<bool, ChannelCount> Channels{};

        // Applies to both MIDI 1.0 and MIDI 2.0 channel voice messages.
        std::array<bool, ChannelVoiceStatusCount> ChannelVoiceStatuses{};

        std::array<bool, SystemMessageCount> SystemMessages{};

        // Note on, note off, poly pressure and the per note MIDI 2.0 messages outside this range
        // are dropped. Everything else ignores it.
        bool LimitNoteRange{ false };
        uint8_t LowestAllowedNote{ LowestNote };
        uint8_t HighestAllowedNote{ HighestNote };

        MessageFilter() noexcept;

        // True when nothing is excluded, which is what lets the inspector say "everything".
        bool PassesEverything() const noexcept;

        // Back to the default. The connection keeps the filter object; it just stops excluding.
        void Reset() noexcept;

        // The hot path. words[0] is enough for everything decided here, but the count is taken
        // so a malformed packet cannot be read past.
        bool Allows(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
    };

    // Index in SystemMessageList, or SystemMessageCount when the status is not one of them.
    size_t IndexOfSystemMessage(_In_ uint8_t status) noexcept;

    // Whether a channel voice status carries a note index in the same byte. MIDI 2.0 adds the
    // per note messages to the three MIDI 1.0 ones.
    bool StatusCarriesNote(_In_ uint8_t status, _In_ bool isMidi2) noexcept;

    // A bit per entry, so a set costs a few characters in the patch file rather than a list of
    // sixteen booleans.
    uint32_t PackFlags(_In_reads_(count) bool const* values, _In_ size_t count) noexcept;
    void UnpackFlags(_In_ uint32_t packed, _Out_writes_(count) bool* values, _In_ size_t count) noexcept;
    bool AllTrue(_In_reads_(count) bool const* values, _In_ size_t count) noexcept;

    json::JsonObject FilterToJson(_In_ MessageFilter const& filter) noexcept;
    MessageFilter FilterFromJson(_In_ json::JsonObject const& object) noexcept;

    // Short and stable. The routing engine compares plans by string, so a filter change has to
    // show up there or it would never be applied to a running route.
    std::wstring FilterSignature(_In_ MessageFilter const& filter) noexcept;
}
