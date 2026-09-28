// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A bounds-checked multicast DNS reader, written separately from the code under test so the
// announcement tests check the bytes against an independent reading of RFC 1035 and RFC 6762.

namespace RtpMidiTest::Mdns
{
    // pos moves past the name as it appears in place, even when it ends in a pointer
    inline bool ReadName(uint8_t const* message, size_t const size, size_t& pos, std::string& name)
    {
        name.clear();

        size_t cursor = pos;
        bool jumped = false;
        int jumps = 0;

        for (;;)
        {
            if (cursor >= size) return false;

            auto const length = message[cursor];

            if (length == 0)
            {
                if (!jumped) pos = cursor + 1;
                return true;
            }

            if ((length & 0xC0) == 0xC0)
            {
                if (cursor + 1 >= size) return false;

                auto const target = (static_cast<size_t>(length & 0x3F) << 8) | message[cursor + 1];
                if (!jumped) pos = cursor + 2;
                jumped = true;

                if (++jumps > 32 || target >= size) return false;
                cursor = target;
                continue;
            }

            if ((length & 0xC0) != 0) return false;
            if (cursor + 1 + length > size) return false;

            if (!name.empty()) name += '.';
            name.append(reinterpret_cast<char const*>(message + cursor + 1), length);

            if (name.size() > 1024) return false;

            cursor += 1u + length;
        }
    }

    struct Record
    {
        std::string Section;
        std::string Name;
        uint16_t Type{ 0 };

        // cache-flush in a response record
        bool TopBit{ false };

        uint32_t Ttl{ 0 };

        // "-> target" for a PTR record, otherwise the length
        std::string Data;
    };

    struct Message
    {
        uint16_t Id{ 0 };
        uint16_t Flags{ 0 };
        bool IsResponse{ false };
        std::vector<Record> Records;
    };

    inline bool Parse(uint8_t const* message, size_t const size, Message& parsed)
    {
        if (size < 12) return false;

        auto const read16 = [&](size_t const at) { return static_cast<uint16_t>((message[at] << 8) | message[at + 1]); };

        parsed = Message{};
        parsed.Id = read16(0);
        parsed.Flags = read16(2);
        parsed.IsResponse = (parsed.Flags & 0x8000) != 0;

        uint16_t const counts[4] = { read16(4), read16(6), read16(8), read16(10) };
        static char const* const sections[4] = { "qd", "an", "ns", "ar" };

        size_t pos = 12;

        for (int section = 0; section < 4; section++)
        {
            for (uint16_t i = 0; i < counts[section]; i++)
            {
                Record record{};
                record.Section = sections[section];

                if (!ReadName(message, size, pos, record.Name)) return false;

                if (section == 0)
                {
                    if (pos + 4 > size) return false;
                    record.Type = read16(pos);
                    record.TopBit = (read16(pos + 2) & 0x8000) != 0;
                    pos += 4;
                }
                else
                {
                    if (pos + 10 > size) return false;
                    record.Type = read16(pos);
                    record.TopBit = (read16(pos + 2) & 0x8000) != 0;
                    record.Ttl = (static_cast<uint32_t>(read16(pos + 4)) << 16) | read16(pos + 6);
                    auto const length = read16(pos + 8);
                    pos += 10;
                    if (pos + length > size) return false;

                    if (record.Type == 12)
                    {
                        size_t target = pos;
                        std::string pointsAt;
                        if (!ReadName(message, size, target, pointsAt)) return false;
                        record.Data = "-> " + pointsAt;
                    }
                    else
                    {
                        record.Data = "(" + std::to_string(length) + " bytes)";
                    }

                    pos += length;
                }

                parsed.Records.push_back(std::move(record));
            }
        }

        return true;
    }
}
