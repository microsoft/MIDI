// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// OSC packets over a stream, like TCP, where nothing marks where one packet ends and the next
// begins. Pure, for the unit tests.
//
// Two ways to mark them are in use:
//
// - SLIP, which the OSC 1.1 paper recommends: each packet between END bytes (0xC0), with any END
//   or ESC (0xDB) byte inside it escaped, as RFC 1055 says. A reader can find the next packet after
//   a damaged one.
// - Size first, as OSC 1.0 says: each packet after its length in bytes, as a 32-bit big-endian
//   integer. A damaged length loses everything after it.
//
// What arrives is untrusted. The reader holds at most one packet of MaximumPacketBytes, and drops
// anything longer.

#include <sal.h>

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace midipatchbay::osc
{
    enum class StreamFraming : int32_t
    {
        Slip = 0,
        SizeFirst = 1,
    };

    namespace slip
    {
        constexpr uint8_t End = 0xC0;
        constexpr uint8_t Escape = 0xDB;
        constexpr uint8_t EscapedEnd = 0xDC;
        constexpr uint8_t EscapedEscape = 0xDD;
    }

    // Adds one packet to out, marked as framing says. SLIP puts an END before it as well as after,
    // as the OSC 1.1 paper does, so line noise before it is a packet of its own and is dropped.
    void AppendFramed(_In_ StreamFraming framing, _In_ std::span<uint8_t const> packet, _Inout_ std::vector<uint8_t>& out);

    enum class StreamProblem : int32_t
    {
        None = 0,

        // A packet longer than MaximumPacketBytes was dropped. SLIP carries on with the next one.
        TooLong = 1,

        // A size of 0 or past MaximumPacketBytes. Nothing after it can be found, so the stream is
        // no use.
        BadSize = 2,
    };

    class StreamReader
    {
    public:
        explicit StreamReader(_In_ StreamFraming framing) noexcept;

        // Reads what arrived, calling onPacket with each whole packet, which is only valid during
        // the call. Returns BadSize when the stream can't be read any further, and TooLong when a
        // packet was dropped. Keeps whatever is left over for next time.
        StreamProblem Read(
            _In_ std::span<uint8_t const> bytes,
            _In_ std::function<void(std::span<uint8_t const>)> const& onPacket);

        void Reset() noexcept;

    private:
        StreamFraming m_framing{ StreamFraming::Slip };

        std::vector<uint8_t> m_packet{};

        // SLIP: the byte before was ESC. Or the packet is too long, and is skipped to the next END.
        bool m_escaped{ false };
        bool m_skipping{ false };

        // Size first: the bytes of the size read so far, and the size once all four are.
        uint8_t m_sizeBytes{ 0 };
        uint32_t m_size{ 0 };
    };
}
