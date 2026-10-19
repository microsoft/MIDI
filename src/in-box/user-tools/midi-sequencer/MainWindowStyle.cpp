// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "ThemeLibrary.h"
#include "resource.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // The themes the app carries (Themes\*.json), in the order Appearance lists them.
        constexpr uint16_t BuiltInStyles[]{ IDR_THEME_NIGHT_DRIVE, IDR_THEME_TACTILE, IDR_THEME_ALUMINUM, IDR_THEME_VELLUM };

        // A built-in theme's name from the app's resources, so it can be translated, or the
        // theme's own name for one the app doesn't know.
        winrt::hstring StyleName(themeengine::ThemeDefinition const& theme)
        {
            static constexpr std::pair<std::wstring_view, std::wstring_view> names[]{
                { L"night-drive", L"StyleNightDrive" },
                { L"tactile", L"StyleTactile" },
                { L"aluminum", L"StyleAluminum" },
                { L"vellum", L"StyleVellum" },
            };

            for (auto const& [id, key] : names)
            {
                if (theme.Id == id)
                {
                    return res::GetString(key);
                }
            }

            return winrt::hstring{ theme.Name };
        }

        std::wstring_view BackdropName(midiapp::WindowBackdrop backdrop) noexcept
        {
            switch (backdrop)
            {
            case midiapp::WindowBackdrop::Mica: return L"mica";
            case midiapp::WindowBackdrop::Acrylic: return L"acrylic";
            default: return L"solid";
            }
        }
    }

    void MainWindow::LoadStyles() noexcept
    {
        try
        {
            m_styles = themeengine::LoadThemesFromResources(BuiltInStyles, [](uint16_t id, std::wstring const& problem)
            {
                MIDI_SEQUENCER_LOG_INFO(std::format(L"Theme resource {}: {}", id, problem).c_str());
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to load the styles.")
    }

    void MainWindow::ApplyStyle() noexcept
    {
        try
        {
            auto const& settings = seq::AppSettings::Current();
            m_style = themeengine::FindTheme(m_styles, settings.StyleId());

            if (m_style == nullptr)
            {
                // No theme could be read: the palette the app was built with.
                m_styleLight = nullptr;
                m_styleDark = nullptr;
                ApplyPalette();
                return;
            }

            std::optional<themeengine::Color> tint{};
            m_styleTint.reset();

            if (settings.UseCustomBackgroundColor())
            {
                m_styleTint = settings.BackgroundColorArgb() | 0xFF000000u;
                tint = themeengine::Color::FromArgb(*m_styleTint);
            }

            m_styleLight = themeengine::ResolvedTheme::Resolve(m_style, false, tint);
            m_styleDark = themeengine::ResolvedTheme::Resolve(m_style, true, tint);

            if (m_styleResources.Apply(RootGrid().Resources(), *m_styleLight, *m_styleDark))
            {
                // A brush the style added or took away is only picked up when the theme changes,
                // so it changes to the other one and back.
                auto const root = RootGrid();
                auto const requested = root.RequestedTheme();
                root.RequestedTheme(root.ActualTheme() == xaml::ElementTheme::Dark ? xaml::ElementTheme::Light : xaml::ElementTheme::Dark);
                root.RequestedTheme(requested);
            }

            ApplyPalette();
            ApplyStrips();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to apply the style.")
    }

    void MainWindow::RefreshStyle() noexcept
    {
        try
        {
            ApplyStyle();
            RebuildHeaders();
            RebuildSceneHeaders();
            InvalidateArrange();
            InvalidateLaunchers();
            InvalidateEditor();

            // The strips take their colors from the theme when they're built.
            RebuildMixer();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the style.")
    }

    // A style whose title bar, transport and status bar are a dark strip in light too (Aluminum)
    // gives them the dark look, and the caption buttons light glyphs to match. After the window's
    // own theme work, which sets the caption buttons for the theme.
    void MainWindow::ApplyStrips() noexcept
    {
        try
        {
            auto const darkStrips = m_styleLight != nullptr && m_styleLight->Metric(L"darkStrips", 0.0) > 0.0;
            auto const requested = darkStrips ? xaml::ElementTheme::Dark : xaml::ElementTheme::Default;

            for (xaml::FrameworkElement const& strip : { AppTitleBar().as<xaml::FrameworkElement>(), TransportBar().as<xaml::FrameworkElement>(), StatusBar().as<xaml::FrameworkElement>() })
            {
                if (strip.RequestedTheme() != requested)
                {
                    strip.RequestedTheme(requested);
                }
            }

            auto const light = RootGrid().ActualTheme() == xaml::ElementTheme::Light;
            m_stripPalette = darkStrips && light && m_styleDark != nullptr ? seq::MakePalette(m_styleDark) : m_palette;
            ColorMetronome();

            DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [weak = get_weak(), darkStrips]()
            {
                try
                {
                    auto strong = weak.get();

                    if (!strong)
                    {
                        return;
                    }

                    if (!darkStrips)
                    {
                        strong->m_chrome.ApplyTitleBarColors();
                        return;
                    }

                    auto const titleBar = strong->AppWindow().TitleBar();
                    auto const white = winrt::Windows::UI::Colors::White();

                    titleBar.ButtonBackgroundColor(winrt::Windows::UI::Colors::Transparent());
                    titleBar.ButtonInactiveBackgroundColor(winrt::Windows::UI::Colors::Transparent());
                    titleBar.ButtonForegroundColor(white);
                    titleBar.ButtonHoverForegroundColor(white);
                    titleBar.ButtonPressedForegroundColor(white);
                    titleBar.ButtonHoverBackgroundColor(seq::Rgba(255, 255, 255, 0.1));
                    titleBar.ButtonPressedBackgroundColor(seq::Rgba(255, 255, 255, 0.16));
                    titleBar.ButtonInactiveForegroundColor(winrt::Windows::UI::Colors::Gray());
                }
                catch (...)
                {
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to apply the style's strips.")
    }

    // The metronome's own drawing, lit in the accent while it's on, in the transport's colors.
    void MainWindow::ColorMetronome() noexcept
    {
        try
        {
            auto const on = seq::AppSettings::Current().MetronomeEnabled();
            // Before any style is applied, the window's own.
            auto const& colors = m_stripPalette.Text.A > 0 ? m_stripPalette : m_palette;
            MetronomeIconPath().Stroke(BrushFor(on ? colors.Accent : colors.Text2));
            MetronomeIconWeight().Fill(BrushFor(on ? colors.Accent : colors.Text2));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to color the metronome.")
    }

    xaml::UIElement MainWindow::MakeStylePicker()
    {
        controls::StackPanel panel{};
        panel.Spacing(8);

        auto const& settings = seq::AppSettings::Current();
        auto const dark = RootGrid().ActualTheme() == xaml::ElementTheme::Dark;

        std::optional<themeengine::Color> tint{};

        if (settings.UseCustomBackgroundColor())
        {
            tint = themeengine::Color::FromArgb(settings.BackgroundColorArgb() | 0xFF000000u);
        }

        // Each style as a tile: a small window in its own colors, light or dark as the app is now,
        // in the window color, and its name.
        controls::RadioButtons picker{};
        picker.Header(winrt::box_value(res::GetString(L"AppearanceStyle")));
        picker.MaxColumns(4);

        int32_t selected{ 0 };
        auto const& current = settings.StyleId();

        for (size_t i = 0; i < m_styles.size(); ++i)
        {
            auto const& style = m_styles[i];

            controls::RadioButton choice{};
            choice.Content(MakeStylePreview(*style, dark, tint));
            choice.Padding(xaml::Thickness{ 6, 0, 0, 0 });
            choice.MinWidth(0);
            automation::AutomationProperties::SetName(choice, StyleName(*style));
            controls::ToolTipService::SetToolTip(choice, winrt::box_value(winrt::hstring{ style->Description }));
            picker.Items().Append(choice);

            if (style->Id == current)
            {
                selected = static_cast<int32_t>(i);
            }
        }

        picker.SelectedIndex(m_styles.empty() ? -1 : selected);
        panel.Children().Append(picker);

        // A style that's a solid material sits on no backdrop, so the backdrop setting below is
        // for Night drive; this says so while one is picked.
        controls::TextBlock note{};
        note.TextWrapping(xaml::TextWrapping::Wrap);
        note.MaxWidth(520.0);
        panel.Children().Append(note);

        auto const updateNote = [weak = get_weak(), note]()
        {
            if (auto strong = weak.get(); strong && strong->m_style != nullptr)
            {
                auto const backdrop = BackdropName(seq::AppSettings::Current().Backdrop());
                auto const solidOnly = !strong->m_style->AllowsBackdrop(backdrop) || strong->m_style->WindowColor == themeengine::WindowColorUse::Material;

                note.Text(res::FormatString(L"AppearanceStyleSolidNote", std::wstring{ StyleName(*strong->m_style) }));
                note.Visibility(solidOnly ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            }
        };

        updateNote();

        picker.SelectionChanged([weak = get_weak(), updateNote](foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const&)
        {
            try
            {
                auto strong = weak.get();

                if (!strong)
                {
                    return;
                }

                auto const index = sender.as<controls::RadioButtons>().SelectedIndex();

                if (index < 0 || static_cast<size_t>(index) >= strong->m_styles.size())
                {
                    return;
                }

                if (strong->m_styles[static_cast<size_t>(index)]->Id == seq::AppSettings::Current().StyleId())
                {
                    return;
                }

                seq::AppSettings::Current().StyleId(strong->m_styles[static_cast<size_t>(index)]->Id);
                strong->RefreshStyle();
                updateNote();
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the style.")
        });

        // The timeline's scroll bar: which kind, and where (MainWindowTimelineBar.cpp).
        panel.Children().Append(MakeTimelineBarSettings());

        return panel;
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeStylePreview(themeengine::ThemeDefinition const& definition, bool dark, std::optional<themeengine::Color> tint)
    {
        controls::StackPanel tile{};
        tile.Spacing(4);

        controls::Border frame{};
        frame.Width(132);
        frame.Height(80);
        frame.CornerRadius(xaml::CornerRadius{ 6, 6, 6, 6 });
        frame.BorderThickness(xaml::Thickness{ 1, 1, 1, 1 });
        frame.BorderBrush(media::SolidColorBrush{ seq::Rgba(128, 128, 128, 0.35) });

        try
        {
            auto const shared = std::make_shared<themeengine::ThemeDefinition const>(definition);
            auto const theme = themeengine::ResolvedTheme::Resolve(shared, dark, tint);
            auto const palette = seq::MakePalette(theme);

            auto const token = [&theme](std::wstring_view name) -> std::optional<winrt::Windows::UI::Color>
            {
                if (theme->HasToken(name))
                {
                    return seq::ToUiColor(theme->Token(name));
                }

                return std::nullopt;
            };

            // A style on the backdrop shows the window color, or the default window's.
            auto const windowFill = definition.WindowColor == themeengine::WindowColorUse::Backdrop && tint.has_value()
                ? seq::ToUiColor(*tint)
                : (dark ? seq::FromRgb(0x202020) : seq::FromRgb(0xF3F3F3));

            auto const chrome = token(L"previewChrome").value_or(windowFill);
            auto const strip = token(L"previewStrip").value_or(chrome);
            auto const field = token(L"previewField").value_or(seq::Mix(palette.LaneBackground, palette.LaneBackground.A / 255.0, chrome));

            controls::Grid window{};
            window.Background(media::SolidColorBrush{ chrome });

            controls::RowDefinition top{};
            top.Height(xaml::GridLength{ 14, xaml::GridUnitType::Pixel });
            window.RowDefinitions().Append(top);
            window.RowDefinitions().Append(controls::RowDefinition{});

            controls::ColumnDefinition headers{};
            headers.Width(xaml::GridLength{ 30, xaml::GridUnitType::Pixel });
            window.ColumnDefinitions().Append(headers);
            window.ColumnDefinitions().Append(controls::ColumnDefinition{});

            // The transport, with a key lit in the style's accent.
            controls::Grid transport{};
            transport.Background(media::SolidColorBrush{ strip });
            controls::Grid::SetColumnSpan(transport, 2);

            shapes::Ellipse key{};
            key.Width(6);
            key.Height(6);
            key.Margin(xaml::Thickness{ 6, 0, 0, 0 });
            key.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            key.Fill(media::SolidColorBrush{ palette.Accent });
            transport.Children().Append(key);
            window.Children().Append(transport);

            // The timeline: three tracks' clips in the style's track colors.
            controls::Canvas lanes{};
            lanes.Margin(xaml::Thickness{ 2, 4, 4, 4 });
            lanes.Background(media::SolidColorBrush{ field });
            controls::Grid::SetRow(lanes, 1);
            controls::Grid::SetColumn(lanes, 1);

            static constexpr uint32_t tracks[]{ 0xFF8A65, 0x5BC0EB, 0x6CCB5F };
            static constexpr double starts[]{ 6, 30, 14 };
            static constexpr double widths[]{ 48, 52, 60 };

            for (size_t i = 0; i < std::size(tracks); ++i)
            {
                auto const color = seq::ThemedTrackColor(palette, seq::FromRgb(tracks[i]));
                auto const body = token(L"previewClip").value_or(seq::ClipFill(palette, color));

                controls::Border clip{};
                clip.Width(widths[i]);
                clip.Height(13);
                clip.CornerRadius(xaml::CornerRadius{ 2, 2, 2, 2 });
                clip.Background(media::SolidColorBrush{ body });
                clip.BorderThickness(xaml::Thickness{ 3, 0, 0, 0 });
                clip.BorderBrush(media::SolidColorBrush{ color });
                controls::Canvas::SetLeft(clip, starts[i]);
                controls::Canvas::SetTop(clip, 4.0 + static_cast<double>(i) * 18.0);
                lanes.Children().Append(clip);
            }

            window.Children().Append(lanes);
            frame.Child(window);
        }
        catch (...)
        {
        }

        tile.Children().Append(frame);

        controls::TextBlock name{};
        name.Text(StyleName(definition));
        name.FontSize(12);
        tile.Children().Append(name);

        return tile;
    }
}
