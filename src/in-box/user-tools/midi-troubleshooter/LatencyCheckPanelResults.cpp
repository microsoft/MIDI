// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The results half of the DPC latency check page: the plain-words answer, the drivers that held
// audio up, and the technical report. See LatencyCheckPanel.xaml.h.

#include "pch.h"
#include "LatencyCheckPanel.xaml.h"

#include "LatencyCheckText.h"
#include "LatencyReportWriter.h"
#include "StringResources.h"

namespace latency = ::miditroubleshooter::latency;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        controls::InfoBarSeverity SeverityFor(_In_ latency::Verdict const verdict) noexcept
        {
            switch (verdict)
            {
            case latency::Verdict::Good: return controls::InfoBarSeverity::Success;
            case latency::Verdict::Caution: return controls::InfoBarSeverity::Warning;
            case latency::Verdict::Problem: return controls::InfoBarSeverity::Error;
            case latency::Verdict::NotEnoughData:
            default: return controls::InfoBarSeverity::Informational;
            }
        }

        // a line that has nothing to say takes no room
        void SetLine(_In_ controls::TextBlock const& block, _In_ std::wstring const& text)
        {
            block.Text(text);
            block.Visibility(text.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }

        std::wstring ProcessorName()
        {
            wchar_t buffer[256]{};
            DWORD size{ sizeof(buffer) };

            if (::RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString",
                RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
            {
                return {};
            }

            std::wstring name{ buffer };

            // some processors pad the name with spaces
            while (!name.empty() && name.back() == L' ')
            {
                name.pop_back();
            }

            return name;
        }

        std::wstring CurrentTime()
        {
            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            return std::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        }
    }

    void LatencyCheckPanel::ClearResults() noexcept
    {
        try
        {
            m_report.clear();

            ResultsPanel().Visibility(xaml::Visibility::Collapsed);
            ShowTraceButton().Visibility(xaml::Visibility::Collapsed);

            CulpritsPanel().Children().Clear();
            AdvicePanel().Children().Clear();
            RanLongPanel().Children().Clear();
            NotesPanel().Children().Clear();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to clear the DPC latency check results.")
    }

    _Use_decl_annotations_
    void LatencyCheckPanel::ShowOutcome(std::shared_ptr<latency::CheckOutcome const> const& outcome) noexcept
    {
        try
        {
            ClearResults();

            auto const& results = outcome->Data;
            auto const& assessment = outcome->Summary;

            auto const style = [this](wchar_t const* const key)
                {
                    return Resources().Lookup(winrt::box_value(key)).as<xaml::Style>();
                };

            VerdictBar().Severity(SeverityFor(assessment.Overall));
            VerdictBar().Title(latency::text::VerdictTitle(assessment));
            VerdictBar().Message(latency::text::VerdictMessage(assessment));

            SetLine(HeldUpText(), latency::text::HeldUpLine(assessment));
            SetLine(MidiHeldUpText(), latency::text::MidiHeldUpLine(assessment));
            SetLine(BufferText(), latency::text::BufferLine(assessment));

            for (auto const& culprit : assessment.Culprits)
            {
                auto const module = results.Drivers[culprit.Driver].Module;

                controls::StackPanel content{};
                content.Spacing(4);

                controls::TextBlock title{};
                title.Style(style(L"LatencyCardTitleStyle"));
                title.Text(latency::text::DriverTitle(results, module));
                content.Children().Append(title);

                if (auto const details = latency::text::DriverDetails(results, module); !details.empty())
                {
                    controls::TextBlock detailsText{};
                    detailsText.Style(style(L"LatencySecondaryTextStyle"));
                    detailsText.Text(details);
                    content.Children().Append(detailsText);
                }

                controls::TextBlock impact{};
                impact.Style(style(L"LatencyBodyTextStyle"));
                impact.Text(latency::text::CulpritImpact(culprit));
                content.Children().Append(impact);

                if (auto const advice = latency::text::CulpritAdvice(results, culprit); !advice.empty())
                {
                    controls::TextBlock adviceText{};
                    adviceText.Style(style(L"LatencyBodyTextStyle"));
                    adviceText.Text(advice);
                    content.Children().Append(adviceText);
                }

                controls::Border card{};
                card.Style(style(L"LatencyCardStyle"));
                card.Child(content);

                CulpritsPanel().Children().Append(card);
            }

            CulpritsHeading().Visibility(assessment.Culprits.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            auto const advice = latency::text::ExtraAdvice(assessment);

            for (auto const& line : advice)
            {
                controls::TextBlock text{};
                text.Style(style(L"LatencyBodyTextStyle"));
                text.Text(line);

                AdvicePanel().Children().Append(text);
            }

            AdviceHeading().Visibility(advice.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            // drivers that ran long but held nothing up, for reference
            for (auto const& culprit : assessment.RanLong)
            {
                controls::TextBlock text{};
                text.Style(style(L"LatencyBodyTextStyle"));
                text.Text(latency::text::RanLongLine(results, culprit));

                RanLongPanel().Children().Append(text);
            }

            auto const ranLongVisibility = assessment.RanLong.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
            RanLongHeading().Visibility(ranLongVisibility);
            RanLongCaption().Visibility(ranLongVisibility);

            for (auto const& line : latency::text::Notes(*outcome))
            {
                controls::TextBlock text{};
                text.Style(style(L"LatencySecondaryTextStyle"));
                text.Text(line);

                NotesPanel().Children().Append(text);
            }

            m_report = BuildReport(*outcome);

            ShowTraceButton().Visibility(results.Trace.Live && !m_savedTracePath.empty() ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            ResultsPanel().Visibility(xaml::Visibility::Visible);

            // the report buttons follow whether there's a report now
            SetState(m_state);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the DPC latency check results.")
    }

    _Use_decl_annotations_
    std::wstring LatencyCheckPanel::BuildReport(latency::CheckOutcome const& outcome) const
    {
        latency::ReportContext context{};
        context.ToolVersion = m_host.System.ToolVersion;
        context.CurrentTime = CurrentTime();
        context.RunningElevated = m_host.Elevated;

        // only a check run here is about this PC
        if (outcome.Data.Trace.Live)
        {
            context.OsVersion = m_host.System.WindowsVersion;
            context.Architecture = m_host.System.WindowsArchitecture;
            context.ProcessorName = ProcessorName();
            context.SavedTracePath = outcome.Data.Trace.SourcePath;
        }

        context.Session = outcome.Session;
        context.Findings = latency::text::Findings(outcome);

        return latency::WriteReport(outcome.Data, outcome.Summary, context);
    }
}
