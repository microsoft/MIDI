// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "LfoSweep.h"
#include "LfoWave.h"

#include <algorithm>
#include <cmath>

namespace midiapp
{
    _Use_decl_annotations_
    void LfoSweep::Begin(
        uint64_t origin,
        uint64_t unitsPerSecond,
        double beatsPerCycle,
        double beatsPerMinute,
        int32_t intervalMilliseconds) noexcept
    {
        m_origin = origin;
        m_unitsPerSecond = unitsPerSecond == 0 ? 1'000'000 : unitsPerSecond;
        m_samplesSent = 0;
        m_phaseAtOrigin = 0.0;

        m_beatsPerCycle = std::clamp(beatsPerCycle, MinimumLfoBeatsPerCycle, MaximumLfoBeatsPerCycle);
        m_beatsPerMinute = std::clamp(beatsPerMinute, MinimumLfoBeatsPerMinute, MaximumLfoBeatsPerMinute);
        m_intervalMilliseconds = std::clamp(intervalMilliseconds, MinimumLfoIntervalMilliseconds, MaximumLfoIntervalMilliseconds);
    }

    uint64_t LfoSweep::Interval() const noexcept
    {
        return static_cast<uint64_t>(m_intervalMilliseconds) * m_unitsPerSecond / 1000ull;
    }

    uint64_t LfoSweep::NextDue() const noexcept
    {
        return m_origin + m_samplesSent * Interval();
    }

    _Use_decl_annotations_
    double LfoSweep::PhaseOf(uint64_t sampleIndex) const noexcept
    {
        auto const cycleUnits = 60.0 * static_cast<double>(m_unitsPerSecond) * m_beatsPerCycle / m_beatsPerMinute;

        if (!(cycleUnits > 0.0))
        {
            return 0.0;
        }

        auto const elapsed = static_cast<double>(sampleIndex * Interval());
        auto const turns = m_phaseAtOrigin + elapsed / cycleUnits;

        return turns - std::floor(turns);
    }

    double LfoSweep::NextPhase() const noexcept
    {
        return PhaseOf(m_samplesSent);
    }

    void LfoSweep::Advance() noexcept
    {
        m_samplesSent++;
    }

    _Use_decl_annotations_
    void LfoSweep::Retime(double beatsPerCycle, double beatsPerMinute, int32_t intervalMilliseconds) noexcept
    {
        // Rebased on the sample that is due next, keeping the phase it had reached. Re-timing the
        // whole history at the new rate would jump the wave.
        m_phaseAtOrigin = PhaseOf(m_samplesSent);
        m_origin = NextDue();
        m_samplesSent = 0;

        m_beatsPerCycle = std::clamp(beatsPerCycle, MinimumLfoBeatsPerCycle, MaximumLfoBeatsPerCycle);
        m_beatsPerMinute = std::clamp(beatsPerMinute, MinimumLfoBeatsPerMinute, MaximumLfoBeatsPerMinute);
        m_intervalMilliseconds = std::clamp(intervalMilliseconds, MinimumLfoIntervalMilliseconds, MaximumLfoIntervalMilliseconds);
    }

    _Use_decl_annotations_
    bool LfoSweep::CatchUp(uint64_t now) noexcept
    {
        auto const due = NextDue();

        if (due > now || now - due <= Interval() * 8)
        {
            return false;
        }

        m_phaseAtOrigin = PhaseOf(m_samplesSent);
        m_origin = now;
        m_samplesSent = 0;

        return true;
    }
}
