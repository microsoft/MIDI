// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midicalculator
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midicalculator)";

        constexpr wchar_t ValueTempo[] = L"Tempo";
        constexpr wchar_t ValueMeterNumerator[] = L"MeterNumerator";
        constexpr wchar_t ValueMeterDenominator[] = L"MeterDenominator";
        constexpr wchar_t ValueSampleRate[] = L"SampleRate";
        constexpr wchar_t ValueReferenceHertz[] = L"ReferenceHertz";
        constexpr wchar_t ValueTemperament[] = L"Temperament";
        constexpr wchar_t ValueTemperamentRoot[] = L"TemperamentRoot";
        constexpr wchar_t ValueMiddleCOctave[] = L"MiddleCOctave";
        constexpr wchar_t ValueTicksPerQuarterNote[] = L"TicksPerQuarterNote";
        constexpr wchar_t ValueLastPage[] = L"LastPage";
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    _Use_decl_annotations_
    void AppSettings::WriteDouble(std::wstring_view name, double value) const noexcept
    {
        try
        {
            // a string, because the registry has no floating point type; written with "." whatever
            // the customer's locale, so it reads back the same everywhere
            WriteString(name, std::format(L"{}", value));
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    double AppSettings::ReadDouble(std::wstring_view name, double defaultValue, double minimum, double maximum) const noexcept
    {
        try
        {
            auto const text = ReadString(name, L"");

            if (text.empty())
            {
                return defaultValue;
            }

            wchar_t* end{ nullptr };
            auto const value = std::wcstod(text.c_str(), &end);

            if (end == text.c_str() || !std::isfinite(value) || value < minimum || value > maximum)
            {
                return defaultValue;
            }

            return value;
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        m_tempo = ReadDouble(ValueTempo, DefaultTempo, midicalc::MinimumTempo, midicalc::MaximumTempo);

        midicalc::Meter const meter{
            static_cast<int32_t>(ReadDword(ValueMeterNumerator, 4)),
            static_cast<int32_t>(ReadDword(ValueMeterDenominator, 4)) };
        m_meter = midicalc::IsValidMeter(meter) ? meter : midicalc::Meter{};

        m_sampleRate = ReadDouble(ValueSampleRate, DefaultSampleRate, MinimumSampleRate, MaximumSampleRate);
        m_referenceHertz = ReadDouble(ValueReferenceHertz, midicalc::DefaultReferenceHertz, MinimumReferenceHertz, MaximumReferenceHertz);

        auto const temperament = ReadDword(ValueTemperament, 0);
        m_temperament = temperament < static_cast<uint32_t>(midicalc::TemperamentCount)
            ? static_cast<midicalc::Temperament>(temperament)
            : midicalc::Temperament::Equal;

        m_temperamentRoot = static_cast<int32_t>(ReadDword(ValueTemperamentRoot, 0) % 12);

        auto const middleC = ReadDword(ValueMiddleCOctave, midicalc::DefaultMiddleCOctave);
        m_middleCOctave = (middleC >= 1 && middleC <= 7) ? static_cast<uint8_t>(middleC) : midicalc::DefaultMiddleCOctave;

        auto const ticks = ReadDword(ValueTicksPerQuarterNote, DefaultTicksPerQuarterNote);
        m_ticksPerQuarterNote = (ticks >= 1 && ticks <= 65535) ? ticks : DefaultTicksPerQuarterNote;

        auto const page = ReadDword(ValueLastPage, 0);
        m_lastPage = page <= static_cast<uint32_t>(CalculatorPage::History)
            ? static_cast<CalculatorPage>(page)
            : CalculatorPage::Tempo;
    }

    _Use_decl_annotations_
    void AppSettings::Tempo(double value) noexcept
    {
        m_tempo = std::clamp(value, midicalc::MinimumTempo, midicalc::MaximumTempo);
        WriteDouble(ValueTempo, m_tempo);
    }

    _Use_decl_annotations_
    void AppSettings::Meter(midicalc::Meter value) noexcept
    {
        if (!midicalc::IsValidMeter(value))
        {
            return;
        }

        m_meter = value;
        WriteDword(ValueMeterNumerator, static_cast<uint32_t>(value.Numerator));
        WriteDword(ValueMeterDenominator, static_cast<uint32_t>(value.Denominator));
    }

    _Use_decl_annotations_
    void AppSettings::SampleRate(double value) noexcept
    {
        m_sampleRate = std::clamp(value, MinimumSampleRate, MaximumSampleRate);
        WriteDouble(ValueSampleRate, m_sampleRate);
    }

    _Use_decl_annotations_
    void AppSettings::ReferenceHertz(double value) noexcept
    {
        m_referenceHertz = std::clamp(value, MinimumReferenceHertz, MaximumReferenceHertz);
        WriteDouble(ValueReferenceHertz, m_referenceHertz);
    }

    void AppSettings::ResetSession() noexcept
    {
        Tempo(DefaultTempo);
        Meter(midicalc::Meter{});
        SampleRate(DefaultSampleRate);
        ReferenceHertz(midicalc::DefaultReferenceHertz);
    }

    _Use_decl_annotations_
    void AppSettings::Temperament(midicalc::Temperament value) noexcept
    {
        m_temperament = value;
        WriteDword(ValueTemperament, static_cast<uint32_t>(value));
    }

    _Use_decl_annotations_
    void AppSettings::TemperamentRoot(int32_t value) noexcept
    {
        m_temperamentRoot = ((value % 12) + 12) % 12;
        WriteDword(ValueTemperamentRoot, static_cast<uint32_t>(m_temperamentRoot));
    }

    _Use_decl_annotations_
    void AppSettings::MiddleCOctave(uint8_t value) noexcept
    {
        m_middleCOctave = (value >= 1 && value <= 7) ? value : midicalc::DefaultMiddleCOctave;
        WriteDword(ValueMiddleCOctave, m_middleCOctave);
    }

    _Use_decl_annotations_
    void AppSettings::TicksPerQuarterNote(uint32_t value) noexcept
    {
        m_ticksPerQuarterNote = std::clamp(value, 1u, 65535u);
        WriteDword(ValueTicksPerQuarterNote, m_ticksPerQuarterNote);
    }

    _Use_decl_annotations_
    void AppSettings::LastPage(CalculatorPage value) noexcept
    {
        m_lastPage = value;
        WriteDword(ValueLastPage, static_cast<uint32_t>(value));
    }
}
