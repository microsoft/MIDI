// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Holds what one network connection sends to a chosen multiple of MIDI 1.0 wire speed, for the
// Network MIDI 2.0 and RTP-MIDI transports. A device which passes network MIDI on to a DIN
// cable can only take it at that speed, and neither protocol gives it a way to say so.
//
// Nothing is held back unless sending it at once would go over the chosen speed. A message on a
// quiet connection always goes immediately. Times are the caller's clock: the same counter and
// frequency as MIDI timestamps. Nothing here waits or locks; the caller does both.
// ============================================================================

#pragma once

#include <sal.h>

#include <algorithm>
#include <cstdint>

namespace WindowsMidiServicesInternal
{
    // 31,250 bits a second, and a MIDI 1.0 byte takes ten of them on the cable
    constexpr uint32_t MidiWireSpeedBytesPerSecond = 3125;

    // About a megabit. Anything faster is no limit at all.
    constexpr uint32_t MidiSendSpeedMaxMultiple = 32;

    // What may go out at once after a quiet moment, for each multiple of wire speed. At wire
    // speed that is 20 ms of the cable, more than any chord.
    constexpr uint32_t MidiSendSpeedAllowanceBytesPerMultiple = 64;

    // A speed limit read from configuration. 0 is no limit, and so is anything above the maximum.
    inline uint32_t ClampMidiSendSpeedMultiple(_In_ uint32_t const multiple) noexcept
    {
        return multiple > MidiSendSpeedMaxMultiple ? 0 : multiple;
    }

    // Roughly what one UMP message takes on a MIDI 1.0 cable. It only paces the connection, so it
    // does not have to be exact.
    inline uint32_t EstimateMidi1WireByteCount(_In_ uint32_t const firstWord, _In_ uint32_t const wordCount) noexcept
    {
        switch (firstWord >> 28)
        {
        case 0x0:
            // NOOP and JR timestamps never reach a cable
            return 0;

        case 0x1:
            switch ((firstWord >> 16) & 0xFF)
            {
            case 0xF1:
            case 0xF3:
                return 2;

            case 0xF2:
                return 3;

            default:
                return 1;
            }

        case 0x2:
        {
            auto const status = (firstWord >> 20) & 0x0F;

            return (status == 0xC || status == 0xD) ? 2 : 3;
        }

        case 0x3:
        {
            // complete, start, continue, end
            auto const form = (firstWord >> 20) & 0x0F;
            auto const dataBytes = (std::min)((firstWord >> 16) & 0x0Fu, 6u);

            return dataBytes +
                ((form == 0 || form == 1) ? 1u : 0u) +
                ((form == 0 || form == 3) ? 1u : 0u);
        }

        case 0x4:
            // becomes a single MIDI 1.0 message
            return 3;

        default:
            return wordCount * 3;
        }
    }


    // The send budget for one connection. Not thread safe.
    class MidiSendPacer
    {
    public:
        // A multiple of MIDI 1.0 wire speed, 0 for no limit
        void Configure(_In_ uint32_t const speedMultiple, _In_ uint64_t const ticksPerSecond) noexcept
        {
            auto const multiple = ClampMidiSendSpeedMultiple(speedMultiple);

            m_ticksPerSecond = ticksPerSecond;
            m_bytesPerSecond = multiple * MidiWireSpeedBytesPerSecond;
            m_allowanceTicks = TicksFor(static_cast<uint64_t>(multiple) * MidiSendSpeedAllowanceBytesPerMultiple);
        }

        bool IsLimited() const noexcept { return m_bytesPerSecond != 0 && m_ticksPerSecond != 0; }

        // How long until a message of this many bytes may go. Zero is now. Check and charge a
        // batch a message at a time: one message larger than the allowance goes as soon as the
        // allowance is full, so a whole batch checked as one would go at once.
        uint64_t TicksUntilAllowed(_In_ uint64_t const byteCount, _In_ uint64_t const now) const noexcept
        {
            if (!IsLimited() || byteCount == 0)
            {
                return 0;
            }

            // more than the whole allowance in one piece goes as soon as the allowance is full
            auto const needed = (std::max)(m_busyUntil, now) + (std::min)(TicksFor(byteCount), m_allowanceTicks);
            auto const allowedUpTo = now + m_allowanceTicks;

            return needed <= allowedUpTo ? 0 : needed - allowedUpTo;
        }

        // Records that this many bytes went at this time
        void Charge(_In_ uint64_t const byteCount, _In_ uint64_t const now) noexcept
        {
            if (!IsLimited() || byteCount == 0)
            {
                return;
            }

            m_busyUntil = (std::max)(m_busyUntil, now) + TicksFor(byteCount);
        }

        // Forgets what was sent, as at the start of a session
        void Reset() noexcept { m_busyUntil = 0; }

