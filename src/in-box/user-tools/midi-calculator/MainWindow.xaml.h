// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MainWindow.g.h"

#include "AppSettings.h"
#include "CardBuilder.h"
#include "TapTempo.h"

namespace winrt::midicalculator::implementation
{
    // Each page's cards are built in code from CardBuilder, in its own MainWindow<Page>.cpp, so
    // they all look the same and the XAML stays small.
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnRootSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);
        void OnPageHostSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args);
        void OnNavigationSelectionChanged(controls::NavigationView const& sender, controls::NavigationViewSelectionChangedEventArgs const& args);

        void OnAlwaysOnTopChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSettingsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        void OnSessionTempoChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnSessionSampleRateChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnSessionReferenceChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args);
        void OnSessionFrameRateChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args);
        void OnSessionMeterLostFocus(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);
        void OnSessionMeterKeyDown(foundation::IInspectable const& sender, xaml::Input::KeyRoutedEventArgs const& args);
        void OnSessionResetClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args);

        // called before Activate so the window never appears at the wrong size first
        void RestoreWindowPlacement() noexcept;

    private:
        void InitializeWindowChrome() noexcept;
        void ShowPage(_In_ ::midicalculator::CalculatorPage page) noexcept;
        void ApplyStartupOptions() noexcept;

        // Copies to the clipboard, keeps it in History, and says so in the status line.
        void CopyAnswer(_In_ winrt::hstring const& page, _In_ winrt::hstring const& label, _In_ winrt::hstring const& value) noexcept;
        ::midicalculator::ui::CopyHandler CopyFor(_In_ winrt::hstring const& page) noexcept;
        void ShowStatus(_In_ winrt::hstring const& text) noexcept;

        // ---- session values (MainWindowSession.cpp) ----
        void LoadSessionControls() noexcept;
        void ApplySessionMeter() noexcept;
        void UseAsSessionTempo(_In_ double tempo, _In_ winrt::hstring const& page, _In_ winrt::hstring const& question) noexcept;
        // every page that reads a session value recalculates
        void SessionChanged() noexcept;
        void UpdateReferenceLabel() noexcept;

        // ---- Tempo and beats (MainWindowTempo.cpp) ----
        void BuildTempoPage();
        void UpdateTempoPage() noexcept;
        void UpdateBeatCalculator() noexcept;
        void UpdateNoteLengths() noexcept;
        void UpdateTempoChange(_In_ bool fromPercent) noexcept;
        void Tap() noexcept;

        // ---- Notes and pitch (MainWindowPitch.cpp) ----
        void BuildPitchPage();
        void UpdatePitchPage() noexcept;
        void UpdateNoteTable() noexcept;

        // ---- Timecode and samples (MainWindowTimecode.cpp) ----
        void BuildTimecodePage();
        void UpdateTimecodePage() noexcept;
        void UpdateTimecodeCalculator() noexcept;
        void UpdateTimecodeConvert() noexcept;
        void UpdateSamples() noexcept;
        void AddToTape(_In_ winrt::hstring const& question, _In_ winrt::hstring const& answer) noexcept;

        // ---- MIDI 2.0 values (MainWindowMidi2.cpp) ----
        void BuildMidi2Page();
        void UpdateMidi2Page() noexcept;
        void UpdateNotePitch() noexcept;
        void UpdatePitchBend() noexcept;
        void UpdateScaling() noexcept;
        void UpdateUmpTiming() noexcept;
        void UpdateDecoder() noexcept;

        // ---- Windows timestamps (MainWindowTime.cpp) ----
        void BuildTimePage();
        void UpdateTimePage() noexcept;
        void UpdateLiveClock() noexcept;
        void UpdateTimestamp() noexcept;
        void UpdateBetween(_In_ bool fromOffset) noexcept;
        void UpdateOtherTimes() noexcept;
        void StartLiveClock() noexcept;
        void StopLiveClock() noexcept;

        // ---- History (MainWindowHistory.cpp) ----
        void BuildHistoryPage();
        void RefreshHistory() noexcept;

        midiapp::WindowChrome m_chrome{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        bool m_initialized{ false };
        bool m_suppressSession{ false };
        ::midicalculator::CalculatorPage m_page{ ::midicalculator::CalculatorPage::Tempo };
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_statusTimer{ nullptr };

        std::vector<std::shared_ptr<::midicalculator::ui::CardColumns>> m_columns{};

        // ---- Tempo and beats ----
        midiapp::TapTempo m_tapTempo{ 1.0, 1000.0, 0.0 };
        std::optional<double> m_tappedTempo{};
        controls::TextBlock m_tapValue{ nullptr };
        controls::TextBlock m_tapDetail{ nullptr };

        std::shared_ptr<::midicalculator::ui::Segmented> m_beatFind{};
        controls::ComboBox m_beatMeter{ nullptr };
        controls::NumberBox m_beatBars{ nullptr };
        controls::NumberBox m_beatBeats{ nullptr };
        controls::TextBox m_beatLength{ nullptr };
        controls::ComboBox m_beatLengthUnit{ nullptr };
        controls::NumberBox m_beatTempo{ nullptr };
        ::midicalculator::ui::BigResult m_beatResult{};
        std::optional<double> m_beatAnswerTempo{};

        controls::TextBlock m_noteLengthsTitle{ nullptr };
        std::shared_ptr<::midicalculator::ui::Segmented> m_noteUnit{};
        controls::TextBlock m_oneBarLabel{ nullptr };
        controls::TextBlock m_preDelay{ nullptr };
        controls::StackPanel m_tapDots{ nullptr };
        std::array<std::array<controls::TextBlock, 3>, 7> m_noteCells{};
        controls::TextBlock m_oneBar{ nullptr };
        controls::TextBlock m_sixteenBars{ nullptr };

        controls::NumberBox m_changeFrom{ nullptr };
        controls::NumberBox m_changeTo{ nullptr };
        controls::NumberBox m_changePercent{ nullptr };
        controls::TextBlock m_changeSpeed{ nullptr };
        controls::TextBlock m_changeLength{ nullptr };
        controls::TextBlock m_changePitch{ nullptr };
        bool m_suppressChange{ false };

        // ---- Notes and pitch ----
        controls::TextBox m_pitchNote{ nullptr };
        controls::NumberBox m_pitchCents{ nullptr };
        ::midicalculator::ui::BigResult m_pitchFrequency{};
        controls::TextBlock m_pitchNumber{ nullptr };
        controls::TextBlock m_pitchPeriod{ nullptr };
        controls::TextBlock m_pitchSamples{ nullptr };
        controls::TextBlock m_pitchSamplesLabel{ nullptr };
        controls::TextBlock m_pitchOtherNames{ nullptr };
        controls::Grid m_gauge{ nullptr };
        xaml::Shapes::Rectangle m_gaugeNeedle{ nullptr };
        controls::HyperlinkButton m_frequencyLink{ nullptr };
        controls::NumberBox m_intervalMove{ nullptr };
        bool m_suppressInterval{ false };
        std::array<controls::TextBlock, 12> m_tableNumbers{};

        controls::NumberBox m_frequency{ nullptr };
        ::midicalculator::ui::BigResult m_frequencyNote{};
        controls::TextBlock m_frequencyFraction{ nullptr };
        controls::TextBlock m_frequencyPitch79{ nullptr };

        controls::TextBox m_intervalFrom{ nullptr };
        controls::TextBox m_intervalTo{ nullptr };
        controls::TextBlock m_intervalName{ nullptr };
        controls::TextBlock m_intervalDistance{ nullptr };
        controls::TextBlock m_intervalRatio{ nullptr };
        controls::TextBlock m_intervalFrequencies{ nullptr };

        controls::ComboBox m_temperament{ nullptr };
        controls::ComboBox m_temperamentRoot{ nullptr };
        controls::NumberBox m_tableOctave{ nullptr };
        std::array<controls::TextBlock, 12> m_tableNames{};
        std::array<controls::TextBlock, 12> m_tableHertz{};

        // ---- Timecode and samples ----
        controls::ComboBox m_tcRate{ nullptr };
        std::shared_ptr<::midicalculator::ui::Segmented> m_tcMode{};
        controls::TextBox m_tcA{ nullptr };
        controls::TextBox m_tcB{ nullptr };
        std::array<controls::Primitives::ToggleButton, 4> m_tcOperators{};
        int32_t m_tcOperator{ 0 };
        bool m_tcUpdatingOperators{ false };
        ::midicalculator::ui::BigResult m_tcResult{};
        controls::StackPanel m_tcTape{ nullptr };
        std::optional<int64_t> m_tcAnswer{};
        winrt::hstring m_tcQuestion{};
        winrt::hstring m_tcAnswerText{};

        controls::TextBox m_cvTimecode{ nullptr };
        controls::ComboBox m_cvRate{ nullptr };
        controls::TextBlock m_cvFrames{ nullptr };
        controls::TextBlock m_cvRealTime{ nullptr };
        controls::TextBlock m_cvSamples{ nullptr };
        controls::TextBlock m_cvSamplesLabel{ nullptr };
        controls::TextBlock m_cvBars{ nullptr };
        controls::TextBlock m_cvBarsLabel{ nullptr };
        controls::ComboBox m_cvTargetRate{ nullptr };
        controls::TextBlock m_cvTarget{ nullptr };

        controls::TextBlock m_samplesTitle{ nullptr };
        controls::TextBox m_samples{ nullptr };
        controls::TextBlock m_samplesResult{ nullptr };
        std::array<controls::TextBlock, 2> m_bufferRates{};
        std::array<std::array<controls::TextBlock, 6>, 2> m_bufferCells{};

        // ---- MIDI 2.0 values ----
        controls::TextBox m_m2Note{ nullptr };
        controls::NumberBox m_m2Cents{ nullptr };
        controls::NumberBox m_m2Hertz{ nullptr };
        controls::Grid m_m2Bits{ nullptr };
        controls::TextBlock m_m2Step{ nullptr };
        controls::TextBlock m_m2Pitch79{ nullptr };
        controls::TextBlock m_m2SoundsAs{ nullptr };
        controls::TextBlock m_m2Pitch725{ nullptr };
        controls::TextBlock m_m2Widened{ nullptr };
        controls::TextBox m_m2Read{ nullptr };
        controls::TextBlock m_m2ReadResult{ nullptr };
        bool m_suppressNotePitch{ false };

        controls::NumberBox m_bendRange{ nullptr };
        controls::NumberBox m_bendRangeCents{ nullptr };
        controls::NumberBox m_bendCents{ nullptr };
        controls::TextBlock m_bend32{ nullptr };
        controls::TextBlock m_bend14{ nullptr };
        controls::TextBlock m_bend14In32{ nullptr };
        controls::TextBlock m_bendRpn2{ nullptr };
        controls::TextBlock m_bendRpn1{ nullptr };

        controls::TextBox m_scaleValue{ nullptr };
        controls::ComboBox m_scaleBits{ nullptr };
        controls::ComboBox m_scaleMethod{ nullptr };
        std::array<std::array<controls::TextBlock, 3>, 4> m_scaleCells{};
        std::array<controls::Border, 4> m_scaleHighlights{};

        controls::TextBox m_jrTicks{ nullptr };
        controls::TextBlock m_jrResult{ nullptr };
        controls::NumberBox m_dctpq{ nullptr };
        controls::NumberBox m_dcTicks{ nullptr };
        controls::TextBlock m_dcResult{ nullptr };
        controls::TextBlock m_dcResultLabel{ nullptr };

        controls::TextBox m_umpWords{ nullptr };
        controls::TextBlock m_umpName{ nullptr };
        controls::TextBlock m_umpDetail{ nullptr };
        controls::Grid m_umpBits{ nullptr };
        controls::TextBlock m_umpMidi1{ nullptr };
        controls::TextBox m_mfrId{ nullptr };
        controls::TextBlock m_mfrResult{ nullptr };

        // ---- Windows timestamps ----
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_liveTimer{ nullptr };
        controls::TextBlock m_liveNow{ nullptr };
        controls::TextBlock m_liveFrequency{ nullptr };
        controls::TextBlock m_liveSinceStart{ nullptr };
        controls::TextBlock m_liveTimerPeriod{ nullptr };
        controls::Primitives::ToggleButton m_liveFreeze{ nullptr };

        controls::TextBox m_timestamp{ nullptr };
        controls::NumberBox m_ticksPerSecond{ nullptr };
        controls::TextBlock m_tsSeconds{ nullptr };
        controls::TextBlock m_tsMilliseconds{ nullptr };
        controls::TextBlock m_tsMicroseconds{ nullptr };
        controls::TextBlock m_tsSinceStart{ nullptr };
        controls::TextBlock m_tsClock{ nullptr };

        controls::TextBox m_betweenFrom{ nullptr };
        controls::TextBox m_betweenTo{ nullptr };
        ::midicalculator::ui::BigResult m_betweenResult{};
        controls::NumberBox m_betweenAdd{ nullptr };
        controls::TextBlock m_betweenAdded{ nullptr };

        controls::ComboBox m_otherKind{ nullptr };
        controls::TextBox m_otherInput{ nullptr };
        controls::TextBlock m_otherLocal{ nullptr };
        controls::TextBlock m_otherIso{ nullptr };
        controls::TextBlock m_otherFileTime{ nullptr };
        controls::TextBlock m_otherFileTimeHex{ nullptr };
        controls::TextBlock m_otherDotNet{ nullptr };
        controls::TextBlock m_otherUnixSeconds{ nullptr };
        controls::TextBlock m_otherUnixMilliseconds{ nullptr };
        controls::TextBlock m_otherTimestamp{ nullptr };
        controls::TextBlock m_otherWinRt{ nullptr };
        controls::TextBlock m_otherSinceStart{ nullptr };
        controls::NumberBox m_lengthTicks{ nullptr };
        controls::ComboBox m_lengthUnit{ nullptr };
        controls::TextBlock m_lengthResult{ nullptr };
        void UpdateLength() noexcept;

        // ---- History ----
        controls::TextBox m_historySearch{ nullptr };
        controls::StackPanel m_historyList{ nullptr };
        controls::TextBlock m_historyEmpty{ nullptr };
        controls::StackPanel m_historyTools{ nullptr };
        std::shared_ptr<::midicalculator::ui::CardColumns> m_historyColumns{};
        void SaveHistoryAsText() noexcept;
    };
}

namespace winrt::midicalculator::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
