// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscPacket.h"

#include <bit>
#include <cstring>

namespace midipatchbay::osc
{
    namespace
    {
        constexpr char BundleTag[8] = { '#', 'b', 'u', 'n', 'd', 'l', 'e', '\0' };

        uint32_t ReadUInt32(_In_reads_(4) uint8_t const* bytes) noexcept
        {
            return (static_cast<uint32_t>(bytes[0]) << 24) |
                (static_cast<uint32_t>(bytes[1]) << 16) |
                (static_cast<uint32_t>(bytes[2]) << 8) |
                static_cast<uint32_t>(bytes[3]);
        }

        uint64_t ReadUInt64(_In_reads_(8) uint8_t const* bytes) noexcept
        {
            return (static_cast<uint64_t>(ReadUInt32(bytes)) << 32) | ReadUInt32(bytes + 4);
        }

        void WriteUInt32(_Inout_ std::vector<uint8_t>& out, _In_ uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value >> 24));
            out.push_back(static_cast<uint8_t>(value >> 16));
            out.push_back(static_cast<uint8_t>(value >> 8));
            out.push_back(static_cast<uint8_t>(value));
        }

        void WriteUInt64(_Inout_ std::vector<uint8_t>& out, _In_ uint64_t value)
        {
            WriteUInt32(out, static_cast<uint32_t>(value >> 32));
            WriteUInt32(out, static_cast<uint32_t>(value));
        }

        void WritePaddedString(_Inout_ std::vector<uint8_t>& out, _In_ std::string_view text)
        {
            out.insert(out.end(), text.begin(), text.end());
            out.resize(out.size() + PaddedStringSize(text.size()) - text.size(), 0);
        }

        // A zero-terminated string starting at position, padded to four bytes. False when it
        // doesn't end inside the packet.
        bool ReadString(
            _In_reads_(size) uint8_t const* bytes,
            _In_ size_t size,
            _Inout_ size_t& position,
            _Out_ std::string_view& text) noexcept
        {
            text = {};

            if (position >= size)
            {
                return false;
            }

            auto const start = bytes + position;
            auto const end = static_cast<uint8_t const*>(std::memchr(start, 0, size - position));

            if (end == nullptr)
            {
                return false;
            }

            auto const length = static_cast<size_t>(end - start);
            auto const padded = PaddedStringSize(length);

            if (padded > size - position)
            {
                return false;
            }

            text = std::string_view{ reinterpret_cast<char const*>(start), length };
            position += padded;

            return true;
        }

        ReadProblem ReadArguments(
            _In_reads_(size) uint8_t const* bytes,
            _In_ size_t size,
            _In_ size_t position,
            _Inout_ MessageView& message) noexcept
        {
            if (message.TypeTags.size() > MaximumArguments)
            {
                return ReadProblem::TooManyArguments;
            }

            for (auto const tag : message.TypeTags)
            {
                auto& argument = message.Arguments[message.ArgumentCount];
                argument = Argument{};
                argument.Type = tag;

                auto const remaining = size - position;

                switch (tag)
                {
                case type::Int32:
                case type::Character:
                case type::Color:
                case type::Midi:
                case type::Float32:
                {
                    if (remaining < 4)
                    {
                        return ReadProblem::Malformed;
                    }

                    auto const value = ReadUInt32(bytes + position);

                    if (tag == type::Float32)
                    {
                        argument.Real = std::bit_cast<float>(value);
                    }
                    else if (tag == type::Int32)
                    {
                        argument.Integer = static_cast<int32_t>(value);
                    }
                    else
                    {
                        argument.Integer = value;
                    }

                    position += 4;
                    break;
                }

                case type::Int64:
                case type::TimeTag:
                case type::Double:
                {
                    if (remaining < 8)
                    {
                        return ReadProblem::Malformed;
                    }

                    auto const value = ReadUInt64(bytes + position);

                    if (tag == type::Double)
                    {
                        argument.Real = std::bit_cast<double>(value);
                    }
                    else if (tag == type::Int64)
                    {
                        argument.Integer = static_cast<int64_t>(value);
                    }
                    else
                    {
                        argument.Time = value;
                    }

                    position += 8;
                    break;
                }

                case type::String:
                case type::Symbol:
                    if (!ReadString(bytes, size, position, argument.Text))
                    {
                        return ReadProblem::Malformed;
                    }
                    break;

                case type::Blob:
                {
                    if (remaining < 4)
                    {
                        return ReadProblem::Malformed;
                    }

                    auto const length = ReadUInt32(bytes + position);

                    // Compared before padding, so a length near 4 GB can't wrap around.
                    if (length > remaining - 4)
                    {
                        return ReadProblem::Malformed;
                    }

                    auto const padded = (static_cast<size_t>(length) + 3) & ~size_t{ 3 };

                    if (padded > remaining - 4)
                    {
                        return ReadProblem::Malformed;
                    }

                    argument.Bytes = std::span<uint8_t const>{ bytes + position + 4, length };
                    position += 4 + padded;
                    break;
                }

                case type::True:
                    argument.Integer = 1;
                    break;

                case type::False:
                case type::Nil:
                case type::Infinitum:
                case type::ArrayBegin:
                case type::ArrayEnd:
                    break;

                default:
                    return ReadProblem::UnknownType;
                }

                message.ArgumentCount++;
            }

            return ReadProblem::None;
        }

        ReadProblem ReadElement(
            _In_reads_(size) uint8_t const* bytes,
            _In_ size_t size,
            _In_ uint64_t timeTag,
            _In_ size_t depth,
            _Inout_ MessageVisitor& visitor,
            _Inout_ MessageView& scratch) noexcept;

        ReadProblem ReadBundle(
            _In_reads_(size) uint8_t const* bytes,
            _In_ size_t size,
            _In_ size_t depth,
            _Inout_ MessageVisitor& visitor,
            _Inout_ MessageView& scratch) noexcept
        {
            if (depth >= MaximumBundleDepth)
            {
                visitor.OnUnreadable(ReadProblem::TooDeep);
                return ReadProblem::TooDeep;
            }

            if (size < 16)
            {
                visitor.OnUnreadable(ReadProblem::Malformed);
                return ReadProblem::Malformed;
            }

            auto const timeTag = ReadUInt64(bytes + 8);
            size_t position{ 16 };
            auto first = ReadProblem::None;

            while (position < size)
            {
                if (size - position < 4)
                {
                    visitor.OnUnreadable(ReadProblem::Malformed);
                    return first == ReadProblem::None ? ReadProblem::Malformed : first;
                }

                auto const length = ReadUInt32(bytes + position);
                position += 4;

                if (length == 0 || (length & 3) != 0 || length > size - position)
                {
                    visitor.OnUnreadable(ReadProblem::Malformed);
                    return first == ReadProblem::None ? ReadProblem::Malformed : first;
                }

                auto const problem = ReadElement(bytes + position, length, timeTag, depth + 1, visitor, scratch);

                if (first == ReadProblem::None)
                {
                    first = problem;
                }

                position += length;
            }

            return first;
        }

        ReadProblem ReadElement(
            _In_reads_(size) uint8_t const* bytes,
            _In_ size_t size,
            _In_ uint64_t timeTag,
            _In_ size_t depth,
            _Inout_ MessageVisitor& visitor,
            _Inout_ MessageView& scratch) noexcept
        {
            if (size >= sizeof(BundleTag) && std::memcmp(bytes, BundleTag, sizeof(BundleTag)) == 0)
            {
                return ReadBundle(bytes, size, depth, visitor, scratch);
            }

            auto const problem = ReadMessage(bytes, size, scratch);

            if (problem != ReadProblem::None)
            {
                visitor.OnUnreadable(problem);
                return problem;
            }

            visitor.OnMessage(scratch, timeTag);

            return ReadProblem::None;
        }
    }

    _Use_decl_annotations_
    ReadProblem ReadMessage(uint8_t const* bytes, size_t size, MessageView& message) noexcept
    {
        message.Address = {};
        message.TypeTags = {};
        message.ArgumentCount = 0;

        if (bytes == nullptr || size == 0 || size > MaximumPacketBytes || (size & 3) != 0 || bytes[0] != '/')
        {
            return ReadProblem::Malformed;
        }

        size_t position{ 0 };

        if (!ReadString(bytes, size, position, message.Address))
        {
            return ReadProblem::Malformed;
        }

        if (position >= size || bytes[position] != ',')
        {
            return ReadProblem::NoTypeTags;
        }

        std::string_view tags{};

        if (!ReadString(bytes, size, position, tags))
        {
            return ReadProblem::Malformed;
        }

        message.TypeTags = tags.substr(1);

        return ReadArguments(bytes, size, position, message);
    }

    _Use_decl_annotations_
    ReadProblem ReadPacket(uint8_t const* bytes, size_t size, MessageVisitor& visitor) noexcept
    {
        if (bytes == nullptr || size == 0 || size > MaximumPacketBytes || (size & 3) != 0)
        {
            visitor.OnUnreadable(ReadProblem::Malformed);
            return ReadProblem::Malformed;
        }

        // Large, so it lives here once rather than once for each level of a bundle.
        MessageView scratch{};

        return ReadElement(bytes, size, Immediately, 0, visitor, scratch);
    }

    _Use_decl_annotations_
    MessageBuilder::MessageBuilder(std::string_view address) :
        m_address{ address }
    {
    }

    _Use_decl_annotations_
    MessageBuilder& MessageBuilder::Int(int32_t value)
    {
        m_typeTags += type::Int32;
        WriteUInt32(m_arguments, static_cast<uint32_t>(value));
        return *this;
    }

    _Use_decl_annotations_
    MessageBuilder& MessageBuilder::Float(float value)
    {
        m_typeTags += type::Float32;
        WriteUInt32(m_arguments, std::bit_cast<uint32_t>(value));
        return *this;
    }

    _Use_decl_annotations_
    MessageBuilder& MessageBuilder::String(std::string_view value)
    {
        m_typeTags += type::String;
        WritePaddedString(m_arguments, value);
        return *this;
    }

    _Use_decl_annotations_
    MessageBuilder& MessageBuilder::Blob(std::span<uint8_t const> value)
    {
        m_typeTags += type::Blob;
        WriteUInt32(m_arguments, static_cast<uint32_t>(value.size()));
        m_arguments.insert(m_arguments.end(), value.begin(), value.end());
        m_arguments.resize(m_arguments.size() + (((value.size() + 3) & ~size_t{ 3 }) - value.size()), 0);
        return *this;
    }

    _Use_decl_annotations_
    MessageBuilder& MessageBuilder::Midi(uint8_t port, uint8_t status, uint8_t data1, uint8_t data2)
    {
        m_typeTags += type::Midi;
        m_arguments.push_back(port);
        m_arguments.push_back(status);
        m_arguments.push_back(data1);
        m_arguments.push_back(data2);
        return *this;
    }

    _Use_decl_annotations_
    MessageBuilder& MessageBuilder::Bool(bool value)
    {
        m_typeTags += value ? type::True : type::False;
        return *this;
    }

    size_t MessageBuilder::Size() const noexcept
    {
        return PaddedStringSize(m_address.size()) + PaddedStringSize(m_typeTags.size()) + m_arguments.size();
    }

    _Use_decl_annotations_
    void MessageBuilder::AppendTo(std::vector<uint8_t>& packet) const
    {
        WritePaddedString(packet, m_address);
        WritePaddedString(packet, m_typeTags);
        packet.insert(packet.end(), m_arguments.begin(), m_arguments.end());
    }

    _Use_decl_annotations_
    std::vector<uint8_t> MakePacket(MessageBuilder const& message)
    {
        std::vector<uint8_t> packet{};
        packet.reserve(message.Size());
        message.AppendTo(packet);
        return packet;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> MakeBundle(uint64_t timeTag, std::span<MessageBuilder const> messages)
    {
        std::vector<uint8_t> packet{};

        packet.insert(packet.end(), std::begin(BundleTag), std::end(BundleTag));
        WriteUInt64(packet, timeTag);

        for (auto const& message : messages)
        {
            WriteUInt32(packet, static_cast<uint32_t>(message.Size()));
            message.AppendTo(packet);
        }

        return packet;
    }
}
