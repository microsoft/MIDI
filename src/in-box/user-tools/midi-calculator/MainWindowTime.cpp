// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Windows timestamps: the live MIDI clock, a timestamp in real units and clock time, the gap
// between two timestamps, and the other tick counts Windows uses.
//
// Timestamps come from the SDK's MidiClock, which is what the service stamps messages with. If
// the SDK isn't there, the same counter is read directly so the page still works.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "CalcText.h"
#include "CalcTime.h"
#include "StringResources.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;
namespace ui = ::midicalculator::ui;
namespace text = ::midicalculator::text;

namespace winrt::midicalculator::implementation
{
    namespace
    {
        enum class OtherKind : int32_t
        {
            LocalTime = 0,
            UtcTime = 1,
            FileTime = 2,
            DotNetTicks = 3,
            UnixSeconds = 4,
            UnixMilliseconds = 5,
            MidiTimestamp = 6
        };

        uint64_t TimestampNow() noexcept
        {
            try
            {
                return midi2::MidiClock::Now();
            }
            catch (...)
            {
                LARGE_INTEGER counter{};
                ::QueryPerformanceCounter(&counter);
                return static_cast<uint64_t>(counter.QuadPart);
            }
        }

        uint64_t TimestampFrequency() noexcept
        {
            try
            {
                return midi2::MidiClock::TimestampFrequency();
            }
            catch (...)
            {
                LARGE_INTEGER frequency{};
                ::QueryPerformanceFrequency(&frequency);
                return static_cast<uint64_t>(frequency.QuadPart);
            }
        }

        int64_t FileTimeNow() noexcept
        {
            FILETIME now{};
            ::GetSystemTimePreciseAsFileTime(&now);
            return static_cast<int64_t>((static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);
        }

        ::midicalc::ClockPair ClockNow() noexcept
        {
            // read together, so the pair describes one moment
            ::midicalc::ClockPair pair{};
            pair.Timestamp = TimestampNow();
            pair.FileTime = FileTimeNow();
            pair.TicksPerSecond = TimestampFrequency();
            return pair;
        }

        FILETIME ToFileTime(int64_t value) noexcept
        {
            FILETIME result{};
            result.dwLowDateTime = static_cast<DWORD>(static_cast<uint64_t>(value) & 0xFFFFFFFF);
            result.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(value) >> 32);
            return result;
        }

        // The date and time in the customer's own format and time zone, with milliseconds.
        std::wstring LocalDateTime(int64_t fileTime)
        {
            if (fileTime < 0)
            {
                return L"\u2014";
            }

            auto const utc = ToFileTime(fileTime);
            SYSTEMTIME utcTime{};
            SYSTEMTIME local{};

            if (!::FileTimeToSystemTime(&utc, &utcTime) || !::SystemTimeToTzSpecificLocalTimeEx(nullptr, &utcTime, &local))
            {
                return L"\u2014";
            }

            wchar_t date[96]{};
            wchar_t time[64]{};
            ::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_LONGDATE, &local, nullptr, date, ARRAYSIZE(date), nullptr);
            ::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, nullptr, time, ARRAYSIZE(time));

            // the locale's time has no milliseconds, so they go after the seconds
            std::wstring timeText{ time };
            auto const seconds = std::format(L"{:02}", local.wSecond);
            auto const at = timeText.rfind(seconds);

            if (at != std::wstring::npos)
            {
                timeText.insert(at + seconds.size(), std::format(L".{:03}", local.wMilliseconds));
            }

