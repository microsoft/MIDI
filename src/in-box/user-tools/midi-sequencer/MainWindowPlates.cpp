// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// A style's plates behind the window's controls (ThemePlates.h): the transport's keys, readouts,
// buttons, fields and chips, the track list's toolbar, and the clip editor's header. A style
// without the parts (Night drive) leaves the controls as they are.

#include "pch.h"
#include "MainWindow.xaml.h"

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // A key or button that's on: "on" for keys and chips, "toggled" for buttons, as the
        // comps' CSS names them.
        void AddOnStates(xaml::FrameworkElement const& element, std::vector<std::wstring>& states)
        {
            if (auto const toggle = element.try_as<primitives::ToggleButton>())
            {
                if (auto const checked = toggle.IsChecked(); checked != nullptr && checked.Value())
                {
                    states.emplace_back(L"on");
                    states.emplace_back(L"toggled");
                }
            }
        }

        themeengine::PlateOptions OnStates(std::vector<std::wstring_view> extra = {}, bool sized = false)
        {
            themeengine::PlateOptions options{};
            options.Sized = sized;
            // Each extra state, and the same with "-on" while the key is on ("play-on").
            options.States = [extra = std::vector<std::wstring>{ extra.begin(), extra.end() }](xaml::FrameworkElement const& element, std::vector<std::wstring>& states)
            {
                auto const before = states.size();
                AddOnStates(element, states);
                auto const on = states.size() > before;
                states.insert(states.end(), extra.begin(), extra.end());

                if (on)
                {
                    for (auto const& state : extra)
                    {
                        states.push_back(state + L"-on");
                    }
                }
            };
            return options;
        }

        // A key's light: in the color its state names, where the part puts it (led-size, led-top,
        // led-right, led-bottom) or else where the theme's transport.led does. A light that's off
        // (led-glow 0) has no glow.
        void DrawKeyLight(
            canvas::CanvasDrawingSession const& ds,
            themeengine::ResolvedTheme const& theme,
            themeengine::ResolvedStyle const& style,
            themeengine::PaintRect const& rect)
        {
            auto const color = style.FindColor(L"led-color");

            if (!color.has_value())
            {
                return;
            }

            float size{};
            double top{};
            double right{};
            double bottom{};

            if (auto const own = style.Number(L"led-size", -1.0); own > 0.0)
            {
                size = static_cast<float>(own);
                top = style.Number(L"led-top", -1.0);
                right = style.Number(L"led-right", -1.0);
                bottom = style.Number(L"led-bottom", 4.0);
            }
            else
            {
                auto const led = theme.Part(L"transport.led");
                size = static_cast<float>(led->Width.value_or(5.0));
                top = led->Number(L"top", -1.0);
                right = led->Number(L"right", -1.0);
                bottom = led->Number(L"bottom", 4.0);
            }

            auto const cx = right >= 0 ? rect.X + rect.Width - static_cast<float>(right) - size / 2.0f : rect.X + rect.Width / 2.0f;
            auto const cy = top >= 0 ? rect.Y + static_cast<float>(top) + size / 2.0f : rect.Y + rect.Height - static_cast<float>(bottom) - size / 2.0f;
            auto const ui = seq::ToUiColor(*color);

            if (style.Number(L"led-glow", 1.0) > 0.0)
            {
                ds.FillCircle(cx, cy, size * 1.3f, seq::WithAlpha(ui, ui.A / 255.0 * 0.22));
                ds.FillCircle(cx, cy, size * 0.85f, seq::WithAlpha(ui, ui.A / 255.0 * 0.35));
            }

            ds.FillCircle(cx, cy, size / 2.0f, ui);
        }

        themeengine::PlateOptions LitKey(themeengine::PlateOptions options)
        {
            options.Decorate = [](canvas::CanvasDrawingSession const& ds, canvas::ICanvasResourceCreatorWithDpi const&, themeengine::ThemePainter&,
                themeengine::ResolvedTheme const& theme, themeengine::ResolvedStyle const& style, themeengine::PaintRect const& rect)
            {
                DrawKeyLight(ds, theme, style, rect);
            };
            return options;
        }

        void SpanAll(xaml::FrameworkElement const& surface, int32_t columns, xaml::Thickness const& margin)
        {
            controls::Grid::SetColumnSpan(surface, columns);
            surface.Margin(margin);
        }
    }

    void MainWindow::SetUpPlates() noexcept
    {
        try
        {
            m_defaultTransportHeight = TransportBar().Height();
            m_defaultTransportSpacing = TransportLeft().Spacing();

            // Each layer is the first thing in its panel, over the panel's own background; it
            // reaches into the panel's padding so shadows aren't cut off at the edges.
            m_transportPlates.Attach(TransportBar(), 0);
            SpanAll(m_transportPlates.Surface(), 3, xaml::Thickness{ -12, -6, -12, -6 });

            m_toolbarPlates.Attach(ArrangeToolbar(), 0);
            SpanAll(m_toolbarPlates.Surface(), 4, xaml::Thickness{ -10, 0, -8, 0 });

            m_editorPlates.Attach(EditorHeader(), 0);
            SpanAll(m_editorPlates.Surface(), 3, xaml::Thickness{ -12, 0, -12, 0 });

            // The track headers and scene headers: one layer over the ruler's and pinned rows'
            // fills and behind everything else in the arrange's first two columns. Their rows
            // are built again often, so their plates are added as they're built.
            uint32_t toolbar{ 0 };
            auto const arrange = ArrangeGrid().Children();
            m_headerPlates.Attach(ArrangeGrid(), arrange.IndexOf(ArrangeToolbar(), toolbar) ? toolbar : 0);
            controls::Grid::SetRow(m_headerPlates.Surface(), 1);
            controls::Grid::SetRowSpan(m_headerPlates.Surface(), 4);
            controls::Grid::SetColumnSpan(m_headerPlates.Surface(), 2);

            // The transport: keys, with a light when on.
            auto key = [this](xaml::FrameworkElement const& element, std::vector<std::wstring_view> extra)
            {
                m_transportPlates.Add(element, L"transport.key", LitKey(OnStates(std::move(extra), true)));
            };

            key(FileMenuButton(), {});
            key(GoToStartButton(), {});
            key(PlayButton(), { L"play" });
            key(StopButton(), {});
            key(RecordButton(), { L"record" });
            key(LoopToggle(), {});
            key(MetronomeToggle(), {});
            key(MetronomeMenuButton(), { L"split" });

            for (auto const& separator : { TransportSeparator1(), TransportSeparator2(), TransportSeparator3() })
            {
                m_transportPlates.Add(separator, L"transport.separator");
            }

            // The readouts: a well, and their digits.
            m_transportPlates.Add(PositionDisplay(), L"transport.lcd");
            m_transportPlates.Add(TempoDisplay(), L"transport.lcd");
            m_transportPlates.Add(PositionText(), L"transport.digits");
            m_transportPlates.Add(TempoText(), L"transport.digits");
            m_transportPlates.Add(TimeText(), L"transport.small");
            m_transportPlates.Add(MeterText(), L"transport.small");

            // Text a style sets in its own face, with no plate: the window's title and the clip
            // the editor shows.
            m_transportPlates.Add(AppTitleTextBlock(), L"title.text");
            m_editorPlates.Add(EditorClipName(), L"editor.name");

            for (auto const& button : { ClockButton(), CaptureButton(), BackToTimelineButton() })
            {
                m_transportPlates.Add(button, L"button", OnStates());
            }

            // Panic, tinted red in each style's own way (its part's "panic" state).
            m_transportPlates.Add(SilenceButton(), L"button", OnStates({ L"panic" }));

            m_transportPlates.Add(MixerToggle(), L"button", OnStates());
            m_transportPlates.Add(UndoButton(), L"button.subtle", OnStates());
            m_transportPlates.Add(RedoButton(), L"button.subtle", OnStates());
            m_transportPlates.Add(LaunchCombo(), L"field");
            m_transportPlates.Add(SnapCombo(), L"field");

            // The saved chip is on while the sequence is saved.
            themeengine::PlateOptions saved{};
            saved.States = [weak = get_weak()](xaml::FrameworkElement const&, std::vector<std::wstring>& states)
            {
                if (auto strong = weak.get(); strong && strong->SavedChipIcon().Glyph() == L"\uE73E")
                {
                    states.emplace_back(L"on");
                }
            };
            m_transportPlates.Add(SavedChip(), L"chip", std::move(saved));

            // The track list's toolbar.
            m_toolbarPlates.Add(AddTrackButton(), L"button", OnStates());
            m_toolbarPlates.Add(AddFolderButton(), L"button.subtle", OnStates());
            m_toolbarPlates.Add(LauncherToggle(), L"button", OnStates());
            m_toolbarPlates.Add(RazorToggle(), L"button", OnStates());

            // The clip editor's header.
            m_editorPlates.Add(EditorKindChipFrame(), L"chip");
            m_editorPlates.Add(EditorLengthChipFrame(), L"chip");
            m_editorPlates.Add(EditorUsesChip(), L"chip", OnStates({ L"info" }));

            for (auto const& separator : { EditorSeparator1(), EditorSeparator2() })
            {
                m_editorPlates.Add(separator, L"transport.separator");
            }

            for (auto const& button : { EditorQuantizeButton(), EditorTransposeButton(), EditorKeyboardButton(), EditorAutomationButton(),
                PatternTemplatesButton(), PatternVaryButton(), PatternMakeNotesButton(), EditorCloseButton() })
            {
                m_editorPlates.Add(button, L"button.subtle", OnStates());
            }

            for (auto const& toggle : { EditorDrawToggle(), EditorGhostToggle(), PatternKeepLengthToggle(), PatternHearToggle() })
            {
                m_editorPlates.Add(toggle, L"button.subtle", OnStates());
            }

            m_editorPlates.Add(ValuesAsCombo(), L"field");
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set up the style's plates.")
    }

    void MainWindow::ApplyPlates() noexcept
    {
        try
        {
            auto const light = RootGrid().ActualTheme() == xaml::ElementTheme::Light;
            auto const& style = light ? m_styleLight : m_styleDark;

            // A style whose strips are dark draws the transport's plates in its dark look.
            auto const darkStrips = m_styleLight != nullptr && m_styleLight->Metric(L"darkStrips", 0.0) > 0.0;
            auto const& strip = darkStrips ? m_styleDark : style;

            auto changed = m_transportPlates.Theme(strip);
            changed = m_toolbarPlates.Theme(style) || changed;
            changed = m_editorPlates.Theme(style) || changed;

            // The rows and scenes are built again for a new look, which adds their plates.
            m_headerPlates.Theme(style);

            // The transport's height and the keys' spacing, as the style lays them out.
            auto const plated = style != nullptr && style->HasPart(L"transport.key");
            TransportBar().Height(plated ? style->Metric(L"transportHeight", m_defaultTransportHeight) : m_defaultTransportHeight);
            TransportLeft().Spacing(plated ? style->Metric(L"transportSpacing", m_defaultTransportSpacing) : m_defaultTransportSpacing);
            TransportRight().Spacing(plated ? style->Metric(L"transportSpacing", m_defaultTransportSpacing) : m_defaultTransportSpacing);

            // A combo box's own fills went or came back: only a change of theme makes its
            // template take that, so each panel changes to the other theme and back.
            if (changed)
            {
                for (xaml::FrameworkElement const& host : { TransportBar().as<xaml::FrameworkElement>(), ArrangeToolbar().as<xaml::FrameworkElement>(), EditorHeader().as<xaml::FrameworkElement>() })
                {
                    auto const requested = host.RequestedTheme();
                    host.RequestedTheme(host.ActualTheme() == xaml::ElementTheme::Dark ? xaml::ElementTheme::Light : xaml::ElementTheme::Dark);
                    host.RequestedTheme(requested);
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to apply the style's plates.")
    }

    void MainWindow::RefreshPlates() noexcept
    {
        m_transportPlates.Refresh();
        m_toolbarPlates.Refresh();
        m_editorPlates.Refresh();
        m_headerPlates.Refresh();
    }

    // ---------------------------------------------------------------- track and scene headers

    bool MainWindow::HeadersPlated() const noexcept
    {
        auto const theme = m_headerPlates.Theme();
        return theme != nullptr && theme->HasPart(L"header.track");
    }

    bool MainWindow::HeaderLedsPlated() const noexcept
    {
        auto const theme = m_headerPlates.Theme();
        return theme != nullptr && theme->HasPart(L"header.led");
    }

    // A track header's parts as the style draws them: the row itself (lit when selected), the
    // track's color as a cap, the M, S and R keys, and the light that shows it's playing. Its
    // row, strip and light give up their own fills for them (HeaderBackground, UpdateLeds).
    _Use_decl_annotations_
    void MainWindow::PlateHeaderRow(
        seq::ArrangeRow const& row,
        seq::Track const* track,
        controls::Grid const& grid,
        shapes::Rectangle const& strip,
        std::vector<std::pair<primitives::ToggleButton, std::wstring_view>> const& toggles,
        shapes::Ellipse const& led) noexcept
    {
        try
        {
            auto const theme = m_headerPlates.Theme();

            if (theme == nullptr)
            {
                return;
            }

            auto const color = theme->TrackColor(track != nullptr ? track->Color : 0xBDBDBDu);
            xaml::FrameworkElement const clip = row.Pinned ? PinnedHeaderCanvas().as<xaml::FrameworkElement>() : ScrollHeaderCanvas().as<xaml::FrameworkElement>();
            auto const compact = row.Height < 30.0;

            auto const options = [&](themeengine::PlateOptions plate = {})
            {
                plate.Group = L"rows";
                plate.ClipTo = clip;
                plate.Track = color;
                return plate;
            };

            if (theme->HasPart(L"header.track"))
            {
                themeengine::PlateOptions plate{};
                plate.States = [weak = get_weak(), kind = row.Kind, id = row.TrackId](xaml::FrameworkElement const&, std::vector<std::wstring>& states)
                {
                    if (kind == seq::ArrangeRowKind::Folder)
                    {
                        states.emplace_back(L"folder");
                    }
                    else if (kind == seq::ArrangeRowKind::Tempo)
                    {
                        states.emplace_back(L"tempo");
                    }

                    if (auto strong = weak.get(); strong && kind != seq::ArrangeRowKind::Tempo && !id.empty() && (id == strong->m_selectedTrackId || strong->IsTrackSelected(id)))
                    {
                        states.emplace_back(L"selected");
                    }
                };

                m_headerPlates.Add(grid, L"header.track", options(std::move(plate)));
            }

            // The strip's column is as wide as the cap and its margins.
            if (strip != nullptr && theme->HasPart(L"header.cap"))
            {
                auto const cap = theme->Part(L"header.cap", {}, color);

                if (grid.ColumnDefinitions().Size() > 0)
                {
                    grid.ColumnDefinitions().GetAt(0).Width(xaml::GridLength{ cap->Width.value_or(4.0) + cap->Margin[1] + cap->Margin[3], xaml::GridUnitType::Pixel });
                }

                strip.Fill(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
                m_headerPlates.Add(strip, L"header.cap", options());
            }

            if (theme->HasPart(L"header.toggle"))
            {
                for (auto const& [toggle, kind] : toggles)
                {
                    std::vector<std::wstring_view> extra{ kind };
                    auto plate = OnStates(extra, true);

                    if (compact)
                    {
                        // After the key's own states, so a compact row's size wins.
                        plate.States = [states = plate.States](xaml::FrameworkElement const& element, std::vector<std::wstring>& list)
                        {
                            states(element, list);
                            list.emplace_back(L"compact");
                        };
                    }

                    m_headerPlates.Add(toggle, L"header.toggle", options(LitKey(std::move(plate))));
                }
            }

            // The light is on while its tag says so (UpdateLeds).
            if (led != nullptr && theme->HasPart(L"header.led"))
            {
                led.Fill(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });

                themeengine::PlateOptions plate{};
                plate.States = [](xaml::FrameworkElement const& element, std::vector<std::wstring>& states)
                {
                    if (winrt::unbox_value_or<bool>(element.Tag(), false))
                    {
                        states.emplace_back(L"out");
                    }
                };

                m_headerPlates.Add(led, L"header.led", options(std::move(plate)));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw a track header in the style.")
    }

    // A scene header as the style draws it: a key of its own (Tactile) or a ruled cell, with the
    // scene's number and the play glyph in the part's colors.
    _Use_decl_annotations_
    void MainWindow::PlateScene(
        controls::Button const& button,
        controls::TextBlock const& label,
        ::winrt::Microsoft::UI::Xaml::Documents::Run const& number,
        controls::FontIcon const& play) noexcept
    {
        try
        {
            auto const theme = m_headerPlates.Theme();

            if (theme == nullptr || !theme->HasPart(L"scene"))
            {
                return;
            }

            auto const style = theme->Part(L"scene");

            // The text stays clear of a key inset from the cell.
            button.Padding(xaml::Thickness{ 8.0 + style->Margin[3], 0, 6.0 + style->Margin[1], 0 });

            if (auto const color = style->FindColor(L"number-color"))
            {
                number.Foreground(media::SolidColorBrush{ seq::ToUiColor(*color) });
            }

            if (auto const face = style->Word(L"number-font", {}); !face.empty())
            {
                number.FontFamily(media::FontFamily{ winrt::hstring{ theme->Font(face, face) } });
            }

            if (auto const color = style->FindColor(L"glyph-color"))
            {
                play.Foreground(media::SolidColorBrush{ seq::ToUiColor(*color) });
            }

            if (style->FontSize > 0)
            {
                label.FontSize(style->FontSize);
            }

            if (style->Word(L"numerals", {}) == L"lining")
            {
                ::winrt::Microsoft::UI::Xaml::Documents::Typography::SetNumeralStyle(label, xaml::FontNumeralStyle::Lining);
                ::winrt::Microsoft::UI::Xaml::Documents::Typography::SetNumeralAlignment(label, xaml::FontNumeralAlignment::Tabular);
            }

            themeengine::PlateOptions plate{};
            plate.Group = L"scenes";
            plate.ClipTo = SceneHeaderHost();
            m_headerPlates.Add(button, L"scene", std::move(plate));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw a scene header in the style.")
    }
}
