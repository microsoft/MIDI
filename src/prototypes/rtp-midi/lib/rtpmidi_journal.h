// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. RTP-MIDI recovery journal (RFC 6295 section 5, appendices A and B).
//
// The parser walks the whole structure and bounds-checks every LENGTH field, and extracts the
// chapters a receiver needs to undo the damage a lost packet does: which notes are on or off
// (N), controller values (C), pitch wheel (W) and program (P). The writer covers the same
// subset, which is enough to test the parser and to send a useful journal of our own.
// ============================================================================

#pragma once

#include "rtpmidi_protocol.h"

namespace RtpMidi
{
    // Channel journal table of contents, in the order the chapters appear.
    constexpr uint8_t ChapterP = 0x80;
    constexpr uint8_t ChapterC = 0x40;
    constexpr uint8_t ChapterM = 0x20;
    constexpr uint8_t ChapterW = 0x10;
    constexpr uint8_t ChapterN = 0x08;
    constexpr uint8_t ChapterE = 0x04;
    constexpr uint8_t ChapterT = 0x02;
    constexpr uint8_t ChapterA = 0x01;

    struct JournalNoteOn
    {
        uint8_t Note{ 0 };
        uint8_t Velocity{ 0 };
        bool PlayHint{ false };     // the Y bit: the sender suggests playing a recovered note on
        bool InPreviousPacket{ false }; // writer only: clears the S bit
    };

    struct JournalController
    {
        uint8_t Number{ 0 };
        bool IsValue{ true };       // A = 0: Value holds the controller value
        uint8_t Value{ 0 };
        bool IsCount{ false };      // A = 1: T picks the count tool (1) or the toggle tool (0)
        uint8_t Alt{ 0 };
    };

    struct ChannelJournal
    {
        uint8_t Channel{ 0 };
        uint8_t Toc{ 0 };

        bool HasProgram{ false };
        uint8_t Program{ 0 };
        bool HasBank{ false };
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };

        std::vector<JournalController> Controllers;

        bool HasPitchWheel{ false };
        uint8_t PitchLsb{ 0 };
        uint8_t PitchMsb{ 0 };

        std::vector<JournalNoteOn> NoteOns;
        std::vector<uint8_t> NoteOffs;
        bool NoteOffInPreviousPacket{ false };  // writer only: clears B, the S bit of the off bits

