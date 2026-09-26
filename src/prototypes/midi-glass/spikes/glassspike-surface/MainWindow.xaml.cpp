// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "App.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace winrt::glassspike::implementation
{
    namespace
    {
        constexpr int64_t MicrosecondsPerSecond = 1000000;

        // Long enough to settle, short enough that a run is not mostly warm-up.
        constexpr int64_t WarmUpMicroseconds = 500000;
    }

    void MainWindow::OnRootLoaded(
        Windows::Foundation::IInspectable const& sender,
        RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_options = App::Options();

        Title(L"MIDI Glass rendering spike");

        // Never open maximized and never leave a window maximized: someone is using this machine.
        if (auto appWindow = AppWindow())
        {
            appWindow.Resize(Windows::Graphics::SizeInt32{ 1420, 1040 });
        }

        int32_t modeIndex = 0;
        if (m_options.Mode == L"composition") { modeIndex = 1; }
        else if (m_options.Mode == L"hybrid") { modeIndex = 2; }

        ModeCombo().SelectedIndex(modeIndex);
        ControlCountBox().Value(static_cast<double>(m_options.ControlCount));
        AnimateCountBox().Value(static_cast<double>(m_options.AnimatedCount));
        SecondsBox().Value(m_options.Seconds);

        m_result.PrivateBytesBaseline = gspike::TakeMemorySnapshot().PrivateBytes;

        HookPointerEvents();

        // A run where the window went to the back, or where the machine was doing something else,
        // is not evidence. Catching that here is cheaper than remembering it afterwards.
        m_activatedToken = Activated([this](auto&&, Microsoft::UI::Xaml::WindowActivatedEventArgs const& e)
            {
                if (m_running && e.WindowActivationState() == Microsoft::UI::Xaml::WindowActivationState::Deactivated)
                {
                    m_lostActivation = true;
                }
            });

        if (!m_options.ParseError.empty())
        {
            AppendResults(std::format(L"Command line: {}\n", m_options.ParseError));
        }

        if (m_options.SkipMidi)
        {
            SetStatus(L"MIDI probe skipped by request. The input path will not be measured.");
        }
        else
        {
            OpenProbeAsync();
        }

        if (m_options.AutoRun)
        {
            // Let the window settle before anything is timed.
            auto queue = DispatcherQueue();
            auto strong = get_strong();

            queue.TryEnqueue(
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                [strong]()
                {
                    strong->StartRun();
                });
        }
    }

    fire_and_forget MainWindow::OpenProbeAsync()
    {
        auto strong = get_strong();
        auto queue = DispatcherQueue();
        auto endpointId = m_options.EndpointDeviceId;

        try
        {
            // The session and connection calls block on the service. Doing that on the STA UI
            // thread hangs the window; only the send is allowed to happen there.
            co_await winrt::resume_background();

            const bool opened = strong->m_probe.Open(endpointId);
            strong->m_probeReady.store(opened);

            const std::wstring status = opened
                ? std::format(L"MIDI ready on {}", strong->m_probe.EndpointDeviceId())
                : std::format(L"MIDI not available: {}. Everything except the input path is still measured.",
                    strong->m_probe.Status());

            if (queue)
            {
                queue.TryEnqueue([strong, status]() { strong->SetStatus(status); });
            }
        }
        catch (...)
        {
            strong->m_probeReady.store(false);
        }
    }

    std::wstring MainWindow::SelectedMode()
    {
        if (auto item = ModeCombo().SelectedItem().try_as<ComboBoxItem>())
        {
            if (auto tag = item.Tag())
            {
                return std::wstring{ unbox_value_or<hstring>(tag, L"xaml") };
            }
        }

        return L"xaml";
    }

    void MainWindow::SetStatus(std::wstring const& text)
    {
        StatusText().Text(hstring{ text });
    }

    void MainWindow::AppendResults(std::wstring const& text)
    {
        std::wstring existing{ ResultsText().Text() };

        if (!existing.empty() && existing.back() != L'\n' && existing.back() != L'\r')
        {
            existing += L"\r\n";
        }

        existing += text;

        ResultsText().Text(hstring{ existing });
    }

    void MainWindow::OnBuildClick(
        Windows::Foundation::IInspectable const& sender,
        RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        BuildPage();

        SetStatus(std::format(L"{} built {} controls in {:.1f} ms, {} XAML elements.",
            m_result.Mode, m_result.ControlCount, m_result.BuildMilliseconds, m_result.XamlElementCount));
    }

    void MainWindow::OnRunClick(
        Windows::Foundation::IInspectable const& sender,
        RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        StartRun();
    }

    void MainWindow::OnThemeClick(
        Windows::Foundation::IInspectable const& sender,
        RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (!m_renderer)
        {
            SetStatus(L"Build a page first.");
            return;
        }

        m_alternateTheme = !m_alternateTheme;

        const int64_t start = gspike::NowMicroseconds();
        m_renderer->ApplyTheme(m_alternateTheme);
        SurfaceCanvas().UpdateLayout();
        const double elapsed = static_cast<double>(gspike::NowMicroseconds() - start) / 1000.0;

        const bool reached = m_renderer->VerifyTheme(m_alternateTheme);

        SetStatus(std::format(L"Theme swap took {:.1f} ms and reached every control: {}.",
            elapsed, reached ? L"yes" : L"NO"));
    }

    void MainWindow::BuildPage()
    {
        const uint32_t controlCount = static_cast<uint32_t>(std::max(1.0, ControlCountBox().Value()));
        const uint32_t animateCount = static_cast<uint32_t>(std::max(0.0, AnimateCountBox().Value()));
        const std::wstring mode = SelectedMode();

        m_activeControl = -1;
        m_alternateTheme = false;

        if (m_renderer)
        {
            m_renderer->Teardown();
            m_renderer.reset();
        }

        SurfaceCanvas().Children().Clear();
        SurfaceCanvas().UpdateLayout();

        m_page = gspike::BuildPage(controlCount, animateCount);

        SurfaceCanvas().Width(m_page.Width);
        SurfaceCanvas().Height(m_page.Height);

        if (mode == L"composition") { m_renderer = gspike::MakeCompositionRenderer(); }
        else if (mode == L"hybrid") { m_renderer = gspike::MakeHybridRenderer(); }
        else { m_renderer = gspike::MakeXamlElementRenderer(); }

        const auto baseline = gspike::TakeMemorySnapshot();

        const int64_t start = gspike::NowMicroseconds();
        m_renderer->Build(SurfaceCanvas(), m_page);
        const int64_t built = gspike::NowMicroseconds();

        const auto afterBuild = gspike::TakeMemorySnapshot();

        m_result = gspike::RunResult{};
        m_result.Mode = mode;
        m_result.ControlCount = controlCount;
        m_result.AnimatedCount = static_cast<uint32_t>(m_page.AnimatedIndices.size());
        m_result.BuildMilliseconds = static_cast<double>(built - start) / 1000.0;
        m_result.XamlElementCount = m_renderer->XamlElementCount();
        m_result.HasAutomationPeers = m_renderer->HasAutomationPeers();
        m_result.PrivateBytesBaseline = baseline.PrivateBytes;
        m_result.PrivateBytesAfterBuild = afterBuild.PrivateBytes;
        m_result.WorkingSetAfterBuild = afterBuild.WorkingSetBytes;

        m_buildDoneUs = built;
        m_awaitingFirstFrame = true;
        m_pageBuilt = true;
    }

    void MainWindow::MeasureSetValueCost()
    {
        if (!m_renderer || m_page.Controls.empty())
        {
            return;
        }

        m_setValueSamples.Clear();

        // A single update is faster than the clock can resolve on its own, so each sample is a
        // batch of a hundred divided back down.
        constexpr uint32_t batch = 100;
        constexpr uint32_t batches = 200;

        const uint32_t count = static_cast<uint32_t>(m_page.Controls.size());
        uint32_t index = 0;
        float value = 0.0f;

        for (uint32_t b = 0; b < batches; b++)
        {
            const int64_t start = gspike::NowMicroseconds();

            for (uint32_t i = 0; i < batch; i++)
            {
                value += 0.013f;
                if (value > 1.0f) { value -= 1.0f; }

                m_renderer->SetValue(index, value, 0.0f);

                index++;
                if (index >= count) { index = 0; }
            }

            const int64_t elapsed = gspike::NowMicroseconds() - start;

            m_setValueSamples.Add(static_cast<double>(elapsed) / static_cast<double>(batch));
        }

        // Put the page back the way the model describes it.
        for (uint32_t i = 0; i < count; i++)
        {
            m_renderer->SetValue(i, m_page.Controls[i].Value, 0.0f);
        }
    }

    void MainWindow::MeasureSendCost()
    {
        m_sendSamples.Clear();

        if (!m_probeReady.load())
        {
            return;
        }

        // The half of the hot path that needs no hands. A send is faster than the clock can
        // resolve on its own, so each sample is a batch of fifty divided back down.
        constexpr uint32_t batch = 50;
        constexpr uint32_t batches = 200;

        for (uint32_t b = 0; b < batches; b++)
        {
            const int64_t start = gspike::NowMicroseconds();

            for (uint32_t i = 0; i < batch; i++)
            {
                m_probe.SendControlChange(0, 0, 7, static_cast<float>(i) / static_cast<float>(batch));
            }

            m_sendSamples.Add(
                static_cast<double>(gspike::NowMicroseconds() - start) / static_cast<double>(batch));
        }
    }

    void MainWindow::StartRun()
    {
        if (m_running)
        {
            return;
        }

        BuildPage();
        MeasureSetValueCost();
        MeasureSendCost();

        m_result.SetValueP50Microseconds = m_setValueSamples.Percentile(0.50);
        m_result.SetValueP99Microseconds = m_setValueSamples.Percentile(0.99);

        if (m_sendSamples.Count() > 0)
        {
            m_result.SendP50Microseconds = m_sendSamples.Percentile(0.50);
            m_result.SendP99Microseconds = m_sendSamples.Percentile(0.99);
        }

        m_result.DurationSeconds = std::max(1.0, SecondsBox().Value());

        m_frameSamples.Clear();
        m_animateSamples.Clear();
        m_pointerToSendSamples.Clear();
        m_handlerToSendSamples.Clear();
        m_sawTouch = m_sawPen = m_sawMouse = false;
        m_sawPointerTimestamp = false;
        m_pointerTimestampTrusted = false;
        m_animationPhase = 0.0;
        m_lostActivation = false;

        m_cpuAtRunStart = gspike::TakeCpuSnapshot();
        m_runStartUs = gspike::NowMicroseconds();
        m_lastFrameUs = 0;
        m_running = true;

        BuildButton().IsEnabled(false);
        RunButton().IsEnabled(false);

        SetStatus(std::format(
            L"Running {} for {:.0f} s. Drag a fader with a mouse, a finger and a pen to fill in the input path.",
            m_result.Mode, m_result.DurationSeconds));

        m_renderingToken = CompositionTarget::Rendering({ this, &MainWindow::OnRendering });
    }

    void MainWindow::OnRendering(
        Windows::Foundation::IInspectable const& sender,
        Windows::Foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        const int64_t now = gspike::NowMicroseconds();

        if (m_awaitingFirstFrame)
        {
            m_result.FirstLayoutMilliseconds = static_cast<double>(now - m_buildDoneUs) / 1000.0;
            m_awaitingFirstFrame = false;
        }

        if (!m_running)
        {
            return;
        }

        const int64_t sinceStart = now - m_runStartUs;
        const bool warmedUp = sinceStart > WarmUpMicroseconds;

        if (m_lastFrameUs != 0 && warmedUp)
        {
            m_frameSamples.Add(static_cast<double>(now - m_lastFrameUs) / 1000.0);
        }

        m_lastFrameUs = now;

        // The animation: the twelve controls that move, driven from the UI thread the way an
        // incoming stream of MIDI feedback would drive them. The layout pass is inside the
        // measurement on purpose - for a XAML element tree that is most of the cost, and leaving
        // it out would make approach A look free.
        const int64_t animateStart = gspike::NowMicroseconds();

        m_animationPhase += 0.04;

        uint32_t slot = 0;
        for (uint32_t index : m_page.AnimatedIndices)
        {
            const double phase = m_animationPhase + static_cast<double>(slot) * 0.5;
            const float value = static_cast<float>(0.5 + 0.5 * std::sin(phase));
            const float bloom = static_cast<float>(0.5 + 0.5 * std::sin(phase * 0.5));

            m_renderer->SetValue(index, value, bloom);

            slot++;
        }

        SurfaceCanvas().UpdateLayout();

        if (warmedUp && !m_page.AnimatedIndices.empty())
        {
            m_animateSamples.Add(static_cast<double>(gspike::NowMicroseconds() - animateStart));
        }

        if (static_cast<double>(sinceStart) / static_cast<double>(MicrosecondsPerSecond) >= m_result.DurationSeconds)
        {
            FinishRun();
        }
    }

    void MainWindow::FinishRun()
    {
        m_running = false;

        if (m_renderingToken)
        {
            CompositionTarget::Rendering(m_renderingToken);
            m_renderingToken = {};
        }

        const auto cpuAtEnd = gspike::TakeCpuSnapshot();

        m_result.SystemBusyPercent = gspike::SystemBusyFraction(m_cpuAtRunStart, cpuAtEnd) * 100.0;
        m_result.ProcessCpuMilliseconds = gspike::ProcessCpuMilliseconds(m_cpuAtRunStart, cpuAtEnd);
        m_result.LostActivation = m_lostActivation;

        // The threshold is what this process can account for on its own plus headroom. One core
        // of twenty four is about four percent, and the spike never needs more than a fraction
        // of that, so anything past fifteen percent means somebody else was working.
        const double selfPercent =
            (m_result.ProcessCpuMilliseconds / (m_result.DurationSeconds * 1000.0))
            * 100.0 / static_cast<double>(std::max(1u, std::thread::hardware_concurrency()));

        m_result.Clean = !m_lostActivation && (m_result.SystemBusyPercent - selfPercent) < 15.0;

        m_result.FrameP50 = m_frameSamples.Percentile(0.50);
        m_result.FrameP95 = m_frameSamples.Percentile(0.95);
        m_result.FrameP99 = m_frameSamples.Percentile(0.99);
        m_result.FrameMax = m_frameSamples.Max();
        m_result.FrameCount = m_frameSamples.Count();
        m_result.FramesOver20Ms = m_frameSamples.CountAbove(20.0);

        m_result.AnimateP50Microseconds = m_animateSamples.Percentile(0.50);
        m_result.AnimateP99Microseconds = m_animateSamples.Percentile(0.99);

        if (m_pointerToSendSamples.Count() > 0)
        {
            m_result.PointerToSendP50Microseconds = m_pointerToSendSamples.Percentile(0.50);
            m_result.PointerToSendP99Microseconds = m_pointerToSendSamples.Percentile(0.99);
            m_result.HandlerToSendP50Microseconds = m_handlerToSendSamples.Percentile(0.50);
            m_result.HandlerToSendP99Microseconds = m_handlerToSendSamples.Percentile(0.99);
            m_result.PointerSampleCount = m_pointerToSendSamples.Count();
            m_result.PointerTimestampTrusted = m_pointerTimestampTrusted;
        }

        std::wstring devices;
        if (m_sawMouse) { devices += L"mouse "; }
        if (m_sawTouch) { devices += L"touch "; }
        if (m_sawPen) { devices += L"pen "; }
        if (devices.empty()) { devices = L"none"; }
        m_result.PointerDeviceTypes = devices;

        m_result.LatencyEndpoint = m_probeReady.load() ? m_probe.EndpointDeviceId() : std::wstring{};
        m_result.LatencyNote = m_probeReady.load() ? std::wstring{} : std::format(L"MIDI was not open: {}", m_probe.Status());

        // The theme swap is timed as part of the run so every mode reports one.
        const int64_t themeStart = gspike::NowMicroseconds();
        m_renderer->ApplyTheme(true);
        SurfaceCanvas().UpdateLayout();
        m_result.ThemeSwapMilliseconds = static_cast<double>(gspike::NowMicroseconds() - themeStart) / 1000.0;
        m_result.ThemeSwapReachedEveryControl = m_renderer->VerifyTheme(true);
        m_renderer->ApplyTheme(false);
        m_alternateTheme = false;

        if (m_frameSamples.Overflowed() || m_animateSamples.Overflowed())
        {
            m_result.LatencyNote += L" Sample buffer filled; the run was longer than the spike is sized for.";
        }

        AppendResults(gspike::FormatResultText(m_result));

        if (!m_options.OutputPath.empty())
        {
            gspike::AppendLineToFile(m_options.OutputPath, gspike::FormatResultJson(m_result));
        }

        BuildButton().IsEnabled(true);
        RunButton().IsEnabled(true);

        SetStatus(std::format(L"{} finished. {} frames measured.", m_result.Mode, m_result.FrameCount));

        if (m_options.AutoRun)
        {
            Application::Current().Exit();
        }
    }

    void MainWindow::HookPointerEvents()
    {
        auto canvas = SurfaceCanvas();

        canvas.PointerPressed({ this, &MainWindow::OnSurfacePointerPressed });
        canvas.PointerMoved({ this, &MainWindow::OnSurfacePointerMoved });
        canvas.PointerReleased({ this, &MainWindow::OnSurfacePointerReleased });
        canvas.PointerCaptureLost({ this, &MainWindow::OnSurfacePointerReleased });
    }

    void MainWindow::NoteDeviceType(Microsoft::UI::Input::PointerDeviceType type)
    {
        switch (type)
        {
        case Microsoft::UI::Input::PointerDeviceType::Touch: m_sawTouch = true; break;
        case Microsoft::UI::Input::PointerDeviceType::Pen: m_sawPen = true; break;
        default: m_sawMouse = true; break;
        }
    }

    void MainWindow::OnSurfacePointerPressed(
        Windows::Foundation::IInspectable const& sender,
        PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_renderer)
        {
            return;
        }

        auto point = args.GetCurrentPoint(SurfaceCanvas());
        const auto position = point.Position();

        m_activeControl = m_renderer->HitTest(
            static_cast<float>(position.X),
            static_cast<float>(position.Y));

        if (m_activeControl >= 0)
        {
            NoteDeviceType(args.Pointer().PointerDeviceType());
            SurfaceCanvas().CapturePointer(args.Pointer());
            args.Handled(true);
        }
    }

    void MainWindow::OnSurfacePointerMoved(
        Windows::Foundation::IInspectable const& sender,
        PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_activeControl < 0 || !m_renderer)
        {
            return;
        }

        // Everything from here to the send is the hot path. No allocation, no lookup by name,
        // and the message goes out from this handler rather than from a later render tick.
        const int64_t handlerEntry = gspike::NowMicroseconds();

        auto point = args.GetCurrentPoint(SurfaceCanvas());
        const auto position = point.Position();
        const uint64_t pointerTimestamp = point.Timestamp();

        auto const& descriptor = m_page.Controls[static_cast<size_t>(m_activeControl)];

        float value = 0.0f;

        if (descriptor.Kind == gspike::ControlKind::Fader)
        {
            const float top = descriptor.Y + gspike::PipeInset;
            const float length = std::max(1.0f, descriptor.Height - gspike::PipeInset * 2.0f);

            value = 1.0f - (static_cast<float>(position.Y) - top) / length;
        }
        else
        {
            const float left = descriptor.X + gspike::PipeInset;
            const float length = std::max(1.0f, descriptor.Width - gspike::PipeInset * 2.0f);

            value = (static_cast<float>(position.X) - left) / length;
        }

        value = std::clamp(value, 0.0f, 1.0f);

        int64_t sentAt = 0;

        if (m_probeReady.load())
        {
            sentAt = m_probe.SendControlChange(
                descriptor.Group,
                descriptor.Channel,
                descriptor.ControllerNumber,
                value);
        }

        m_renderer->SetValue(static_cast<uint32_t>(m_activeControl), value, 1.0f);
        m_activeValue = value;

        NoteDeviceType(args.Pointer().PointerDeviceType());

        if (sentAt != 0)
        {
            m_handlerToSendSamples.Add(static_cast<double>(sentAt - handlerEntry));

            // The system stamps pointer input in microseconds. Whether that shares a timebase
            // with QueryPerformanceCounter is checked rather than assumed: a delta outside a
            // couple of hundred milliseconds means the two clocks are unrelated and the number
            // would be meaningless.
            const int64_t delta = handlerEntry - static_cast<int64_t>(pointerTimestamp);

            if (!m_sawPointerTimestamp)
            {
                m_sawPointerTimestamp = true;
                m_pointerTimestampTrusted = delta >= 0 && delta < 200000;
            }

            if (m_pointerTimestampTrusted)
            {
                m_pointerToSendSamples.Add(static_cast<double>(sentAt - static_cast<int64_t>(pointerTimestamp)));
            }
            else
            {
                m_pointerToSendSamples.Add(static_cast<double>(sentAt - handlerEntry));
            }
        }

        args.Handled(true);
    }

    void MainWindow::OnSurfacePointerReleased(
        Windows::Foundation::IInspectable const& sender,
        PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_activeControl >= 0 && m_renderer)
        {
            // The value the hand left it at stays; only the bloom decays away.
            m_renderer->SetValue(static_cast<uint32_t>(m_activeControl), m_activeValue, 0.0f);
        }

        m_activeControl = -1;

        SurfaceCanvas().ReleasePointerCapture(args.Pointer());
    }
}
