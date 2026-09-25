// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RuntimeWindow.xaml.h"
#include "RuntimeWindow.g.cpp"

#include "AppSettings.h"
#include "StringResources.h"
#include "LayoutStore.h"
#include "ThemeStore.h"
#include "SurfaceScale.h"
#include "GlassControl.h"
#include "SingleInstance.h"
#include "resource.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr int32_t DefaultWindowWidth = 1320;
        constexpr int32_t DefaultWindowHeight = 900;

        winrt::Windows::UI::Color ToColor(_In_ glass::ThemeColor const& color) noexcept
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(255, color.R, color.G, color.B);
        }
    }

    _Use_decl_annotations_
    bool RuntimeWindow::LoadLayout(std::wstring const& filePath)
    {
        try
        {
            auto const read = glass::ReadLayoutFile(filePath);

            if (!read.Succeeded)
            {
                return false;
            }

            m_filePath = filePath;
            m_document = read.Document;

            auto const themes = glass::AllThemes();

            m_theme = themes.empty() ? glass::Theme{} : themes[0];

            for (auto const& theme : themes)
            {
                if (theme.Name == m_document.ThemeName)
                {
                    m_theme = theme;
                    break;
                }
            }

            // One owner per running layout, and the file path is what makes it unique, so the
            // same layout opened twice shares its connections instead of doubling them.
            m_ownerId = filePath;

            // A runtime window does not use the library window's saved placement, and does not
            // write one back. Where a layout opens is a property of the layout and arrives with
            // the display work; sharing one setting between the two would mean opening the
            // library moved every surface.
            try
            {
                if (auto const appWindow = AppWindow())
                {
                    appWindow.Resize({ DefaultWindowWidth, DefaultWindowHeight });
                }
            }
            catch (...)
            {
            }

            return true;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to load the layout.")

        return false;
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnRootLoaded(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            m_dispatcher = DispatcherQueue();

            midiapp::WindowChromeElements elements{};

            elements.Window = *this;
            elements.Root = RootGrid();
            elements.Fill = WindowFill();
            elements.Tint = WindowTint();
            elements.TitleBar = AppTitleBar();
            elements.LeftInset = TitleBarLeftInsetColumn();
            elements.RightInset = TitleBarRightInsetColumn();

            m_chrome.Initialize(elements, ::midiglass::AppSettings::Current());
            m_chrome.SetWindowIconFromResource(IDI_APPICON);

            auto const title = m_document.Name.empty()
                ? resources::GetString(L"AppDisplayName")
                : resources::FormatString(L"RuntimeWindowTitleFormat", m_document.Name);

            Title(title);
            AppTitleTextBlock().Text(title);

            if (auto const icon = midiapp::WindowChrome::LoadIconImageSource(IDI_APPICON, 32))
            {
                AppTitleBarIcon().Source(icon);
            }

            // Windows says reduce motion, so the bloom switches instead of fading. The surface is
            // a wall of animation by design, which is exactly why this is not optional.
            try
            {
                winrt::Windows::UI::ViewManagement::UISettings settings{};
                m_renderer.SetReducedMotion(!settings.AnimationsEnabled());
            }
            catch (...)
            {
            }

            m_updatingChrome = true;

            ScaleSelector().Items().Append(box_value(resources::GetString(L"ScaleActualSize")));
            ScaleSelector().Items().Append(box_value(resources::GetString(L"ScaleFitToScreen")));
            ScaleSelector().Items().Append(box_value(resources::GetString(L"ScaleCustom")));
            ScaleSelector().SelectedIndex(static_cast<int32_t>(m_document.Scale));

            if (m_document.Pages.size() > 1)
            {
                for (auto const& page : m_document.Pages)
                {
                    PageSelector().Items().Append(box_value(winrt::hstring{
                        page.Name.empty() ? page.Id : page.Name }));
                }

                PageSelector().SelectedIndex(0);
                PageSelector().Visibility(xaml::Visibility::Visible);
            }

            m_updatingChrome = false;

            UpdateDeckBrush();

            if (m_document.Pages.empty())
            {
                LoadProblemText().Text(resources::GetString(L"RuntimeNoPages"));
                LoadProblemText().Visibility(xaml::Visibility::Visible);
                SurfaceScroll().Visibility(xaml::Visibility::Collapsed);
            }
            else
            {
                BuildPage(0);
            }

            StartDevices();

            Closed({ this, &RuntimeWindow::OnWindowClosed });

            m_loaded = true;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set up the runtime window.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnWindowClosed(foundation::IInspectable const& sender, xaml::WindowEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_closing = true;

        try
        {
            // Nothing is left held. A finger lifted by a window closing still has to end its note.
            m_input.ReleaseAll();
            m_input.Detach();

            if (m_player != nullptr)
            {
                // Blocking calls, so they leave the UI thread. The router outlives the window.
                m_player->Stop();
            }

            m_renderer.Teardown();

            m_chrome.Shutdown();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to shut the runtime window down cleanly.")
    }

    void RuntimeWindow::UpdateDeckBrush()
    {
        try
        {
            SurfaceDeck().Background(glass::MakeDeckBrush(m_theme.Deck));

            // Everything outside the page is deliberately not the deck, so the page reads as the
            // object and the surround reads as nothing.
            auto const surround = glass::BlendOver(m_theme.Deck.Color, { 0, 0, 0, 255 }, 0.45);

            SurfaceScroll().Background(media::SolidColorBrush(ToColor(surround)));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to color the deck.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::BuildPage(size_t pageIndex)
    {
        if (pageIndex >= m_document.Pages.size())
        {
            return;
        }

        m_input.ReleaseAll();
        m_input.Detach();

        m_pageIndex = pageIndex;

        SurfaceCanvas().Width(m_document.PageWidth);
        SurfaceCanvas().Height(m_document.PageHeight);

        m_renderer.Build(SurfaceCanvas(), m_document, m_theme, pageIndex);

        auto weak = get_weak();

        m_renderer.DescribeValue = [weak](uint32_t controlIndex, double value) -> std::wstring
            {
                auto strong = weak.get();

                return strong != nullptr && strong->m_player != nullptr
                    ? strong->m_player->DescribeValue(controlIndex, value)
                    : std::wstring{};
            };

        m_input.ValueChanged = [weak](size_t itemIndex, double value, bool isFinal)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlValueChanged(itemIndex, value, isFinal);
                }
            };

        m_input.ValueYChanged = [weak](size_t itemIndex, double value, bool isFinal)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlValueYChanged(itemIndex, value, isFinal);
                }
            };

        m_input.KeyChanged = [weak](size_t itemIndex, int32_t key, double velocity, bool isDown)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlKeyChanged(itemIndex, key, velocity, isDown);
                }
            };

        m_input.Switched = [weak](size_t itemIndex, bool isOn)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlSwitched(itemIndex, isOn);
                }
            };

        m_input.Snap = [weak](size_t itemIndex, double position) -> double
            {
                auto strong = weak.get();

                if (strong != nullptr && strong->m_player != nullptr)
                {
                    return strong->m_player->SnapToDetent(
                        strong->m_renderer.ControlIndexOf(itemIndex), position);
                }

                return position;
            };

        m_input.TouchChanged = [weak](size_t itemIndex, bool isTouched)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlTouched(itemIndex, isTouched);
                }
            };

        m_input.Attach(m_renderer);

        // Assistive technology drives the same path a finger does, so a screen reader moving a
        // fader sends exactly what a finger would.
        for (size_t i = 0; i < m_renderer.ItemCount(); ++i)
        {
            auto element = m_renderer.ElementAt(i);

            if (element == nullptr)
            {
                continue;
            }

            auto* implementation = winrt::get_self<implementation::GlassControl>(element);

            implementation->SetValueRequestHandler([weak, i](uint32_t, double value)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_renderer.SetValue(i, value);
                        strong->OnControlSetDirectly(i, value);
                    }
                });

            implementation->SetInvokeHandler([weak, i](uint32_t)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnControlSwitched(i, true);
                        strong->OnControlSwitched(i, false);
                    }
                });
        }

        ApplyScale();
    }

    _Use_decl_annotations_
    void RuntimeWindow::ShowPage(size_t pageIndex)
    {
        if (pageIndex >= m_document.Pages.size() || pageIndex == m_pageIndex)
        {
            return;
        }

        BuildPage(pageIndex);

        // The selector is chrome following the surface, not the other way around, so its own
        // handler must not turn round and build the page again.
        auto const previous = m_updatingChrome;
        m_updatingChrome = true;

        PageSelector().SelectedIndex(static_cast<int32_t>(pageIndex));

        m_updatingChrome = previous;
    }

    void RuntimeWindow::ApplyScale()
    {
        try
        {
            auto const viewport = glass::ComputeViewport(
                m_document.PageWidth,
                m_document.PageHeight,
                SurfaceScroll().ActualWidth(),
                SurfaceScroll().ActualHeight(),
                m_document.Scale,
                m_document.CustomScalePercent);

            if (viewport.Scale <= 0.0)
            {
                return;
            }

            SurfaceScale().ScaleX(viewport.Scale);
            SurfaceScale().ScaleY(viewport.Scale);

            // The transform paints the page scaled; the border carries the scaled size so the
            // scroll viewer knows how much there is and centers what fits.
            SurfaceDeck().Width(viewport.ContentWidth);
            SurfaceDeck().Height(viewport.ContentHeight);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to scale the surface.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnSurfaceSizeChanged(
        foundation::IInspectable const& sender,
        xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyScale();
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnPageSelectionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome || !m_loaded)
        {
            return;
        }

        auto const index = PageSelector().SelectedIndex();

        if (index >= 0 && static_cast<size_t>(index) != m_pageIndex)
        {
            BuildPage(static_cast<size_t>(index));
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnScaleSelectionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome || !m_loaded)
        {
            return;
        }

        auto const index = ScaleSelector().SelectedIndex();

        if (index < 0)
        {
            return;
        }

        m_document.Scale = static_cast<glass::ScaleMode>(index);

        ApplyScale();

        // The last used mode is remembered per layout, which is the whole reason it is on the
        // document rather than in app settings.
        try
        {
            if (!m_filePath.empty())
            {
                glass::WriteLayoutFile(m_document, m_filePath);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to remember the scale mode.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnViewModeToggled(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const on = ViewModeToggle().IsChecked().GetBoolean();

        // While View mode is on the surface sends nothing. A pinch on a control surface is
        // ambiguous, because two fingers might be two fingers on two faders, which is the entire
        // point of the product.
        m_input.ReleaseAll();
        m_input.SetViewMode(on);
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnFullScreenClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SetFullScreen(!m_fullScreen);
    }

    _Use_decl_annotations_
    void RuntimeWindow::SetFullScreen(bool fullScreen)
    {
        try
        {
            auto const appWindow = AppWindow();

            if (appWindow == nullptr || fullScreen == m_fullScreen)
            {
                return;
            }

            m_fullScreen = fullScreen;

            appWindow.SetPresenter(m_fullScreen
                ? winrt::Microsoft::UI::Windowing::AppWindowPresenterKind::FullScreen
                : winrt::Microsoft::UI::Windowing::AppWindowPresenterKind::Default);

            // The chrome is the only place Panic lives, so it cannot go away with the title bar.
            AppTitleBar().Visibility(m_fullScreen ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            ChromeBar().Visibility(m_fullScreen ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            ShowFullScreenChrome();
            HoldDisplayAwake(m_fullScreen);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to switch full screen.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnFullScreenAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);

        SetFullScreen(!m_fullScreen);
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnEscapeAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (!m_fullScreen)
        {
            return;
        }

        args.Handled(true);

        SetFullScreen(false);
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnPanicAccelerator(
        xaml::Input::KeyboardAccelerator const& sender,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        args.Handled(true);

        Panic();
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnPanicClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        Panic();
    }

    void RuntimeWindow::Panic()
    {
        m_input.ReleaseAll();

        glass::LivePlayer::Panic();
    }
}
