// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// MIDI to OSC and back, for each of an OSC connection's formats. Received messages become MIDI
// 1.0 messages (UMP types 1, 2 and 3) on the group of the row they match. Messages going out are
// MIDI 1.0 first: MIDI 2.0 channel voice messages are translated the way the UMP specification's
// default translation says. Pure, for the unit tests.

#include "OscConnection.h"
#include "OscPacket.h"

#include <array>
#include <span>

namespace midipatchbay::osc
{
    // Most MIDI messages one received OSC message can become, so a pattern such as
    // /midi/*/cc/7 can't flood a synth.
    constexpr size_t MaximumMessagesFromOneOscMessage = 128;

    // Largest SysEx message sent or received, F0 and F7 included.
    constexpr size_t MaximumSysExBytes = 32 * 1024;

    // Takes what a received OSC message becomes.
    class UmpReceiver
    {
    public:
        virtual ~UmpReceiver() noexcept = default;

        virtual void OnUmp(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept = 0;
    };

    enum class InboundResult : int32_t
    {
        Translated = 0,

        // No row's address matches. The message goes in the "recently heard" list.
        NoRowMatched = 1,

        // A row's address matches, but the arguments aren't what it takes.
        Unreadable = 2,

        // A pattern matched more than MaximumMessagesFromOneOscMessage messages. The first ones
        // went through.
        TooManyMessages = 3,
    };

    struct InboundOutcome
    {
        InboundResult Result{ InboundResult::NoRowMatched };

        // MIDI messages made. A SysEx message counts once, however many UMPs it takes.
        uint32_t Messages{ 0 };
    };

    // Translates one received message for a connection's rows. Never allocates.
    InboundOutcome TranslateInbound(
        _In_ std::span<OscRow const> rows,
        _In_ MessageView const& message,
        _Inout_ UmpReceiver& receiver) noexcept;

    enum class OutboundResult : int32_t
    {
        // At least one OSC message is ready.
        Sent = 0,

        // No row is on the message's group.
        NoRow = 1,

        // Nothing in MIDI 1.0 says this, such as a per-note controller, or a message with no
        // group.
        NotSendable = 2,

        // The row is set not to send this kind: SysEx, clock or active sensing.
        LeftOut = 3,

        // Part of a SysEx message. It goes out when its last part arrives.
        Waiting = 4,

        // A SysEx message larger than MaximumSysExBytes, or one whose parts arrived out of order.
        SysExNotSent = 5,
    };

    // Turns UMPs into OSC messages. Remembers SysEx messages that span several UMPs, one for each
    // group, so one belongs to one connection and is used from one thread at a time.
    class OutboundTranslator
    {
    public:
        OutboundResult Translate(
            _In_ std::span<OscRow const> rows,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount,
            _Inout_ std::vector<MessageBuilder>& out);

        // Forgets every SysEx message in progress.
        void Reset() noexcept;

    private:
        struct SysExInProgress
        {
            std::vector<uint8_t> Bytes{};
            bool Collecting{ false };
            bool TooLarge{ false };
        };

        std::array<SysExInProgress, 16> m_sysEx{};
    };

    // A MIDI 1.0 message as status and data bytes.
    struct Midi1Message
    {
        uint8_t Status{ 0 };
        uint8_t Data1{ 0 };
        uint8_t Data2{ 0 };
    };

    // The MIDI 1.0 messages a MIDI 2.0 channel voice message (UMP type 4) becomes under the UMP
    // specification's default translation: up to four, for a registered or assignable
    // controller. None for the messages MIDI 1.0 has no equivalent of.
    size_t Midi1FromMidi2(
        _In_reads_(2) uint32_t const* words,
        _Out_writes_to_(4, return) Midi1Message* out) noexcept;

    // Pitch bend as a float from 0 to 1 with its center at exactly 0.5, and back.
    float UnitFromPitchBend(_In_ uint16_t value) noexcept;
    uint16_t PitchBendFromUnit(_In_ double unit) noexcept;

    // The arguments of a received message, for people to read: "i 60, f 0.5, s hello".
    std::wstring ArgumentsText(_In_ MessageView const& message);

    // A row that would take this message, when its address looks like one of the formats.
    std::optional<OscRow> SuggestRow(_In_ MessageView const& message, _In_ uint8_t group);
}
