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
#include "SharingDialogs.h"
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
            m_isSigned = !::midiglass::sharing::SignerOf(filePath).empty();

            m_toolbar = m_document.ToolbarWindow;
            m_seeThrough = m_document.SeeThrough;
            m_keepOnTop = m_document.AlwaysOnTop;

            m_theme = glass::ResolveDocumentTheme(m_document);

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
                    // The title bar becomes part of the window's content when the chrome is set
                    // up, so the window is measured that way from the start.
                    ExtendsContentIntoTitleBar(true);

                    // A toolbar, or a page smaller than the usual window, opens at its own size.
                    if (!FitWindowToPage())
                    {
                        appWindow.Resize({ DefaultWindowWidth, DefaultWindowHeight });
                    }
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

            // After the shared chrome, which would otherwise pin this window whenever the
            // library window is pinned.
            ApplyWindowStyle();
            ApplySeeThrough();

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

            m_listedPages = glass::PagesToChooseFrom(m_document);

            if (m_listedPages.size() > 1)
            {
                for (auto const index : m_listedPages)
                {
                    auto const& page = m_document.Pages[index];

                    PageSelector().Items().Append(box_value(winrt::hstring{
                        page.Name.empty() ? page.Id : page.Name }));
                }

                PageSelector().SelectedIndex(0);
                PageSelector().Visibility(xaml::Visibility::Visible);
            }

            m_updatingChrome = false;

            for (auto const& page : m_document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    m_controlValues.push_back(control.DefaultValue);
                    m_controlValuesY.push_back(control.DefaultValueY);
                }
            }

            UpdateDeckBrush();

            if (m_document.Pages.empty())
            {
                LoadProblemText().Text(resources::GetString(L"RuntimeNoPages"));
                LoadProblemText().Visibility(xaml::Visibility::Visible);
                SurfaceScroll().Visibility(xaml::Visibility::Collapsed);
            }
            else
            {
                BuildPage(m_listedPages.empty() ? 0 : m_listedPages.front());
            }

            StartDevices();

            // Only a layout that is actually running. The designer and Try mode do not hold the
            // machine awake, because somebody building a layout is at the keyboard anyway.
            if (::midiglass::AppSettings::Current().KeepAwakeWhileRunning())
            {
                HoldDisplayAwake(true);
            }

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
            // Whatever else happens, the machine gets its power behavior back.
            HoldDisplayAwake(false);

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
            // Nothing behind the controls at all, so whatever is behind the window shows.
            if (m_seeThrough)
            {
                SurfaceDeck().Background(nullptr);
                SurfaceScroll().Background(media::SolidColorBrush(winrt::Windows::UI::Colors::Transparent()));

                ClearDeckOverlays();
                return;
            }

            SurfaceDeck().Background(glass::MakeDeckBrush(m_theme.Deck));

            // Everything outside the page is deliberately not the deck, so the page reads as the
            // object and the surround reads as nothing.
            auto const surround = glass::BlendOver(m_theme.Deck.Color, { 0, 0, 0, 255 }, 0.45);

            SurfaceScroll().Background(media::SolidColorBrush(ToColor(surround)));

            // The border carries the page at its scaled size, so its width over the page's is
            // the scale a repeating picture and the rain are drawn at.
            auto const pageScale = m_document.PageWidth > 0
                ? SurfaceDeck().Width() / static_cast<double>(m_document.PageWidth)
                : 1.0;

            glass::ApplyDeckOverlay(
                SurfaceGrain(), m_theme, SurfaceDeck().Width(), SurfaceDeck().Height(), 1.0,
                glass::DeckOverlayLayer::BeneathControls, std::isnan(pageScale) ? 1.0 : pageScale, true);

            glass::ApplyDeckOverlay(
                SurfaceDeck(), m_theme, SurfaceDeck().Width(), SurfaceDeck().Height(), 1.0,
                glass::DeckOverlayLayer::AboveControls);
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

        m_renderer.Build(SurfaceCanvas(), m_document, m_theme, pageIndex, true);
        RestoreValues();
        m_renderer.ShowCurrentPage(m_document, pageIndex);

        auto weak = get_weak();

        m_renderer.DescribeValue = [weak](
            uint32_t controlIndex,
            glass::ValueAxis axis,
            double value) -> std::wstring
            {
                auto strong = weak.get();

                return strong != nullptr && strong->m_player != nullptr
                    ? strong->m_player->DescribeValue(controlIndex, axis, value)
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

        m_input.PadTouched = [weak](size_t itemIndex, glass::PadTouch const& touch)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlPadTouched(itemIndex, touch);
                }
            };

        m_input.Switched = [weak](size_t itemIndex, bool isOn, double velocity)
            {
                if (auto strong = weak.get())
                {
                    strong->OnControlSwitched(itemIndex, isOn, velocity);
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
                        // Assistive technology cannot press harder, so it always hits at full.
                        strong->OnControlSwitched(i, true, 1.0);
                        strong->OnControlSwitched(i, false, 0.0);
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

        auto const listed = std::find(m_listedPages.begin(), m_listedPages.end(), pageIndex);

        PageSelector().SelectedIndex(listed == m_listedPages.end()
            ? -1
            : static_cast<int32_t>(listed - m_listedPages.begin()));

        m_updatingChrome = previous;
    }

    _Use_decl_annotations_
    void RuntimeWindow::RememberValue(uint32_t controlIndex, double value) noexcept
    {
        if (controlIndex < m_controlValues.size())
        {
            m_controlValues[controlIndex] = value;
        }
    }

    _Use_decl_annotations_
    void RuntimeWindow::RememberValueY(uint32_t controlIndex, double value) noexcept
    {
        if (controlIndex < m_controlValuesY.size())
        {
            m_controlValuesY[controlIndex] = value;
        }
    }

    void RuntimeWindow::RestoreValues() noexcept
    {
        try
        {
            for (size_t item = 0; item < m_renderer.ItemCount(); ++item)
            {
                auto const controlIndex = m_renderer.ControlIndexOf(item);

                // A page tab says which page is showing, and the page being built decides that.
                if (controlIndex >= m_controlValues.size() ||
                    m_renderer.KindAt(item) == glass::ControlKind::PageTab)
                {
                    continue;
                }

                m_renderer.SetValue(item, m_controlValues[controlIndex]);
                m_renderer.SetValueY(item, m_controlValuesY[controlIndex]);

                if (auto const element = m_renderer.ElementAt(item))
                {
                    winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(m_controlValues[controlIndex]);
                }
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to put the controls back where they were.")
    }

    void RuntimeWindow::ApplyScale()
    {
        try
        {
            // A toolbar's window is its page, so the page fills it, whatever size the window
            // came out at on this display.
            auto const mode = m_toolbar && !m_fullScreen ? glass::ScaleMode::FitToScreen : m_document.Scale;

            auto const viewport = glass::ComputeViewport(
                m_document.PageWidth,
                m_document.PageHeight,
                SurfaceScroll().ActualWidth(),
                SurfaceScroll().ActualHeight(),
                mode,
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

            if (m_seeThrough)
            {
                return;
            }

            // The scan lines belong to the border rather than to the scaled canvas, so their
            // pitch is screen pixels at every zoom instead of a beat pattern at most of them.
            glass::ApplyDeckOverlay(
                SurfaceGrain(), m_theme, viewport.ContentWidth, viewport.ContentHeight, 1.0,
                glass::DeckOverlayLayer::BeneathControls, viewport.Scale, true);

            glass::ApplyDeckOverlay(
                SurfaceDeck(), m_theme, viewport.ContentWidth, viewport.ContentHeight, 1.0,
                glass::DeckOverlayLayer::AboveControls);
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

        if (index >= 0 && static_cast<size_t>(index) < m_listedPages.size() &&
            m_listedPages[static_cast<size_t>(index)] != m_pageIndex)
        {
            try
            {
                BuildPage(m_listedPages[static_cast<size_t>(index)]);
            }
            MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the page.")
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
        RememberScale();
    }

    void RuntimeWindow::RememberScale()
    {
        // The last used mode is remembered per layout, which is the whole reason it is on the
        // document rather than in app settings. A signed layout stays exactly as it was signed,
        // so it keeps the mode for this run only.
        try
        {
            if (!m_filePath.empty() && !m_isSigned)
            {
                glass::WriteLayoutFile(m_document, m_filePath);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to remember the scale mode.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnKeepOnTopChecked(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome || !m_loaded)
        {
            return;
        }

        ApplyKeepOnTop(true);
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnKeepOnTopUnchecked(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome || !m_loaded)
        {
            return;
        }

        ApplyKeepOnTop(false);
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

            // Leaving full screen brings back an ordinary window, so the layout's own style, and a
            // toolbar's handle, go back on it.
            ApplyWindowStyle();

            ShowFullScreenChrome();
            HoldDisplayAwake(m_fullScreen || ::midiglass::AppSettings::Current().KeepAwakeWhileRunning());
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

    // Panic is pressed when something has already gone wrong, so neither half may take the window
    // down, and a failure releasing the held controls must not stop the panic itself going out.
    void RuntimeWindow::Panic()
    {
        try
        {
            m_input.ReleaseAll();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to release the held controls.")

        try
        {
            glass::LivePlayer::Panic();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to send panic.")
    }
}
