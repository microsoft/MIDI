// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LatencyCheckText.h"
#include "LatencyDriverNames.h"
#include "StringResources.h"

namespace res = ::miditroubleshooter::resources;

namespace miditroubleshooter::latency::text
{
    namespace
    {
        // The customer's number format, built once. GetNumberFormatEx needs every part of it to
        // choose its own number of decimals.
        struct NumberFormat
        {
            wchar_t Decimal[8]{ L"." };
            wchar_t Thousand[8]{ L"," };
            NUMBERFMTW Format{};

            NumberFormat() noexcept
            {
                GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, Decimal, static_cast<int>(std::size(Decimal)));
                GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, Thousand, static_cast<int>(std::size(Thousand)));

                DWORD leadingZero{ 1 };
                DWORD negativeOrder{ 1 };

                GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_ILZERO | LOCALE_RETURN_NUMBER,
                    reinterpret_cast<LPWSTR>(&leadingZero), sizeof(leadingZero) / sizeof(wchar_t));
                GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_INEGNUMBER | LOCALE_RETURN_NUMBER,
                    reinterpret_cast<LPWSTR>(&negativeOrder), sizeof(negativeOrder) / sizeof(wchar_t));

                // "3;0" is groups of three, "3;2;0" is three and then twos. NUMBERFMTW writes
                // those as 3 and 32, and a list that doesn't end in 0 doesn't repeat.
                wchar_t grouping[16]{};
                UINT groupingValue{ 3 };

                if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SGROUPING, grouping, static_cast<int>(std::size(grouping))) > 0)
                {
                    std::wstring digits{};

                    for (auto const character : std::wstring_view{ grouping })
                    {
                        if (character >= L'0' && character <= L'9')
                        {
                            digits.push_back(character);
                        }
                    }

                    if (!digits.empty() && digits.back() == L'0')
                    {
                        digits.pop_back();
                    }
                    else
                    {
                        digits.push_back(L'0');
                    }

                    groupingValue = digits.empty() ? 0 : static_cast<UINT>(std::wcstoul(digits.c_str(), nullptr, 10));
                }

                Format.LeadingZero = leadingZero;
                Format.Grouping = groupingValue;
                Format.lpDecimalSep = Decimal;
                Format.lpThousandSep = Thousand;
                Format.NegativeOrder = negativeOrder;
            }
        };

        std::wstring FormatNumber(_In_ std::wstring const& plain, _In_ UINT const decimals)
        {
            static NumberFormat const format{};

            auto numberFormat = format.Format;
            numberFormat.NumDigits = decimals;

            wchar_t buffer[64]{};

            if (GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, plain.c_str(), &numberFormat, buffer, static_cast<int>(std::size(buffer))) > 0)
            {
                return buffer;
            }

            return plain;
        }

        std::wstring KindKey(_In_ std::wstring_view const prefix, _In_ DriverKind const kind)
        {
            return std::wstring{ prefix } + std::wstring{ names::KindName(kind) };
        }

        // A resource that can be missing on purpose, such as a kind of driver with no tip.
        std::wstring OptionalString(_In_ std::wstring const& key)
        {
            auto value = std::wstring{ res::GetString(key) };

            // a missing resource comes back as its key
            return value == key ? std::wstring{} : value;
        }

        // kinds of driver where getting a newer one is worth suggesting
        bool WorthUpdating(_In_ DriverKind const kind) noexcept
        {
            switch (kind)
            {
            case DriverKind::WindowsKernel:
            case DriverKind::NetworkStack:
            case DriverKind::Hypervisor:
            case DriverKind::Security:
                return false;

            default:
                return true;
            }
        }

        std::wstring Join(_In_ std::wstring const& first, _In_ std::wstring const& second)
        {
            if (first.empty())
            {
                return second;
            }

            if (second.empty())
            {
                return first;
            }

            return first + L" " + second;
        }

        // Audio threads, or the test thread standing in for them.
        bool HeardFromAudio(_In_ Assessment const& assessment) noexcept
        {
            return (assessment.HadAudioApp || assessment.HadProbe) && !assessment.MissingScheduling;
        }
    }

    _Use_decl_annotations_
    std::wstring Milliseconds(uint64_t const microseconds)
    {
        // two decimals, so 0.85 ms and 3.48 ms both say something
        auto const hundredths = (microseconds + 5) / 10;
        auto const plain = std::format(L"{}.{:02}", hundredths / 100, hundredths % 100);

        return std::wstring{ res::FormatString(L"LatencyMillisecondsFormat", FormatNumber(plain, 2)) };
    }

    _Use_decl_annotations_
    std::wstring Count(uint64_t const value)
    {
        return FormatNumber(std::to_wstring(value), 0);
    }

    _Use_decl_annotations_
    std::wstring DriverTitle(Results const& results, int32_t const module)
    {
        auto const* const info = results.Module(module);

        if (info == nullptr)
        {
            return std::wstring{ res::GetString(L"LatencyUnknownDriver") };
        }

        if (!info->Description.empty())
        {
            return std::wstring{ res::FormatString(L"LatencyDriverTitleFormat", info->Description, info->FileName) };
        }

        auto const kindName = OptionalString(KindKey(L"LatencyKind_", info->Kind));

        return kindName.empty() ?
            info->FileName :
            std::wstring{ res::FormatString(L"LatencyDriverTitleFormat", kindName, info->FileName) };
    }

    _Use_decl_annotations_
    std::wstring DriverDetails(Results const& results, int32_t const module)
    {
        auto const* const info = results.Module(module);

        if (info == nullptr)
        {
            return {};
        }

        std::wstring details{};

        if (!info->Company.empty() && !info->Version.empty())
        {
            details = res::FormatString(L"LatencyDriverMadeByVersionFormat", info->Company, info->Version);
        }
        else if (!info->Company.empty())
        {
            details = res::FormatString(L"LatencyDriverMadeByFormat", info->Company);
        }

        if (!info->Devices.empty())
        {
            auto const usedBy = info->Devices.size() == 1 ?
                std::wstring{ res::FormatString(L"LatencyDriverUsedByFormat", info->Devices.front()) } :
                std::wstring{ res::FormatString(L"LatencyDriverUsedByMoreFormat", info->Devices.front(), Count(info->Devices.size() - 1)) };

            details = Join(details, usedBy);
        }

        return details;
    }

    _Use_decl_annotations_
    std::wstring CulpritImpact(Culprit const& culprit)
    {
        auto const key = culprit.LongestKind == CauseKind::Isr ? L"LatencyImpactIsrFormat" :
            culprit.LongestKind == CauseKind::RaisedIrql ? L"LatencyImpactHeldFormat" : L"LatencyImpactDpcFormat";

        std::wstring impact{ res::FormatString(key, Milliseconds(culprit.LongestMicroseconds)) };

        if (culprit.LateMoments == 1)
        {
            impact = Join(impact, std::wstring{ res::GetString(L"LatencyImpactLateOnce") });
        }
        else if (culprit.LateMoments > 1)
        {
            impact = Join(impact, std::wstring{ res::FormatString(L"LatencyImpactLateManyFormat", Count(culprit.LateMoments)) });
        }

        return impact;
    }

    _Use_decl_annotations_
    std::wstring CulpritAdvice(Results const& results, Culprit const& culprit)
    {
        auto const* const info = results.Module(results.Drivers[culprit.Driver].Module);
        auto const kind = info != nullptr ? info->Kind : DriverKind::Unknown;

        std::wstring update{};

        if (WorthUpdating(kind))
        {
            if (info != nullptr && info->IsMicrosoft)
            {
                update = res::GetString(L"LatencyUpdateWindows");
            }
            else if (info != nullptr && !info->Company.empty())
            {
                update = res::FormatString(L"LatencyUpdateCompanyFormat", info->Company);
            }
            else
            {
                update = res::GetString(L"LatencyUpdateUnknownCompany");
            }
        }

        return Join(update, OptionalString(KindKey(L"LatencyTip_", kind)));
    }

    _Use_decl_annotations_
    std::wstring RanLongLine(Results const& results, Culprit const& culprit)
    {
        return std::wstring{ res::FormatString(L"LatencyRanLongLineFormat",
            DriverTitle(results, results.Drivers[culprit.Driver].Module),
            Milliseconds(culprit.LongestMicroseconds)) };
    }

    _Use_decl_annotations_
    std::wstring VerdictTitle(Assessment const& assessment)
    {
        switch (assessment.Overall)
        {
        case Verdict::Good: return std::wstring{ res::GetString(L"LatencyVerdictGoodTitle") };
        case Verdict::Caution: return std::wstring{ res::GetString(L"LatencyVerdictCautionTitle") };
        case Verdict::Problem: return std::wstring{ res::GetString(L"LatencyVerdictProblemTitle") };
        case Verdict::NotEnoughData:
        default: return std::wstring{ res::GetString(L"LatencyVerdictNotEnoughDataTitle") };
        }
    }

    _Use_decl_annotations_
    std::wstring VerdictMessage(Assessment const& assessment)
    {
        // only what was actually held up, never a DPC on its own
        auto const worst = Milliseconds((std::max)(assessment.LongestHoldUpMicroseconds, assessment.MidiLongestHoldUpMicroseconds));

        switch (assessment.Overall)
        {
        case Verdict::Good: return std::wstring{ res::FormatString(L"LatencyVerdictGoodFormat", worst) };
        case Verdict::Caution: return std::wstring{ res::FormatString(L"LatencyVerdictCautionFormat", worst) };
        case Verdict::Problem: return std::wstring{ res::FormatString(L"LatencyVerdictProblemFormat", worst) };
        case Verdict::NotEnoughData:
        default:
            if (assessment.AnalysisIncomplete)
            {
                return std::wstring{ res::GetString(L"LatencyVerdictIncomplete") };
            }

            if (assessment.MissingDpcs)
            {
                return std::wstring{ res::GetString(L"LatencyVerdictNotEnoughData") };
            }

            return std::wstring{ res::GetString(assessment.MissingScheduling ? L"LatencyVerdictNoScheduling" : L"LatencyVerdictNothingToWatch") };
        }
    }

    _Use_decl_annotations_
    std::wstring HeldUpLine(Assessment const& assessment)
    {
        if (!HeardFromAudio(assessment))
        {
            return {};
        }

        auto const threshold = Milliseconds(LateMomentMicroseconds);
        auto const longest = Milliseconds(assessment.LongestHoldUpMicroseconds);

        // with no audio app, the test thread is what was held up
        auto const prefix = assessment.HadAudioApp ? std::wstring_view{ L"LatencyHeldUp" } : std::wstring_view{ L"LatencyTestHeldUp" };

        if (assessment.LateMoments == 0)
        {
            return std::wstring{ res::FormatString(std::wstring{ prefix } + L"NoneFormat", threshold, longest) };
        }

        if (assessment.LateMoments == 1)
        {
            return std::wstring{ res::FormatString(std::wstring{ prefix } + L"OnceFormat", threshold, longest) };
        }

        return std::wstring{ res::FormatString(std::wstring{ prefix } + L"ManyFormat", threshold, Count(assessment.LateMoments), longest) };
    }

    _Use_decl_annotations_
    std::wstring MidiHeldUpLine(Assessment const& assessment)
    {
        if (!assessment.HadMidiService || assessment.MissingScheduling)
        {
            return {};
        }

        auto const threshold = Milliseconds(LateMomentMicroseconds);
        auto const longest = Milliseconds(assessment.MidiLongestHoldUpMicroseconds);

        if (assessment.MidiLateMoments == 0)
        {
            return std::wstring{ res::FormatString(L"LatencyMidiHeldUpNoneFormat", threshold, longest) };
        }

        if (assessment.MidiLateMoments == 1)
        {
            return std::wstring{ res::FormatString(L"LatencyMidiHeldUpOnceFormat", threshold, longest) };
        }

        return std::wstring{ res::FormatString(L"LatencyMidiHeldUpManyFormat", threshold, Count(assessment.MidiLateMoments), longest) };
    }

    _Use_decl_annotations_
    std::wstring BufferLine(Assessment const& assessment)
    {
        if (assessment.RiskyBufferMicroseconds < CautionMicroseconds)
        {
            return {};
        }

        return std::wstring{ res::FormatString(L"LatencyBufferFormat",
            Milliseconds(assessment.RiskyBufferMicroseconds),
            Count(SamplesAt48Kilohertz(assessment.RiskyBufferMicroseconds))) };
    }

    _Use_decl_annotations_
    std::vector<std::wstring> ExtraAdvice(Assessment const& assessment)
    {
        std::vector<std::wstring> advice{};

        if (assessment.AdvisePowerMode)
        {
            advice.emplace_back(res::GetString(L"LatencyAdvicePowerMode"));
        }

        if (assessment.AdviseBusyPrograms)
        {
            advice.emplace_back(res::GetString(L"LatencyAdviceBusyPrograms"));
        }

        if (assessment.AdviseDiskWaits)
        {
            advice.emplace_back(res::GetString(L"LatencyAdviceDiskWaits"));
        }

        if (assessment.UnseenStalls > 0)
        {
            advice.emplace_back(res::FormatString(L"LatencyAdviceUnseenStallFormat", Milliseconds(assessment.LongestUnseenStallMicroseconds)));
        }

        return advice;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> Notes(CheckOutcome const& outcome)
    {
        std::vector<std::wstring> notes{};

        auto const& results = outcome.Data;
        auto const& assessment = outcome.Summary;

        if (!results.Trace.Live)
        {
            notes.emplace_back(res::FormatString(L"LatencyNoteFromFileFormat", results.Trace.SourcePath));

            auto const matched = std::any_of(results.Modules.begin(), results.Modules.end(),
                [](ModuleInfo const& module) { return module.LocalFileMatches; });

            if (!matched)
            {
                notes.emplace_back(res::GetString(L"LatencyNoteOtherPc"));
            }
        }
        else if (!results.Trace.SourcePath.empty())
        {
            notes.emplace_back(res::FormatString(L"LatencyNoteTraceSavedFormat", results.Trace.SourcePath));
        }

        if (results.Trace.Live && results.Probe.Error != 0)
        {
            notes.emplace_back(res::GetString(L"LatencyNoteProbeFailed"));
        }
        else if (results.Probe.Ran && !results.Probe.UsedMmcss)
        {
            notes.emplace_back(res::GetString(L"LatencyNoteProbeWithoutMmcss"));
        }

        // with nothing to watch, or no scheduling events, the answer itself already says so
        if (assessment.Overall != Verdict::NotEnoughData && !assessment.HadAudioApp)
        {
            notes.emplace_back(res::GetString(assessment.HadProbe ? L"LatencyNoteNoAudioApp" : L"LatencyNoteNoAudioThreads"));
        }

        if (assessment.MissingSamples)
        {
            notes.emplace_back(res::GetString(L"LatencyNoteNoSamples"));
        }

        if (outcome.Session && !outcome.Session->FrameworkEventsEnabled)
        {
            notes.emplace_back(res::GetString(L"LatencyNoteNoFrameworkEvents"));
        }

        if (assessment.LostEvents)
        {
            notes.emplace_back(res::GetString(L"LatencyNoteEventsLost"));
        }

        return notes;
    }

    _Use_decl_annotations_
    std::vector<ReportFinding> Findings(CheckOutcome const& outcome)
    {
        std::vector<ReportFinding> findings{};

        auto const& results = outcome.Data;
        auto const& assessment = outcome.Summary;

        auto const add = [&findings](std::wstring_view const id, std::wstring const& text)
            {
                if (!text.empty())
                {
                    findings.push_back(ReportFinding{ std::wstring{ id }, text });
                }
            };

        // The report viewer shows every finding as something to look into, so good news stays
        // out of this list. The page and the report's summary still have it.
        if (assessment.Overall != Verdict::Good)
        {
            add(L"verdict", VerdictTitle(assessment) + L". " + VerdictMessage(assessment));
        }

        if (assessment.LateMoments > 0)
        {
            add(L"late_audio", HeldUpLine(assessment));
        }

        if (assessment.MidiLateMoments > 0)
        {
            add(L"late_midi", MidiHeldUpLine(assessment));
        }

        add(L"risky_buffer", BufferLine(assessment));

        for (auto const& culprit : assessment.Culprits)
        {
            auto const module = results.Drivers[culprit.Driver].Module;

            add(L"long_driver", std::wstring{ res::FormatString(L"LatencyFindingDriverFormat",
                DriverTitle(results, module), CulpritImpact(culprit), CulpritAdvice(results, culprit)) });
        }

        if (assessment.AdvisePowerMode)
        {
            add(L"power_mode", std::wstring{ res::GetString(L"LatencyAdvicePowerMode") });
        }

        if (assessment.AdviseBusyPrograms)
        {
            add(L"busy_programs", std::wstring{ res::GetString(L"LatencyAdviceBusyPrograms") });
        }

        if (assessment.AdviseDiskWaits)
        {
            add(L"disk_waits", std::wstring{ res::GetString(L"LatencyAdviceDiskWaits") });
        }

        if (assessment.UnseenStalls > 0)
        {
            add(L"unseen_stall", std::wstring{ res::FormatString(L"LatencyAdviceUnseenStallFormat", Milliseconds(assessment.LongestUnseenStallMicroseconds)) });
        }

        if (assessment.Overall != Verdict::NotEnoughData && !assessment.HadAudioApp)
        {
            add(assessment.HadProbe ? L"no_audio_app" : L"no_audio_threads",
                std::wstring{ res::GetString(assessment.HadProbe ? L"LatencyNoteNoAudioApp" : L"LatencyNoteNoAudioThreads") });
        }

        if (assessment.MissingScheduling)
        {
            add(L"no_scheduling_events", std::wstring{ res::GetString(L"LatencyNoteNoScheduling") });
        }

        if (assessment.MissingSamples)
        {
            add(L"no_samples", std::wstring{ res::GetString(L"LatencyNoteNoSamples") });
        }

        if (assessment.LostEvents)
        {
            add(L"events_lost", std::wstring{ res::GetString(L"LatencyNoteEventsLost") });
        }

        return findings;
    }

    _Use_decl_annotations_
    bool WriteUtf8File(std::wstring const& path, std::wstring const& text) noexcept
    {
        try
        {
            auto const required = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            std::string utf8{};

            if (required > 0)
            {
                utf8.resize(static_cast<size_t>(required));
                ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), required, nullptr, nullptr);
            }

            std::ofstream stream{ path, std::ios::binary | std::ios::trunc };

            if (!stream.is_open())
            {
                return false;
            }

            constexpr char byteOrderMark[]{ '\xEF', '\xBB', '\xBF' };

            stream.write(byteOrderMark, sizeof(byteOrderMark));
            stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));

            return stream.good();
        }
        catch (...)
        {
            return false;
        }
    }
}
