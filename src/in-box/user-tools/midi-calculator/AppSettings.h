// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiAppSettings.h"

#include "CalcPitch.h"
#include "CalcTempo.h"

namespace midicalculator
{
    // appearance and window placement live in the shared base, but stay reachable through the
    // app's own namespace so call sites read the same
    using midiapp::AppTheme;
    using midiapp::WindowBackdrop;
    using midiapp::WindowPlacementInfo;

    // The pages, in the order the navigation shows them. The value is saved, so only add.
    enum class CalculatorPage : int32_t
    {
        Tempo = 0,
        Pitch = 1,
        Midi2 = 2,
        WindowsTime = 3,
        History = 4
    };

    class AppSettings : public midiapp::MidiAppSettings
    {
    public:
        static AppSettings& Current() noexcept;

        void Load() noexcept;

        // ---- the session values every page reads ----

        double Tempo() const noexcept { return m_tempo; }
        void Tempo(double value) noexcept;

        midicalc::Meter Meter() const noexcept { return m_meter; }
        void Meter(midicalc::Meter value) noexcept;

        double SampleRate() const noexcept { return m_sampleRate; }
        void SampleRate(double value) noexcept;

        double ReferenceHertz() const noexcept { return m_referenceHertz; }
        void ReferenceHertz(double value) noexcept;

        void ResetSession() noexcept;

        // ---- tuning and naming ----

        midicalc::Temperament Temperament() const noexcept { return m_temperament; }
        void Temperament(midicalc::Temperament value) noexcept;

        int32_t TemperamentRoot() const noexcept { return m_temperamentRoot; }
        void TemperamentRoot(int32_t value) noexcept;

        // 3 unless the customer changed it, the same default as the Windows MIDI Services API
        uint8_t MiddleCOctave() const noexcept { return m_middleCOctave; }
        void MiddleCOctave(uint8_t value) noexcept;

        midicalc::Tuning Tuning() const noexcept { return { m_referenceHertz, m_temperament, m_temperamentRoot }; }

        // ---- everything else ----

        uint32_t TicksPerQuarterNote() const noexcept { return m_ticksPerQuarterNote; }
        void TicksPerQuarterNote(uint32_t value) noexcept;

        CalculatorPage LastPage() const noexcept { return m_lastPage; }
        void LastPage(CalculatorPage value) noexcept;

        static constexpr double DefaultTempo = 120.0;
        static constexpr double DefaultSampleRate = 48000.0;
        static constexpr double MinimumSampleRate = 1000.0;
        static constexpr double MaximumSampleRate = 1536000.0;
        static constexpr double MinimumReferenceHertz = 300.0;
        static constexpr double MaximumReferenceHertz = 600.0;

        // the same as the Windows MIDI Sequencer
        static constexpr uint32_t DefaultTicksPerQuarterNote = 960;

    private:
        AppSettings() noexcept;

        void WriteDouble(_In_ std::wstring_view name, _In_ double value) const noexcept;
        double ReadDouble(_In_ std::wstring_view name, _In_ double defaultValue, _In_ double minimum, _In_ double maximum) const noexcept;

        double m_tempo{ DefaultTempo };
        midicalc::Meter m_meter{};
        double m_sampleRate{ DefaultSampleRate };
        double m_referenceHertz{ midicalc::DefaultReferenceHertz };

        midicalc::Temperament m_temperament{ midicalc::Temperament::Equal };
        int32_t m_temperamentRoot{ 0 };
        uint8_t m_middleCOctave{ midicalc::DefaultMiddleCOctave };

        uint32_t m_ticksPerQuarterNote{ DefaultTicksPerQuarterNote };
        CalculatorPage m_lastPage{ CalculatorPage::Tempo };
    };
}
