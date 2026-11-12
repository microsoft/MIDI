// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"

#include "App.xaml.h"
#include "CalcHistory.h"
#include "PopupBackdrop.h"
#include "StringResources.h"
#include "resource.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;

namespace winrt::midicalculator::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    }

    void MainWindow::RestoreWindowPlacement() noexcept
    {
        // static, because this runs before the chrome is initialized
        midiapp::WindowChrome::RestorePlacement(*this, native::AppSettings::Current(), 1180, 820);
    }

    _Use_decl_annotations_
    void MainWindow::OnRootLoaded(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            InitializeWindowChrome();

            ::midicalculator::ui::SetCopyNameFormat(res::GetString(L"CopyNameFormat"));

            native::History::Current().Load();

            LoadSessionControls();

            // A page that fails to build leaves the rest of the app working.
            try { BuildTempoPage(); } MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to build the Tempo and beats page.")
            try { BuildPitchPage(); } MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to build the Notes and pitch page.")
            try { BuildMidi2Page(); } MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to build the MIDI 2.0 values page.")
            try { BuildTimePage(); } MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to build the Windows timestamps page.")
            try { BuildHistoryPage(); } MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to build the History page.")

            native::History::Current().Changed([weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->RefreshHistory();
                    }
                });

            Closed([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->StopLiveClock();
                        native::History::Current().Changed(nullptr);
                        strong->m_chrome.SavePlacement();
                        strong->m_chrome.Shutdown();
                    }
                });

            // T, or the spacebar, taps the tempo from anywhere on the Tempo page, except while typing
            // in a box or with a button focused (the spacebar presses that button instead)
            RootGrid().KeyDown([weak = get_weak()](foundation::IInspectable const&, xaml::Input::KeyRoutedEventArgs const& args)
                {
                    auto strong = weak.get();
                    auto const key = args.Key();

                    if (!strong || strong->m_page != native::CalculatorPage::Tempo ||
                        (key != winrt::Windows::System::VirtualKey::T && key != winrt::Windows::System::VirtualKey::Space))
                    {
                        return;
                    }

                    auto const focused = xaml::Input::FocusManager::GetFocusedElement(strong->RootGrid().XamlRoot());

                    if (focused.try_as<controls::TextBox>() || focused.try_as<controls::NumberBox>() || focused.try_as<controls::ComboBox>() ||
                        (key == winrt::Windows::System::VirtualKey::Space && (focused.try_as<controls::Primitives::ButtonBase>() || focused.try_as<controls::NavigationViewItem>())))
                    {
                        return;
                    }

                    strong->Tap();
                    args.Handled(true);
                });

            m_initialized = true;

            auto const& options = App::StartupOptions();
            ShowPage(options.Page.value_or(native::AppSettings::Current().LastPage()));
            ApplyStartupOptions();
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to finish loading the window.")
    }

    void MainWindow::InitializeWindowChrome() noexcept
    {
        try
        {
            midiapp::ApplyPreviewBadgeVisibility(PreviewChiclet());
            midiapp::MakeLiveStatusRegion(StatusText());

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, native::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            Title(res::GetString(L"AppTitle"));

            // Lists and other pop-ups take on the context menus' tinted look, so they match the window.
            midiapp::UseMenuBackdropForLists();

            // 32px source for a 16px slot, so it stays crisp on a high DPI display
            AppTitleBarIcon().Source(midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32));

            AlwaysOnTopToggle().IsChecked(native::AppSettings::Current().AlwaysOnTop());
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to set up the window chrome.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowPage(native::CalculatorPage page) noexcept
    {
        try
        {
            m_page = page;

            TempoPage().Visibility(page == native::CalculatorPage::Tempo ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            PitchPage().Visibility(page == native::CalculatorPage::Pitch ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            Midi2Page().Visibility(page == native::CalculatorPage::Midi2 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            WindowsTimePage().Visibility(page == native::CalculatorPage::WindowsTime ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            HistoryPage().Visibility(page == native::CalculatorPage::History ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // Windows timestamps and History don't use the session values
            auto const usesSession = page == native::CalculatorPage::Tempo || page == native::CalculatorPage::Pitch || page == native::CalculatorPage::Midi2;
            SessionStrip().Visibility(usesSession ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            controls::NavigationViewItem item{ nullptr };
            winrt::hstring title{};

            switch (page)
            {
            case native::CalculatorPage::Pitch: item = NavPitch(); title = res::GetString(L"PagePitch"); break;
            case native::CalculatorPage::Midi2: item = NavMidi2(); title = res::GetString(L"PageMidi2"); break;
            case native::CalculatorPage::WindowsTime: item = NavWindowsTime(); title = res::GetString(L"PageWindowsTime"); break;
            case native::CalculatorPage::History: item = NavHistory(); title = res::GetString(L"PageHistory"); break;
            default: item = NavTempo(); title = res::GetString(L"PageTempo"); break;
            }

            PageTitleText().Text(title);

            if (Navigation().SelectedItem() != item)
            {
                Navigation().SelectedItem(item);
            }

            if (page == native::CalculatorPage::WindowsTime)
            {
                StartLiveClock();
            }
            else
            {
                StopLiveClock();
            }

            // shows the History tools beside the title, or hides them on the other pages
            RefreshHistory();

            native::AppSettings::Current().LastPage(page);
            PageScroller().ChangeView(nullptr, 0.0, nullptr, true);
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to show the page.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNavigationSelectionChanged(controls::NavigationView const&, controls::NavigationViewSelectionChangedEventArgs const& args)
    {
        try
        {
            if (!m_initialized)
            {
                return;
            }

            auto const item = args.SelectedItem().try_as<controls::NavigationViewItem>();

            if (!item)
            {
                return;
            }

            auto const tag = winrt::unbox_value_or<winrt::hstring>(item.Tag(), L"0");
            auto const index = std::wcstol(tag.c_str(), nullptr, 10);

            if (index >= 0 && index <= static_cast<long>(native::CalculatorPage::History) && static_cast<native::CalculatorPage>(index) != m_page)
            {
                ShowPage(static_cast<native::CalculatorPage>(index));
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to change pages.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPageHostSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const& args)
    {
        if (m_historyColumns)
        {
            try
            {
                m_historyColumns->Layout(args.NewSize().Width);
            }
            CATCH_LOG()
        }

        for (auto const& columns : m_columns)
        {
            try
            {
                columns->Layout(args.NewSize().Width);
            }
            CATCH_LOG()
        }
    }

    _Use_decl_annotations_
    void MainWindow::CopyAnswer(winrt::hstring const& page, winrt::hstring const& label, winrt::hstring const& value) noexcept
    {
        try
        {
            ::midicalculator::ui::CopyText(value);
            native::History::Current().Add(std::wstring{ page }, std::wstring{ label }, std::wstring{ value });
            ShowStatus(res::FormatString(L"StatusCopied", std::wstring_view{ value }));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to copy the answer.")
    }

    _Use_decl_annotations_
    ::midicalculator::ui::CopyHandler MainWindow::CopyFor(winrt::hstring const& page) noexcept
    {
        return [weak = get_weak(), page](winrt::hstring const& label, winrt::hstring const& value)
            {
                if (auto strong = weak.get())
                {
                    strong->CopyAnswer(page, label, value);
                }
            };
    }

    _Use_decl_annotations_
    void MainWindow::ShowStatus(winrt::hstring const& text) noexcept
    {
        try
        {
            StatusText().Text(text);

            if (!m_statusTimer)
            {
                m_statusTimer = m_dispatcherQueue.CreateTimer();
                m_statusTimer.Interval(std::chrono::seconds{ 4 });
                m_statusTimer.IsRepeating(false);
                m_statusTimer.Tick([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->StatusText().Text(L"");
                        }
                    });
            }

            m_statusTimer.Stop();
            m_statusTimer.Start();
        }
        CATCH_LOG()
    }

    _Use_decl_annotations_
    void MainWindow::OnRootSizeChanged(foundation::IInspectable const&, xaml::SizeChangedEventArgs const&)
    {
        m_chrome.UpdateTitleBarInsets();
    }

    _Use_decl_annotations_
    void MainWindow::OnAlwaysOnTopChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // Checked fires during InitializeComponent, before the chrome exists
            if (!m_initialized)
            {
                return;
            }

            auto const checked = AlwaysOnTopToggle().IsChecked();
            native::AppSettings::Current().AlwaysOnTop(checked != nullptr && checked.Value());
            m_chrome.ApplyAlwaysOnTop();
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to change the always on top setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSettingsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            midiapp::AppearanceStrings strings{};

            strings.Title = res::GetString(L"SettingsTitle");
            strings.ThemeLabel = res::GetString(L"SettingsThemeLabel");
            strings.ThemeSystem = res::GetString(L"SettingsThemeSystem");
            strings.ThemeLight = res::GetString(L"SettingsThemeLight");
            strings.ThemeDark = res::GetString(L"SettingsThemeDark");
            strings.BackdropLabel = res::GetString(L"SettingsBackdropLabel");
            strings.BackdropSolid = res::GetString(L"SettingsBackdropSolid");
            strings.BackdropMica = res::GetString(L"SettingsBackdropMica");
            strings.BackdropAcrylic = res::GetString(L"SettingsBackdropAcrylic");
            strings.CustomColorCheckBox = res::GetString(L"SettingsCustomColorCheckBox");
            strings.ColorPickerName = res::GetString(L"SettingsColorPickerName");

            // the calculator's own settings, under the appearance ones
            controls::StackPanel extra{};
            extra.Spacing(12);

            auto const& settings = native::AppSettings::Current();

            std::vector<winrt::hstring> octaves{};

            for (uint8_t octave = 1; octave <= 7; ++octave)
            {
                octaves.push_back(res::FormatString(L"SettingsMiddleCOption", octave));
            }

            auto middleC = ::midicalculator::ui::MakeComboBox(res::GetString(L"SettingsMiddleCLabel"), octaves, settings.MiddleCOctave() - 1, 220);
            middleC.SelectionChanged([weak = get_weak(), middleC](auto&&, auto&&)
                {
                    if (auto strong = weak.get(); strong && middleC.SelectedIndex() >= 0)
                    {
                        native::AppSettings::Current().MiddleCOctave(static_cast<uint8_t>(middleC.SelectedIndex() + 1));
                        strong->SessionChanged();
                    }
                });

            auto ticks = ::midicalculator::ui::MakeNumberBox(res::GetString(L"SettingsTicksLabel"), settings.TicksPerQuarterNote(), 1, 65535, 220);
            ticks.ValueChanged([weak = get_weak()](controls::NumberBox const& sender, auto&&)
                {
                    if (auto strong = weak.get(); strong && !std::isnan(sender.Value()))
                    {
                        native::AppSettings::Current().TicksPerQuarterNote(static_cast<uint32_t>(sender.Value()));
                        strong->SessionChanged();
                    }
                });

            extra.Children().Append(middleC);
            extra.Children().Append(::midicalculator::ui::MakeHelp(res::GetString(L"SettingsMiddleCHelp")));
            extra.Children().Append(ticks);

            midiapp::ShowAppearanceFlyout(
                SettingsButton(),
                native::AppSettings::Current(),
                strings,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_chrome.ApplyTheme();
                    }
                },
                extra,
                midiapp::MakeUmpPrimerLink(res::GetString(L"UmpPrimerLink")));
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to open the settings.")
    }

    void MainWindow::ApplyStartupOptions() noexcept
    {
        try
        {
            auto const& options = App::StartupOptions();

            if (options.Value.empty())
            {
                return;
            }

            winrt::hstring const value{ options.Value };

            switch (m_page)
            {
            case native::CalculatorPage::Tempo:
                if (auto const tempo = std::wcstod(options.Value.c_str(), nullptr); tempo > 0.0)
                {
                    SessionTempoBox().Value(tempo);
                }
                break;
            case native::CalculatorPage::Pitch:
                m_pitchNote.Text(value);
                break;
            case native::CalculatorPage::Midi2:
                m_umpWords.Text(value);
                break;
            case native::CalculatorPage::WindowsTime:
                m_timestamp.Text(value);
                break;
            default:
                break;
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to apply the command line.")
    }
}
