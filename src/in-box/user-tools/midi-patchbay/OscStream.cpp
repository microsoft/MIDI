// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "OscStream.h"
#include "OscPacket.h"

#include <algorithm>

namespace midipatchbay::osc
{
    _Use_decl_annotations_
    void AppendFramed(StreamFraming framing, std::span<uint8_t const> packet, std::vector<uint8_t>& out)
    {
        if (framing == StreamFraming::SizeFirst)
        {
            auto const size = static_cast<uint32_t>(packet.size());

            out.push_back(static_cast<uint8_t>(size >> 24));
            out.push_back(static_cast<uint8_t>(size >> 16));
            out.push_back(static_cast<uint8_t>(size >> 8));
            out.push_back(static_cast<uint8_t>(size));
            out.insert(out.end(), packet.begin(), packet.end());

            return;
        }

        out.push_back(slip::End);

        for (auto const byte : packet)
        {
            if (byte == slip::End)
            {
                out.push_back(slip::Escape);
                out.push_back(slip::EscapedEnd);
            }
            else if (byte == slip::Escape)
            {
                out.push_back(slip::Escape);
                out.push_back(slip::EscapedEscape);
            }
            else
            {
                out.push_back(byte);
            }
        }

        out.push_back(slip::End);
    }

    _Use_decl_annotations_
    StreamReader::StreamReader(StreamFraming framing) noexcept :
        m_framing{ framing }
    {
    }

    void StreamReader::Reset() noexcept
    {
        m_packet.clear();
        m_escaped = false;
        m_skipping = false;
        m_sizeBytes = 0;
        m_size = 0;
    }

    _Use_decl_annotations_
    StreamProblem StreamReader::Read(std::span<uint8_t const> bytes, std::function<void(std::span<uint8_t const>)> const& onPacket)
    {
        auto problem = StreamProblem::None;

        if (m_framing == StreamFraming::SizeFirst)
        {
            size_t position{ 0 };

            while (position < bytes.size())
            {
                if (m_sizeBytes < 4)
                {
                    m_size = (m_size << 8) | bytes[position++];

                    if (++m_sizeBytes < 4)
                    {
                        continue;
                    }

                    if (m_size == 0 || m_size > MaximumPacketBytes)
                    {
                        Reset();
                        return StreamProblem::BadSize;
                    }

                    m_packet.reserve(m_size);
                    continue;
                }

                auto const take = (std::min)(bytes.size() - position, static_cast<size_t>(m_size) - m_packet.size());
                m_packet.insert(m_packet.end(), bytes.begin() + position, bytes.begin() + position + take);
                position += take;

                if (m_packet.size() == m_size)
                {
                    onPacket(m_packet);

                    m_packet.clear();
                    m_sizeBytes = 0;
                    m_size = 0;
                }
            }

            return problem;
        }

        for (auto const byte : bytes)
        {
            if (byte == slip::End)
            {
                // An END with nothing before it starts a packet, or is noise: either way, nothing.
                if (!m_skipping && !m_packet.empty())
                {
                    onPacket(m_packet);
                }

                m_packet.clear();
                m_escaped = false;
                m_skipping = false;
                continue;
            }

            if (m_skipping)
            {
                continue;
            }

            auto value = byte;

            if (m_escaped)
            {
                m_escaped = false;

                // RFC 1055 keeps the byte after an ESC that isn't one of the two escapes as it is.
                value = byte == slip::EscapedEnd ? slip::End : byte == slip::EscapedEscape ? slip::Escape : byte;
            }
            else if (byte == slip::Escape)
            {
                m_escaped = true;
                continue;
            }

            if (m_packet.size() >= MaximumPacketBytes)
            {
                m_packet.clear();
                m_skipping = true;
                problem = StreamProblem::TooLong;
                continue;
            }

            m_packet.push_back(value);
        }

        return problem;
    }
}