            return std::format(L"{}, {}", date, timeText);
        }

        std::wstring IsoUtc(int64_t fileTime)
        {
            auto const utc = ToFileTime(fileTime);
            SYSTEMTIME time{};

            if (fileTime < 0 || !::FileTimeToSystemTime(&utc, &time))
            {
                return L"\u2014";
            }

            return std::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
                time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
        }

        // A date and time typed as year-month-day ("2026-11-11 19:30:00.123", "2026-11-12T00:30:00Z")
        // or month/day/year ("11/11/2026 7:30:00.123 PM"). Gives a FILETIME, reading the time as
        // local or UTC.
        std::optional<int64_t> ParseDateTime(std::wstring_view input, bool isUtc) noexcept
        {
            try
            {
                std::vector<std::wstring> numbers{};
                std::wstring current{};
                bool pm{ false };
                bool am{ false };

                for (size_t i = 0; i <= input.size(); ++i)
                {
                    auto const c = i < input.size() ? input[i] : L' ';

                    if (c >= L'0' && c <= L'9')
                    {
                        current.push_back(c);
                        continue;
                    }

                    if (!current.empty())
                    {
                        numbers.push_back(current);
                        current.clear();
                    }

                    if ((c == L'P' || c == L'p') && i + 1 < input.size() && (input[i + 1] == L'M' || input[i + 1] == L'm')) pm = true;
                    if ((c == L'A' || c == L'a') && i + 1 < input.size() && (input[i + 1] == L'M' || input[i + 1] == L'm')) am = true;
                }

                if (numbers.size() < 3)
                {
                    return std::nullopt;
                }

                auto value = [&](size_t index) { return index < numbers.size() ? std::wcstoul(numbers[index].c_str(), nullptr, 10) : 0ul; };

                SYSTEMTIME time{};

                if (numbers[0].size() == 4)
                {
                    time.wYear = static_cast<WORD>(value(0));
                    time.wMonth = static_cast<WORD>(value(1));
                    time.wDay = static_cast<WORD>(value(2));
                }
                else
                {
                    time.wMonth = static_cast<WORD>(value(0));
                    time.wDay = static_cast<WORD>(value(1));
                    time.wYear = static_cast<WORD>(value(2));
                }

                time.wHour = static_cast<WORD>(value(3));
                time.wMinute = static_cast<WORD>(value(4));
                time.wSecond = static_cast<WORD>(value(5));

                if (numbers.size() > 6)
                {
                    // the fraction of a second, to milliseconds: "1" is 100 ms, "123456" is 123 ms
                    auto fraction = numbers[6];
                    fraction.resize(3, L'0');
                    time.wMilliseconds = static_cast<WORD>(std::wcstoul(fraction.c_str(), nullptr, 10));
                }

                if (pm && time.wHour < 12) time.wHour += 12;
                if (am && time.wHour == 12) time.wHour = 0;

                SYSTEMTIME utc{ time };

                if (!isUtc && !::TzSpecificLocalTimeToSystemTimeEx(nullptr, &time, &utc))
                {
                    return std::nullopt;
                }

                FILETIME result{};

                if (!::SystemTimeToFileTime(&utc, &result))
                {
                    return std::nullopt;
                }

                return static_cast<int64_t>((static_cast<uint64_t>(result.dwHighDateTime) << 32) | result.dwLowDateTime);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::wstring Since(double seconds)
        {
            auto const split = ::midicalc::SplitSeconds(seconds);
            return res::FormatString(L"TimeSinceFormat", split.Hours, split.Minutes, text::Number(split.Seconds, 3, true)).c_str();
        }
    }

    void MainWindow::BuildTimePage()
    {
        auto const page = res::GetString(L"PageWindowsTime");
        auto const copy = CopyFor(page);
        auto columns = std::make_shared<ui::CardColumns>();
        auto const frequency = TimestampFrequency();

        // ---- this PC, live: a strip across the top, as in the comp ----
        controls::Border strip{};
        {
            strip.Background(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"CardBackgroundFillColorDefaultBrush")).as<media::Brush>());
            strip.BorderBrush(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"CardStrokeColorDefaultBrush")).as<media::Brush>());
            strip.BorderThickness(xaml::Thickness{ 1 });
            strip.CornerRadius(xaml::CornerRadius{ 8 });
            strip.Padding(xaml::Thickness{ 16, 12, 16, 12 });

            controls::Grid row{};
            row.ColumnSpacing(28);

            auto stat = [&](winrt::hstring const& label, bool monospace)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::Auto());
                row.ColumnDefinitions().Append(column);

                controls::StackPanel panel{};
                panel.Spacing(2);
                panel.Children().Append(ui::MakeHelp(label));

                controls::TextBlock value{};
                value.FontSize(18);
                value.IsTextSelectionEnabled(true);

                if (monospace)
                {
                    value.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                }

                panel.Children().Append(value);
                controls::Grid::SetColumn(panel, static_cast<int32_t>(row.ColumnDefinitions().Size()) - 1);
                row.Children().Append(panel);
                return value;
            };

            m_liveNow = stat(res::GetString(L"TimeLiveNow"), true);
            m_liveFrequency = stat(res::GetString(L"TimeLiveFrequency"), false);
            m_liveSinceStart = stat(res::GetString(L"TimeLiveSinceStart"), false);
            m_liveTimerPeriod = stat(res::GetString(L"TimeLiveTimer"), false);

            controls::ColumnDefinition rest{};
            rest.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
            row.ColumnDefinitions().Append(rest);

            m_liveFreeze = controls::Primitives::ToggleButton{};
            controls::StackPanel freeze{};
            freeze.Orientation(controls::Orientation::Horizontal);
            freeze.Spacing(8);
            controls::FontIcon pause{};
            pause.Glyph(L"\uE769");
            pause.FontSize(13);
            controls::TextBlock freezeText{};
            freezeText.Text(res::GetString(L"TimeLiveFreeze"));
            freeze.Children().Append(pause);
            freeze.Children().Append(freezeText);
            m_liveFreeze.Content(freeze);
            m_liveFreeze.VerticalAlignment(xaml::VerticalAlignment::Center);
            m_liveFreeze.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_liveFreeze, res::GetString(L"TimeLiveFreeze"));
            controls::ToolTipService::SetToolTip(m_liveFreeze, winrt::box_value(res::GetString(L"TimeLiveHelp")));
            controls::Grid::SetColumn(m_liveFreeze, static_cast<int32_t>(row.ColumnDefinitions().Size()) - 1);
            row.Children().Append(m_liveFreeze);

            // a narrow window scrolls the strip rather than cutting it off
            controls::ScrollViewer scroller{};
            scroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
            scroller.HorizontalScrollMode(controls::ScrollMode::Auto);
            scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
            scroller.VerticalScrollMode(controls::ScrollMode::Disabled);
            scroller.Content(row);
            strip.Child(scroller);
        }

        // ---- a timestamp ----
        {
            auto card = ui::MakeCard(res::GetString(L"TimeStampTitle"), L"\uE916");

            m_timestamp = ui::MakeTextBox(res::GetString(L"TimeStampLabel"), winrt::hstring{ std::to_wstring(TimestampNow()) }, 220);
            m_ticksPerSecond = ui::MakeNumberBox(res::GetString(L"TimeTicksPerSecondLabel"), static_cast<double>(frequency), 1, 1e12, 160);
            card.Body.Children().Append(ui::MakeFieldRow({ m_timestamp, m_ticksPerSecond }));

            ui::ResultList results{};
            m_tsSeconds = results.Add(res::GetString(L"TimeSeconds"), copy);
            m_tsMilliseconds = results.Add(res::GetString(L"TimeMilliseconds"), copy);
            m_tsMicroseconds = results.Add(res::GetString(L"TimeMicroseconds"), copy);
            m_tsSinceStart = results.Add(res::GetString(L"TimeAfterStart"), copy);
            m_tsClock = results.Add(res::GetString(L"TimeClockTime"), copy);
            card.Body.Children().Append(results.Root());
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"TimeClockHelp")));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateTimestamp(); };
            m_timestamp.TextChanged(update);
            m_ticksPerSecond.ValueChanged(update);

            columns->Add(card, 0);
        }

        // ---- between two timestamps ----
        {
            auto card = ui::MakeCard(res::GetString(L"TimeBetweenTitle"), L"\uE8AB");
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"TimeBetweenHelp")));

            auto const now = TimestampNow();
            m_betweenFrom = ui::MakeTextBox(res::GetString(L"TimeFromLabel"), winrt::hstring{ std::to_wstring(now) }, 200);
            m_betweenTo = ui::MakeTextBox(res::GetString(L"TimeToLabel"), winrt::hstring{ std::to_wstring(now + frequency / 4) }, 200);
            card.Body.Children().Append(ui::MakeFieldRow({ m_betweenFrom, m_betweenTo }));

            m_betweenResult = ui::MakeBigResult(res::GetString(L"TimeBetweenName"), copy);
            card.Body.Children().Append(m_betweenResult.Root);

            m_betweenAdd = ui::MakeNumberBox(res::GetString(L"TimeAddLabel"), 250, -1e9, 1e9, 150);
            card.Body.Children().Append(m_betweenAdd);

            ui::ResultList added{};
            m_betweenAdded = added.Add(res::GetString(L"TimeAdded"), copy, true);
            card.Body.Children().Append(added.Root());

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateBetween(false); };
            m_betweenFrom.TextChanged(update);
            m_betweenTo.TextChanged(update);
            m_betweenAdd.ValueChanged(update);

            columns->Add(card, 0);
        }

        // ---- other Windows time values ----
        {
            auto card = ui::MakeCard(res::GetString(L"TimeOtherTitle"), L"\uE787");
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"TimeOtherHelp")));

            m_otherKind = ui::MakeComboBox(res::GetString(L"TimeOtherKindLabel"), {
                res::GetString(L"TimeKindLocal"), res::GetString(L"TimeKindUtc"), res::GetString(L"TimeKindFileTime"),
                res::GetString(L"TimeKindDotNet"), res::GetString(L"TimeKindUnixSeconds"), res::GetString(L"TimeKindUnixMilliseconds"),
                res::GetString(L"TimeKindMidi") }, 0, 220);

            SYSTEMTIME local{};
            ::GetLocalTime(&local);
            m_otherInput = ui::MakeTextBox(res::GetString(L"TimeOtherInputLabel"),
                winrt::hstring{ std::format(L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}", local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond, local.wMilliseconds) }, 240);
            card.Body.Children().Append(ui::MakeFieldRow({ m_otherKind, m_otherInput }));

            ui::ResultList results{};
            m_otherLocal = results.Add(res::GetString(L"TimeKindLocal"), copy);
            m_otherIso = results.Add(res::GetString(L"TimeKindUtc"), copy, true);
            m_otherFileTime = results.Add(res::GetString(L"TimeKindFileTime"), copy, true);
            m_otherFileTimeHex = results.Add(res::GetString(L"TimeFileTimeHex"), copy, true);
            m_otherDotNet = results.Add(res::GetString(L"TimeKindDotNet"), copy, true);
            m_otherUnixSeconds = results.Add(res::GetString(L"TimeKindUnixSeconds"), copy, true);
            m_otherUnixMilliseconds = results.Add(res::GetString(L"TimeKindUnixMilliseconds"), copy, true);
            m_otherTimestamp = results.Add(res::GetString(L"TimeKindMidi"), copy, true);
            m_otherWinRt = results.Add(res::GetString(L"TimeWinRt"), copy);
            m_otherSinceStart = results.Add(res::GetString(L"TimeMillisecondsSinceStart"), copy, true);
            m_otherWinRt.Text(res::GetString(L"TimeWinRtValue"));
            card.Body.Children().Append(results.Root());

            // any tick count as time
            m_lengthTicks = ui::MakeNumberBox(res::GetString(L"TimeLengthLabel"), 1500000, 0, 1e18, 150);
            m_lengthUnit = ui::MakeComboBox(res::GetString(L"TimeLengthUnitLabel"), {
                res::GetString(L"TimeLengthHundredNs"), res::GetString(L"TimeLengthMidi"), res::GetString(L"TimeLengthJr") }, 0, 230);
            m_lengthResult = controls::TextBlock{};
            m_lengthResult.IsTextSelectionEnabled(true);
            card.Body.Children().Append(ui::MakeFieldRow({ m_lengthTicks, m_lengthUnit, ui::MakeInlineText(m_lengthResult, false) }));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateOtherTimes(); };
            m_otherKind.SelectionChanged(update);
            m_otherInput.TextChanged(update);

            auto length = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateLength(); };
            m_lengthTicks.ValueChanged(length);
            m_lengthUnit.SelectionChanged(length);

            columns->Add(card, 1);
        }

        controls::StackPanel layout{};
        layout.Spacing(12);
        layout.Children().Append(strip);
        layout.Children().Append(columns->Root());

        WindowsTimePage().Children().Append(layout);
        m_columns.push_back(columns);

        UpdateTimePage();
        UpdateLength();
    }

    void MainWindow::UpdateLength() noexcept
    {
        try
        {
            if (!m_lengthTicks)
            {
                return;
            }

            auto const ticks = m_lengthTicks.Value();

            if (std::isnan(ticks) || ticks < 0)
            {
                m_lengthResult.Text(L"\u2014");
                return;
            }

            double perSecond{ static_cast<double>(::midicalc::HundredNanosecondsPerSecond) };

            switch (m_lengthUnit.SelectedIndex())
            {
            case 1: perSecond = static_cast<double>(TimestampFrequency()); break;
            case 2: perSecond = 31250.0; break;
            default: break;
            }

            m_lengthResult.Text(res::FormatString(L"TimeLengthValue", text::Number(ticks / perSecond * 1000.0, 3)));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to convert the length.")
    }

    void MainWindow::UpdateTimePage() noexcept
    {
        if (!m_timestamp)
        {
            return;
        }

        UpdateLiveClock();
        UpdateTimestamp();
        UpdateBetween(false);
        UpdateOtherTimes();
    }

    void MainWindow::StartLiveClock() noexcept
    {
        try
        {
            if (!m_liveNow)
            {
                return;
            }

            if (!m_liveTimer)
            {
                m_liveTimer = m_dispatcherQueue.CreateTimer();
                m_liveTimer.Interval(std::chrono::milliseconds{ 100 });
                m_liveTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto s = weak.get())
                        {
                            s->UpdateLiveClock();
                        }
                    });
            }

            m_liveTimer.Start();
            UpdateLiveClock();
        }
        CATCH_LOG()
    }

    void MainWindow::StopLiveClock() noexcept
    {
        try
        {
            if (m_liveTimer)
            {
                m_liveTimer.Stop();
            }
        }
        CATCH_LOG()
    }

    void MainWindow::UpdateLiveClock() noexcept
    {
        try
        {
            if (!m_liveNow)
            {
                return;
            }

            auto const frozen = m_liveFreeze.IsChecked() != nullptr && m_liveFreeze.IsChecked().Value();

            if (frozen)
            {
                return;
            }

            auto const now = TimestampNow();
            auto const frequency = TimestampFrequency();

            m_liveNow.Text(winrt::hstring{ text::Whole(now) });
            m_liveFrequency.Text(res::FormatString(L"TimeFrequencyValue", text::Whole(frequency), text::Number(1e9 / static_cast<double>(frequency), 3)));
            m_liveSinceStart.Text(winrt::hstring{ Since(::midicalc::TicksToSeconds(now, frequency)) });

            try
            {
                auto const timer = midi2::MidiClock::GetCurrentSystemTimerInfo();
                m_liveTimerPeriod.Text(res::FormatString(L"TimeTimerValue",
                    text::Number(timer.CurrentIntervalTicks / 10000.0, 3),
                    text::Number(timer.MinimumIntervalTicks / 10000.0, 3),
                    text::Number(timer.MaximumIntervalTicks / 10000.0, 3)));
            }
            catch (...)
            {
                m_liveTimerPeriod.Text(L"\u2014");
            }
        }
        CATCH_LOG()
    }

    void MainWindow::UpdateTimestamp() noexcept
    {
        try
        {
            if (!m_timestamp)
            {
                return;
            }

            auto const ticks = text::ParseUnsigned(m_timestamp.Text());
            auto const rateValue = m_ticksPerSecond.Value();
            auto const rate = std::isnan(rateValue) || rateValue < 1 ? TimestampFrequency() : static_cast<uint64_t>(rateValue);

            if (!ticks.has_value())
            {
                for (auto const& block : { m_tsSeconds, m_tsMilliseconds, m_tsMicroseconds, m_tsSinceStart, m_tsClock })
                {
                    block.Text(L"\u2014");
                }

                return;
            }

            auto const seconds = ::midicalc::TicksToSeconds(*ticks, rate);
            m_tsSeconds.Text(winrt::hstring{ text::Number(seconds, 7) });
            m_tsMilliseconds.Text(winrt::hstring{ text::Number(seconds * 1e3, 4) });
            m_tsMicroseconds.Text(winrt::hstring{ text::Number(seconds * 1e6, 1) });
            m_tsSinceStart.Text(winrt::hstring{ Since(seconds) });

            // clock time only means something for this PC's own counter
            auto const now = ClockNow();

            if (rate == now.TicksPerSecond && *ticks <= now.Timestamp + now.TicksPerSecond * 86400ull * 365ull)
            {
                auto const fileTime = ::midicalc::FileTimeForTimestamp(*ticks, now);
                m_tsClock.Text(fileTime.has_value() ? winrt::hstring{ LocalDateTime(*fileTime) } : winrt::hstring{ L"\u2014" });
            }
            else
            {
                m_tsClock.Text(res::GetString(L"TimeClockOtherPc"));
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to convert the timestamp.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateBetween(bool) noexcept
    {
        try
        {
            if (!m_betweenFrom)
            {
                return;
            }

            auto const frequency = TimestampFrequency();
            auto const from = text::ParseUnsigned(m_betweenFrom.Text());
            auto const to = text::ParseUnsigned(m_betweenTo.Text());

            if (!from.has_value() || !to.has_value())
            {
                ::midicalculator::ui::SetBigText(m_betweenResult.Value, std::wstring_view{ winrt::hstring{ L"\u2014" } });
                m_betweenResult.Detail.Text(res::GetString(L"TimeBetweenNeedBoth"));
            }
            else
            {
                auto const later = *to >= *from;
                auto const gap = later ? *to - *from : *from - *to;
                auto const seconds = ::midicalc::TicksToSeconds(gap, frequency) * (later ? 1.0 : -1.0);
                auto const beats = seconds * native::AppSettings::Current().Tempo() / 60.0;

                ::midicalculator::ui::SetBigText(m_betweenResult.Value, std::wstring_view{ winrt::hstring{ res::FormatString(L"TimeMillisecondsValue", text::Number(seconds * 1000.0, 3, true)) } });
                m_betweenResult.Detail.Text(res::FormatString(L"TimeBetweenDetail",
                    later ? text::Whole(gap) : text::SignedWhole(-static_cast<int64_t>(gap)),
                    text::Number(beats, 3), text::Number(native::AppSettings::Current().Tempo(), 3)));
            }

            auto const add = m_betweenAdd.Value();

            if (from.has_value() && !std::isnan(add))
            {
                auto const moved = ::midicalc::OffsetTimestamp(*from, add / 1000.0, frequency);
                m_betweenAdded.Text(moved.has_value() ? winrt::hstring{ std::to_wstring(*moved) } : res::GetString(L"TimeBeforeStart"));
            }
            else
            {
                m_betweenAdded.Text(L"\u2014");
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to work out the time between the timestamps.")
    }

    void MainWindow::UpdateOtherTimes() noexcept
    {
        try
        {
            if (!m_otherKind)
            {
                return;
            }

            auto const kind = static_cast<OtherKind>((std::max)(0, m_otherKind.SelectedIndex()));
            std::wstring const input{ m_otherInput.Text() };
            auto const now = ClockNow();
            std::optional<int64_t> fileTime{};

            switch (kind)
            {
            case OtherKind::LocalTime: fileTime = ParseDateTime(input, false); break;
            case OtherKind::UtcTime: fileTime = ParseDateTime(input, true); break;
            case OtherKind::FileTime:
                if (auto const v = text::ParseUnsigned(input); v.has_value() && *v <= static_cast<uint64_t>(INT64_MAX)) fileTime = static_cast<int64_t>(*v);
                break;
            case OtherKind::DotNetTicks:
                if (auto const v = text::ParseUnsigned(input); v.has_value() && *v <= static_cast<uint64_t>(INT64_MAX)) fileTime = ::midicalc::DotNetTicksToFileTime(static_cast<int64_t>(*v));
                break;
            case OtherKind::UnixSeconds:
            {
                wchar_t* end{ nullptr };
                auto const v = std::wcstod(input.c_str(), &end);
                if (end != input.c_str() && std::isfinite(v)) fileTime = ::midicalc::UnixSecondsToFileTime(v);
                break;
            }
            case OtherKind::UnixMilliseconds:
                if (auto const v = text::ParseUnsigned(input); v.has_value() && *v <= static_cast<uint64_t>(INT64_MAX / 10000)) fileTime = ::midicalc::UnixMillisecondsToFileTime(static_cast<int64_t>(*v));
                break;
            case OtherKind::MidiTimestamp:
                if (auto const v = text::ParseUnsigned(input); v.has_value()) fileTime = ::midicalc::FileTimeForTimestamp(*v, now);
                break;
            }

            if (!fileTime.has_value() || *fileTime < 0)
            {
                for (auto const& block : { m_otherLocal, m_otherIso, m_otherFileTime, m_otherFileTimeHex, m_otherDotNet, m_otherUnixSeconds, m_otherUnixMilliseconds, m_otherTimestamp, m_otherSinceStart })
                {
                    block.Text(L"\u2014");
                }

                return;
            }

            m_otherLocal.Text(winrt::hstring{ LocalDateTime(*fileTime) });
            m_otherIso.Text(winrt::hstring{ IsoUtc(*fileTime) });
            m_otherFileTime.Text(winrt::hstring{ std::to_wstring(*fileTime) });
            m_otherFileTimeHex.Text(winrt::hstring{ text::Hex(static_cast<uint64_t>(*fileTime), 16) });
            m_otherDotNet.Text(winrt::hstring{ std::to_wstring(::midicalc::FileTimeToDotNetTicks(*fileTime)) });
            m_otherUnixSeconds.Text(winrt::hstring{ std::format(L"{:.3f}", ::midicalc::FileTimeToUnixSeconds(*fileTime)) });
            m_otherUnixMilliseconds.Text(winrt::hstring{ std::to_wstring(::midicalc::FileTimeToUnixMilliseconds(*fileTime)) });

            auto const timestamp = ::midicalc::TimestampForFileTime(*fileTime, now);
            m_otherTimestamp.Text(timestamp.has_value() ? winrt::hstring{ std::to_wstring(*timestamp) } : res::GetString(L"TimeBeforeStart"));
            m_otherSinceStart.Text(timestamp.has_value() ? winrt::hstring{ std::to_wstring(*timestamp * 1000 / now.TicksPerSecond) } : res::GetString(L"TimeBeforeStart"));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to convert the time value.")
    }
}
