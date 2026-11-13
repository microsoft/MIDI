// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The session values along the top: tempo, meter, sample rate and tuning reference. Every page
// that uses one recalculates when it changes.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcHistory.h"
#include "CalcPitch.h"
#include "CalcText.h"
#include "StringResources.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;

namespace winrt::midicalculator::implementation
{
    void MainWindow::LoadSessionControls() noexcept
    {
        try
        {
            auto const& settings = native::AppSettings::Current();

            // the sample rate with the locale's grouping, as in the comps (48,000)
            winrt::Windows::Globalization::NumberFormatting::DecimalFormatter grouped{};
            grouped.IsGrouped(true);
            grouped.FractionDigits(0);
            SessionSampleRateBox().NumberFormatter(grouped);

            m_suppressSession = true;
            SessionTempoBox().Value(settings.Tempo());
            SessionSampleRateBox().Value(settings.SampleRate());
            SessionReferenceBox().Value(settings.ReferenceHertz());
            SessionMeterBox().Text(std::format(L"{}/{}", settings.Meter().Numerator, settings.Meter().Denominator));

            if (SessionFrameRateBox().Items().Size() == 0)
            {
                for (int32_t rate = 0; rate < ::midicalc::FrameRateCount; ++rate)
                {
                    SessionFrameRateBox().Items().Append(winrt::box_value(res::GetString(std::format(L"FrameRateShort{}", rate))));
                }
            }

            SessionFrameRateBox().SelectedIndex(static_cast<int32_t>(settings.FrameRate()));
            m_suppressSession = false;

            UpdateReferenceLabel();
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to show the session values.")
    }

    void MainWindow::UpdateReferenceLabel() noexcept
    {
        try
        {
            // "A3 =", or "A4 =" for someone who calls middle C C4
            auto const name = ::midicalc::NoteName(::midicalc::ReferenceNote, native::AppSettings::Current().MiddleCOctave(), false);
            SessionReferenceLabel().Text(res::FormatString(L"SessionReferenceFormat", name));
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(SessionReferenceBox(), res::FormatString(L"SessionReferenceName", name));
        }
        CATCH_LOG()
    }

    void MainWindow::SessionChanged() noexcept
    {
        if (!m_initialized)
        {
            return;
        }

        UpdateReferenceLabel();
        UpdateTempoPage();
        UpdatePitchPage();
        UpdateTimecodePage();
        UpdateMidi2Page();
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionFrameRateChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        auto const index = SessionFrameRateBox().SelectedIndex();

        if (m_suppressSession || index < 0 || index >= ::midicalc::FrameRateCount)
        {
            return;
        }

        auto const rate = static_cast<::midicalc::FrameRate>(index);
        native::AppSettings::Current().FrameRate(rate);

        // the cards follow the session frame rate; each can still be set on its own after
        if (m_tcRate)
        {
            m_tcRate.SelectedIndex(index);
            m_cvRate.SelectedIndex(index);
        }

        SessionChanged();
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionTempoChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const&)
    {
        if (m_suppressSession || std::isnan(sender.Value()))
        {
            return;
        }

        native::AppSettings::Current().Tempo(sender.Value());
        SessionChanged();
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionSampleRateChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const&)
    {
        if (m_suppressSession || std::isnan(sender.Value()))
        {
            return;
        }

        native::AppSettings::Current().SampleRate(sender.Value());
        SessionChanged();
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionReferenceChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const&)
    {
        if (m_suppressSession || std::isnan(sender.Value()))
        {
            return;
        }

        native::AppSettings::Current().ReferenceHertz(sender.Value());
        SessionChanged();
    }

    void MainWindow::ApplySessionMeter() noexcept
    {
        try
        {
            auto const parsed = ::midicalculator::text::ParseMeter(SessionMeterBox().Text());

            if (parsed.has_value() && ::midicalc::IsValidMeter(::midicalc::Meter{ parsed->first, parsed->second }))
            {
                native::AppSettings::Current().Meter(::midicalc::Meter{ parsed->first, parsed->second });
                SessionChanged();
            }

            // whatever was typed, the box shows the meter in use
            auto const meter = native::AppSettings::Current().Meter();
            SessionMeterBox().Text(std::format(L"{}/{}", meter.Numerator, meter.Denominator));
        }
        CATCH_LOG()
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionMeterLostFocus(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ApplySessionMeter();
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionMeterKeyDown(foundation::IInspectable const&, xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            ApplySessionMeter();
            args.Handled(true);
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnSessionResetClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            native::AppSettings::Current().ResetSession();
            LoadSessionControls();

            if (m_tcRate)
            {
                auto const rate = static_cast<int32_t>(native::AppSettings::Current().FrameRate());
                m_tcRate.SelectedIndex(rate);
                m_cvRate.SelectedIndex(rate);
            }

            SessionChanged();
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to reset the session values.")
    }

    _Use_decl_annotations_
    void MainWindow::UseAsSessionTempo(double tempo, winrt::hstring const& page, winrt::hstring const& question) noexcept
    {
        try
        {
            SessionTempoBox().Value(tempo);

            auto const answer = res::FormatString(L"TempoValueFormat", ::midicalculator::text::Number(tempo, 3));
            native::History::Current().Add(std::wstring{ page }, std::wstring{ question }, std::wstring{ answer });
            ShowStatus(res::FormatString(L"StatusSessionTempo", std::wstring_view{ answer }));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to set the session tempo.")
    }
}