    private:
        uint64_t TicksFor(_In_ uint64_t const byteCount) const noexcept
        {
            return m_bytesPerSecond == 0 ? 0 : (byteCount * m_ticksPerSecond) / m_bytesPerSecond;
        }

        uint64_t m_ticksPerSecond{ 0 };
        uint32_t m_bytesPerSecond{ 0 };
        uint64_t m_allowanceTicks{ 0 };

        // when everything charged so far would have finished going out at the chosen speed
        uint64_t m_busyUntil{ 0 };
    };


    // Slows a connection down while the remote keeps reporting lost data, one halving at a time
    // and never below MIDI 1.0 wire speed, then speeds it back up to the chosen limit once the
    // losses stop. Not thread safe.
    class MidiAutomaticSendSpeed
    {
    public:
        // The requests for one burst of loss arrive close together and count as one
        static constexpr uint64_t LossHoldoffMilliseconds = 500;

        // How long without loss before trying the next speed up. Doubled each time a speed up is
        // followed by loss, so a remote that cannot take a speed is not offered it every few
        // seconds, and reset once a long time passes without loss.
        static constexpr uint64_t MinimumRaiseDelaySeconds = 10;
        static constexpr uint64_t MaximumRaiseDelaySeconds = 320;
        static constexpr uint64_t RaiseDelayResetSeconds = 600;

        // The speed the customer chose, 0 for no limit
        void Configure(_In_ uint32_t const chosenMultiple, _In_ bool const enabled) noexcept
        {
            m_chosen = ClampMidiSendSpeedMultiple(chosenMultiple);
            m_enabled = enabled;
            m_current = m_chosen;
            m_raiseDelaySeconds = MinimumRaiseDelaySeconds;
            m_lastLoss = 0;
            m_lastChange = 0;
            m_lastStepDown = 0;
            m_lastChangeWasRaise = false;
        }

        bool IsEnabled() const noexcept { return m_enabled; }

        // What the connection should send at now, 0 for no limit
        uint32_t CurrentMultiple() const noexcept { return m_current; }

        // The remote asked for data again. True when the speed changed.
        bool OnLoss(_In_ uint64_t const now, _In_ uint64_t const ticksPerSecond) noexcept
        {
            if (!m_enabled)
            {
                return false;
            }

            m_lastLoss = now;

            if (m_lastStepDown != 0 && now - m_lastStepDown < Ticks(LossHoldoffMilliseconds, 1000, ticksPerSecond))
            {
                return false;
            }

            if (m_current == 1)
            {
                return false;
            }

            if (m_lastChangeWasRaise && now - m_lastChange < Ticks(m_raiseDelaySeconds, 1, ticksPerSecond))
            {
                m_raiseDelaySeconds = (std::min)(m_raiseDelaySeconds * 2, MaximumRaiseDelaySeconds);
            }

            m_current = (m_current == 0) ? MidiSendSpeedMaxMultiple : (std::max)(m_current / 2, 1u);
            m_lastChange = now;
            m_lastStepDown = now;
            m_lastChangeWasRaise = false;

            return true;
        }

        // Called now and then. True when the speed changed.
        bool OnTick(_In_ uint64_t const now, _In_ uint64_t const ticksPerSecond) noexcept
        {
            if (!m_enabled || m_current == m_chosen)
            {
                return false;
            }

            auto const quietSince = (std::max)(m_lastLoss, m_lastChange);

            if (now - quietSince < Ticks(m_raiseDelaySeconds, 1, ticksPerSecond))
            {
                return false;
            }

            if (now - m_lastLoss >= Ticks(RaiseDelayResetSeconds, 1, ticksPerSecond))
            {
                m_raiseDelaySeconds = MinimumRaiseDelaySeconds;
            }

            auto const higher = m_current * 2;

            if (m_chosen == 0)
            {
                m_current = higher > MidiSendSpeedMaxMultiple ? 0 : higher;
            }
            else
            {
                m_current = (std::min)(higher, m_chosen);
            }

            m_lastChange = now;
            m_lastChangeWasRaise = true;

            return true;
        }

    private:
        static uint64_t Ticks(_In_ uint64_t const amount, _In_ uint64_t const unitsPerSecond, _In_ uint64_t const ticksPerSecond) noexcept
        {
            return (amount * ticksPerSecond) / unitsPerSecond;
        }

        uint32_t m_chosen{ 0 };
        uint32_t m_current{ 0 };
        bool m_enabled{ false };

        uint64_t m_raiseDelaySeconds{ MinimumRaiseDelaySeconds };
        uint64_t m_lastLoss{ 0 };
        uint64_t m_lastChange{ 0 };
        uint64_t m_lastStepDown{ 0 };
        bool m_lastChangeWasRaise{ false };
    };
}
