// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Open Sound Control packets: reading and writing messages and bundles, as the OSC 1.0
// specification and the OSC 1.1 paper describe them. Pure, for the unit tests.
//
// Every packet that arrives is untrusted. The reader checks every size against the packet before
// it uses it, never allocates, and hands out views into the packet it was given, so nothing it
// returns outlives that packet.

#include <sal.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace midipatchbay::osc
{
    // Bounds on what one packet may hold. A packet past any of them is not read.
    constexpr size_t MaximumArguments = 64;
    constexpr size_t MaximumBundleDepth = 8;

    // The largest UDP payload over IPv4.
    constexpr size_t MaximumPacketBytes = 65507;

    // The time tag that means "now": 63 zero bits and a one.
    constexpr uint64_t Immediately = 1;

    // The type tags this reader knows. Anything else can't be read, because its size is unknown.
    namespace type
    {
        constexpr char Int32 = 'i';
        constexpr char Float32 = 'f';
        constexpr char String = 's';
        constexpr char Blob = 'b';
        constexpr char Int64 = 'h';
        constexpr char TimeTag = 't';
        constexpr char Double = 'd';
        constexpr char Symbol = 'S';
        constexpr char Character = 'c';
        constexpr char Color = 'r';
        constexpr char Midi = 'm';
        constexpr char True = 'T';
        constexpr char False = 'F';
        constexpr char Nil = 'N';
        constexpr char Infinitum = 'I';
        constexpr char ArrayBegin = '[';
        constexpr char ArrayEnd = ']';
    }

    struct Argument
    {
        char Type{ 0 };

        // i, h, c, r and m (the four bytes as one number, port id first), and 1 or 0 for T and F.
        int64_t Integer{ 0 };

        // f and d.
        double Real{ 0 };

        // t.
        uint64_t Time{ 0 };

        // s and S.
        std::string_view Text{};

        // b.
        std::span<uint8_t const> Bytes{};
    };

    struct MessageView
    {
        std::string_view Address{};

        // Without the leading comma.
        std::string_view TypeTags{};

        std::array<Argument, MaximumArguments> Arguments{};
        size_t ArgumentCount{ 0 };
    };

    enum class ReadProblem : int32_t
    {
        None = 0,

        // A size or a string runs past the end, or isn't a multiple of four bytes.
        Malformed = 1,

        // No type tag string. Old senders do this; the types can't be guessed safely.
        NoTypeTags = 2,

        // A type tag this reader doesn't know, so where the next argument starts is unknown.
        UnknownType = 3,

        TooManyArguments = 4,
        TooDeep = 5,
    };

    // Told about each message in a packet, in the order they appear.
    class MessageVisitor
    {
    public:
        virtual ~MessageVisitor() noexcept = default;

        // The time tag of the bundle the message is in, or Immediately when it isn't in one.
        virtual void OnMessage(_In_ MessageView const& message, _In_ uint64_t timeTag) noexcept = 0;

        // An element that couldn't be read. Where its size is known, the rest of the packet is
        // still read.
        virtual void OnUnreadable(_In_ ReadProblem problem) noexcept = 0;
    };

    // Reads one packet: one message, or one bundle. Returns the first problem it met, or None.
    ReadProblem ReadPacket(
        _In_reads_(size) uint8_t const* bytes,
        _In_ size_t size,
        _Inout_ MessageVisitor& visitor) noexcept;

    // Reads one message on its own, into message. The views point into bytes.
    ReadProblem ReadMessage(
        _In_reads_(size) uint8_t const* bytes,
        _In_ size_t size,
        _Out_ MessageView& message) noexcept;

    // Builds one message to send.
    class MessageBuilder
    {
    public:
        MessageBuilder() = default;
        explicit MessageBuilder(_In_ std::string_view address);

        MessageBuilder& Int(_In_ int32_t value);
        MessageBuilder& Float(_In_ float value);
        MessageBuilder& String(_In_ std::string_view value);
        MessageBuilder& Blob(_In_ std::span<uint8_t const> value);
        MessageBuilder& Midi(_In_ uint8_t port, _In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2);
        MessageBuilder& Bool(_In_ bool value);

        std::string const& Address() const noexcept { return m_address; }

        // With the leading comma.
        std::string const& TypeTags() const noexcept { return m_typeTags; }

        size_t Size() const noexcept;

        void AppendTo(_Inout_ std::vector<uint8_t>& packet) const;

    private:
        std::string m_address{};
        std::string m_typeTags{ "," };
        std::vector<uint8_t> m_arguments{};
    };

    // One message as a packet.
    std::vector<uint8_t> MakePacket(_In_ MessageBuilder const& message);

    // Several messages in one bundle with one time tag.
    std::vector<uint8_t> MakeBundle(_In_ uint64_t timeTag, _In_ std::span<MessageBuilder const> messages);

    // The bytes a string takes, with its terminating zero and padding to four bytes.
    constexpr size_t PaddedStringSize(_In_ size_t length) noexcept
    {
        return (length + 4) & ~size_t{ 3 };
    }
}
