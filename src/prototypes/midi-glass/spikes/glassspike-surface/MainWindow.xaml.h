// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "MainWindow.g.h"

#include "SpikeOptions.h"
#include "SurfaceModel.h"
#include "SurfaceRenderer.h"
#include "RunStats.h"
#include "LatencyProbe.h"

namespace winrt::glassspike::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;

        void OnRootLoaded(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnBuildClick(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnRunClick(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void OnThemeClick(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void BuildPage();
        void StartRun();
        void FinishRun();
        void MeasureSetValueCost();
        void MeasureSendCost();
        void HookPointerEvents();
        void OnRendering(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& args);

        void OnSurfacePointerPressed(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnSurfacePointerMoved(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnSurfacePointerReleased(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

        fire_and_forget OpenProbeAsync();
        void SetStatus(std::wstring const& text);
        void AppendResults(std::wstring const& text);
        std::wstring SelectedMode();
        void NoteDeviceType(Microsoft::UI::Input::PointerDeviceType type);

        gspike::SpikeOptions m_options{};
        gspike::PageModel m_page{};
        std::unique_ptr<gspike::ISurfaceRenderer> m_renderer;
        gspike::LatencyProbe m_probe{};
        std::atomic<bool> m_probeReady{ false };

        gspike::RunResult m_result{};

        // Sized for a ten minute run at 240 Hz, so a long run never reallocates mid-measurement.
        gspike::SampleSet m_frameSamples{ 150000 };
        gspike::SampleSet m_animateSamples{ 150000 };
        gspike::SampleSet m_setValueSamples{ 512 };
        gspike::SampleSet m_sendSamples{ 512 };
        gspike::SampleSet m_pointerToSendSamples{ 20000 };
        gspike::SampleSet m_handlerToSendSamples{ 20000 };

        winrt::event_token m_renderingToken{};
        bool m_running{ false };
        bool m_pageBuilt{ false };
        bool m_alternateTheme{ false };
        bool m_pointerTimestampTrusted{ false };
        bool m_sawPointerTimestamp{ false };

        int64_t m_runStartUs{ 0 };
        int64_t m_lastFrameUs{ 0 };
        int64_t m_buildDoneUs{ 0 };
        bool m_awaitingFirstFrame{ false };

        gspike::CpuSnapshot m_cpuAtRunStart{};
        bool m_lostActivation{ false };
        winrt::event_token m_activatedToken{};

        double m_animationPhase{ 0.0 };
        int32_t m_activeControl{ -1 };
        float m_activeValue{ 0.0f };
        bool m_sawTouch{ false };
        bool m_sawPen{ false };
        bool m_sawMouse{ false };
    };
}

namespace winrt::glassspike::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