        bool HasChannelPressure{ false };
        uint8_t ChannelPressure{ 0 };
    };

    struct RecoveryJournal
    {
        bool SinglePacketLoss{ false };
        bool HasSystemJournal{ false };
        bool HasChannelJournals{ false };
        bool EnhancedChapterC{ false };
        uint16_t CheckpointSequence{ 0 };
        uint8_t SystemChapters{ 0 };    // D V Q F X in the low five bits
        std::vector<ChannelJournal> Channels;
    };

    inline bool ParseChannelChapters(uint8_t const* data, size_t size, ChannelJournal& channel)
    {
        Reader reader{ data, size };
        auto const toc = channel.Toc;

        if (toc & ChapterP)
        {
            uint8_t program{ 0 }, bankMsb{ 0 }, bankLsb{ 0 };
            if (!reader.U8(program) || !reader.U8(bankMsb) || !reader.U8(bankLsb)) return false;

            channel.HasProgram = true;
            channel.Program = program & 0x7F;
            channel.HasBank = (bankMsb & 0x80) != 0;
            channel.BankMsb = bankMsb & 0x7F;
            channel.BankLsb = bankLsb & 0x7F;
        }

        if (toc & ChapterC)
        {
            uint8_t header{ 0 };
            if (!reader.U8(header)) return false;

            size_t const logCount = static_cast<size_t>(header & 0x7F) + 1;

            for (size_t i = 0; i < logCount; i++)
            {
                uint8_t number{ 0 }, value{ 0 };
                if (!reader.U8(number) || !reader.U8(value)) return false;

                JournalController log{};
                log.Number = number & 0x7F;

                if ((value & 0x80) == 0)
                {
                    log.IsValue = true;
                    log.Value = value & 0x7F;
                }
                else
                {
                    log.IsValue = false;
                    log.IsCount = (value & 0x40) != 0;
                    log.Alt = value & 0x3F;
                }

                channel.Controllers.push_back(log);
            }
        }

        if (toc & ChapterM)
        {
            // its LENGTH covers the whole chapter, header included
            uint8_t first{ 0 }, second{ 0 };
            if (!reader.U8(first) || !reader.U8(second)) return false;

            size_t const length = (static_cast<size_t>(first & 0x03) << 8) | second;
            if (length < 2 || !reader.Skip(length - 2)) return false;
        }

        if (toc & ChapterW)
        {
            uint8_t first{ 0 }, second{ 0 };
            if (!reader.U8(first) || !reader.U8(second)) return false;

            channel.HasPitchWheel = true;
            channel.PitchLsb = first & 0x7F;
            channel.PitchMsb = second & 0x7F;
        }

        if (toc & ChapterN)
        {
            uint8_t first{ 0 }, second{ 0 };
            if (!reader.U8(first) || !reader.U8(second)) return false;

            size_t logCount = first & 0x7F;
            uint8_t const low = second >> 4;
            uint8_t const high = second & 0x0F;

            // LEN 127 with LOW 15 and HIGH 0 is the one way to say 128 note logs
            if (logCount == 127 && low == 15 && high == 0) logCount = 128;

            for (size_t i = 0; i < logCount; i++)
            {
                uint8_t note{ 0 }, velocity{ 0 };
                if (!reader.U8(note) || !reader.U8(velocity)) return false;

                channel.NoteOns.push_back(JournalNoteOn{ static_cast<uint8_t>(note & 0x7F), static_cast<uint8_t>(velocity & 0x7F), (velocity & 0x80) != 0 });
            }

            size_t offBitsBytes = 0;

            if (low <= high)
            {
                offBitsBytes = static_cast<size_t>(high - low) + 1;
            }
            else if (!(low == 15 && (high == 0 || high == 1)))
            {
                return false;
            }

            for (size_t i = 0; i < offBitsBytes; i++)
            {
                uint8_t bits{ 0 };
                if (!reader.U8(bits)) return false;

                // most significant bit is the lowest note of the eight
                for (int bit = 0; bit < 8; bit++)
                {
                    if (bits & (0x80 >> bit))
                    {
                        auto const note = static_cast<size_t>(low + i) * 8 + static_cast<size_t>(bit);
                        if (note < 128) channel.NoteOffs.push_back(static_cast<uint8_t>(note));
                    }
                }
            }
        }

        if (toc & ChapterE)
        {
            uint8_t header{ 0 };
            if (!reader.U8(header)) return false;
            if (!reader.Skip((static_cast<size_t>(header & 0x7F) + 1) * 2)) return false;
        }

        if (toc & ChapterT)
        {
            uint8_t pressure{ 0 };
            if (!reader.U8(pressure)) return false;

            channel.HasChannelPressure = true;
            channel.ChannelPressure = pressure & 0x7F;
        }

        if (toc & ChapterA)
        {
            uint8_t header{ 0 };
            if (!reader.U8(header)) return false;
            if (!reader.Skip((static_cast<size_t>(header & 0x7F) + 1) * 2)) return false;
        }

        return true;
    }

    inline bool ParseRecoveryJournal(uint8_t const* data, size_t size, RecoveryJournal& journal)
    {
        journal = RecoveryJournal{};

        if (data == nullptr) return false;

        Reader reader{ data, size };

        uint8_t flags{ 0 };
        if (!reader.U8(flags) || !reader.U16(journal.CheckpointSequence)) return false;

        journal.SinglePacketLoss = (flags & 0x80) != 0;
        journal.HasSystemJournal = (flags & 0x40) != 0;
        journal.HasChannelJournals = (flags & 0x20) != 0;
        journal.EnhancedChapterC = (flags & 0x10) != 0;

        size_t const channelJournalCount = static_cast<size_t>(flags & 0x0F) + 1;

        if (journal.HasSystemJournal)
        {
            if (reader.Remaining() < 2) return false;

            auto const header = reader.Current();
            size_t const length = (static_cast<size_t>(header[0] & 0x03) << 8) | header[1];

            if (length < 2 || !reader.Skip(length)) return false;

            journal.SystemChapters = static_cast<uint8_t>((header[0] >> 2) & 0x1F);
        }

        if (journal.HasChannelJournals)
        {
            for (size_t i = 0; i < channelJournalCount; i++)
            {
                if (reader.Remaining() < 3) return false;

                auto const header = reader.Current();
                size_t const length = (static_cast<size_t>(header[0] & 0x03) << 8) | header[1];

                if (length < 3 || length > reader.Remaining()) return false;

                ChannelJournal channel{};
                channel.Channel = static_cast<uint8_t>((header[0] >> 3) & 0x0F);
                channel.Toc = header[2];

                if (!ParseChannelChapters(header + 3, length - 3, channel)) return false;

                journal.Channels.push_back(std::move(channel));

                reader.Skip(length);
            }
        }

        return true;
    }

    // Writes chapters P, C (value tool only), W and N. An S bit is cleared on anything that came
    // from the packet just before this one, and on every level above it, because a receiver that
    // lost exactly one packet looks only at S = 0 elements (RFC 6295 appendix A.1).
    inline std::vector<uint8_t> BuildRecoveryJournal(uint16_t checkpointSequence, std::vector<ChannelJournal> const& channels)
    {
        std::vector<uint8_t> out;

        auto const channelCount = (std::min)(channels.size(), static_cast<size_t>(16));

        bool anyFromPreviousPacket = false;
        for (size_t index = 0; index < channelCount; index++)
        {
            auto const& channel = channels[index];
            if (channel.NoteOffInPreviousPacket) anyFromPreviousPacket = true;
            for (auto const& log : channel.NoteOns) if (log.InPreviousPacket) anyFromPreviousPacket = true;
        }

        uint8_t flags = anyFromPreviousPacket ? 0x00 : 0x80;
        if (channelCount > 0) flags |= static_cast<uint8_t>(0x20 | (channelCount - 1));

        PutU8(out, flags);
        PutU16(out, checkpointSequence);

        for (size_t index = 0; index < channelCount; index++)
        {
            auto const& channel = channels[index];

            bool channelFromPreviousPacket = channel.NoteOffInPreviousPacket;
            for (auto const& log : channel.NoteOns) if (log.InPreviousPacket) channelFromPreviousPacket = true;

            std::vector<uint8_t> chapters;
            uint8_t toc = 0;

            if (channel.HasProgram)
            {
                toc |= ChapterP;
                chapters.push_back(static_cast<uint8_t>(0x80 | (channel.Program & 0x7F)));
                chapters.push_back(static_cast<uint8_t>((channel.HasBank ? 0x80 : 0x00) | (channel.BankMsb & 0x7F)));
                chapters.push_back(static_cast<uint8_t>(channel.BankLsb & 0x7F));
            }

            std::vector<JournalController> values;
            for (auto const& log : channel.Controllers) if (log.IsValue) values.push_back(log);
            if (values.size() > 128) values.resize(128);

            if (!values.empty())
            {
                toc |= ChapterC;
                chapters.push_back(static_cast<uint8_t>(0x80 | (values.size() - 1)));

                for (auto const& log : values)
                {
                    chapters.push_back(static_cast<uint8_t>(0x80 | (log.Number & 0x7F)));
                    chapters.push_back(static_cast<uint8_t>(log.Value & 0x7F));
                }
            }

            if (channel.HasPitchWheel)
            {
                toc |= ChapterW;
                chapters.push_back(static_cast<uint8_t>(0x80 | (channel.PitchLsb & 0x7F)));
                chapters.push_back(static_cast<uint8_t>(channel.PitchMsb & 0x7F));
            }

            if (!channel.NoteOns.empty() || !channel.NoteOffs.empty())
            {
                toc |= ChapterN;

                auto const noteOnCount = (std::min)(channel.NoteOns.size(), static_cast<size_t>(127));

                uint8_t low = 15;
                uint8_t high = 0;
                std::array<uint8_t, 16> offBits{};

                for (auto const note : channel.NoteOffs)
                {
                    if (note > 127) continue;
                    auto const octet = static_cast<uint8_t>(note / 8);
                    offBits[octet] |= static_cast<uint8_t>(0x80 >> (note % 8));
                    low = (std::min)(low, octet);
                    high = (std::max)(high, octet);
                }

                bool const haveOffBits = low <= high;

                // with no off bits, LOW 15 HIGH 1 avoids the 128-log meaning of LOW 15 HIGH 0
                if (!haveOffBits) { low = 15; high = (noteOnCount == 127) ? 1 : 0; }

                chapters.push_back(static_cast<uint8_t>((channel.NoteOffInPreviousPacket ? 0x00 : 0x80) | noteOnCount));
                chapters.push_back(static_cast<uint8_t>((low << 4) | high));

                for (size_t i = 0; i < noteOnCount; i++)
                {
                    auto const& log = channel.NoteOns[i];
                    chapters.push_back(static_cast<uint8_t>((log.InPreviousPacket ? 0x00 : 0x80) | (log.Note & 0x7F)));
                    chapters.push_back(static_cast<uint8_t>((log.PlayHint ? 0x80 : 0x00) | (log.Velocity & 0x7F)));
                }

                if (haveOffBits)
                {
                    for (uint8_t octet = low; octet <= high; octet++) chapters.push_back(offBits[octet]);
                }
            }

            size_t const length = 3 + chapters.size();

            PutU8(out, static_cast<uint8_t>((channelFromPreviousPacket ? 0x00 : 0x80) | ((channel.Channel & 0x0F) << 3) | ((length >> 8) & 0x03)));
            PutU8(out, static_cast<uint8_t>(length));
            PutU8(out, toc);
            out.insert(out.end(), chapters.begin(), chapters.end());
        }

        return out;
    }
}
