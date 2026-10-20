// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "GeneratorNames.h"
#include "SequenceRender.h"
#include "StringResources.h"
#include "ThemeCanvas.h"

namespace res = ::midisequencer::resources;
namespace documents = ::winrt::Microsoft::UI::Xaml::Documents;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double SceneWidth = 100.0;

        // The button after the last scene, which adds one.
        constexpr double AddSceneWidth = 32.0;
        constexpr double TicksPerFourFourBar = 3840.0;

        // Pixels from a clip's right end that resize it rather than move it.
        constexpr double ResizeEdge = 6.0;

        // How far a press has to move before it's a drag rather than a click.
        constexpr double DragThreshold = 4.0;

        xaml::GridLength Pixels(double value) noexcept
        {
            return xaml::GridLength{ value, xaml::GridUnitType::Pixel };
        }

        xaml::GridLength Star() noexcept
        {
            return xaml::GridLength{ 1, xaml::GridUnitType::Star };
        }

        xaml::GridLength Auto() noexcept
        {
            return xaml::GridLength{ 1, xaml::GridUnitType::Auto };
        }

        int64_t EffectiveLength(seq::Sequence const& doc, seq::Placement const& placement) noexcept
        {
            if (placement.Length > 0)
            {
                return placement.Length;
            }

            auto const clip = seq::FindClip(doc, placement.ClipId);
            return clip != nullptr ? std::max<int64_t>(1, clip->Length) : seq::TicksPerQuarterNote * 4;
        }

        // Every track's slots, in every folder, so a scene can be added or removed everywhere.
        void ForEachTrackMutable(std::vector<seq::Track>& tracks, std::function<void(seq::Track&)> const& visit)
        {
            for (auto& track : tracks)
            {
                visit(track);

                if (track.IsFolder)
                {
                    ForEachTrackMutable(track.Children, visit);
                }
            }
        }
    }

    // ---------------------------------------------------------------- helpers

    _Use_decl_annotations_
    media::SolidColorBrush MainWindow::BrushFor(seq::Color color) const
    {
        return media::SolidColorBrush{ color };
    }

    _Use_decl_annotations_
    media::Brush MainWindow::ThemeBrush(wchar_t const* key)
    {
        try
        {
            auto const dictionaries = RootGrid().Resources().ThemeDictionaries();
            auto const theme = RootGrid().ActualTheme() == xaml::ElementTheme::Light ? L"Light" : L"Default";
            auto const dictionary = dictionaries.Lookup(winrt::box_value(theme)).try_as<xaml::ResourceDictionary>();

            if (dictionary != nullptr && dictionary.HasKey(winrt::box_value(key)))
            {
                return dictionary.Lookup(winrt::box_value(key)).try_as<media::Brush>();
            }
        }
        catch (...)
        {
        }

        return media::SolidColorBrush{ m_palette.Text2 };
    }

    int64_t MainWindow::SnapGrid() const noexcept
    {
        return static_cast<int64_t>(seq::AppSettings::Current().SnapTicks());
    }

    _Use_decl_annotations_
    media::SolidColorBrush MainWindow::HeaderBackground(seq::ArrangeRow const& row) const
    {
        // A style that draws the headers draws them behind the row (PlateHeaderRow).
        if ((row.Kind == seq::ArrangeRowKind::Track || row.Kind == seq::ArrangeRowKind::Folder || row.Kind == seq::ArrangeRowKind::Tempo) && HeadersPlated())
        {
            return media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() };
        }

        // A theme can color the headers as plates of its own (Aluminum).
        auto const themed = [this](std::wstring_view token) -> std::optional<winrt::Windows::UI::Color>
        {
            if (m_palette.Theme != nullptr && m_palette.Theme->HasToken(token))
            {
                return seq::ToUiColor(m_palette.Theme->Token(token));
            }

            return std::nullopt;
        };

        auto const selected = row.Kind != seq::ArrangeRowKind::Tempo && !row.TrackId.empty() && (row.TrackId == m_selectedTrackId || IsTrackSelected(row.TrackId));

        if (auto const color = themed(row.Kind == seq::ArrangeRowKind::Lane ? L"headerLane"
            : selected ? L"headerSelected"
            : row.Kind == seq::ArrangeRowKind::Folder ? L"headerFolder" : L"headerTrack"); color.has_value())
        {
            return media::SolidColorBrush{ *color };
        }

        if (row.Kind == seq::ArrangeRowKind::Lane)
        {
            return media::SolidColorBrush{ m_palette.Light ? seq::Rgba(0, 0, 0, 0.025) : seq::Rgba(0, 0, 0, 0.10) };
        }

        if (row.Kind != seq::ArrangeRowKind::Tempo && !row.TrackId.empty() && (row.TrackId == m_selectedTrackId || IsTrackSelected(row.TrackId)))
        {
            return media::SolidColorBrush{ seq::WithAlpha(m_palette.Accent, 0.06) };
        }

        if (row.Kind == seq::ArrangeRowKind::Folder)
        {
            return media::SolidColorBrush{ seq::WithAlpha(m_palette.Light ? seq::Rgba(0, 0, 0) : seq::Rgba(255, 255, 255), m_palette.Light ? 0.025 : 0.03) };
        }

        // Transparent still takes clicks; no background at all wouldn't.
        return media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() };
    }

    _Use_decl_annotations_
    std::wstring MainWindow::NextClipName(wchar_t const* stemKey) const
    {
        std::wstring const stem{ res::GetString(stemKey) };
        std::set<int64_t> used{};

        for (auto const& clip : m_doc.Clips)
        {
            if (clip.Name.size() > stem.size() + 1 && clip.Name.starts_with(stem) && clip.Name[stem.size()] == L' ')
            {
                try
                {
                    size_t consumed{ 0 };
                    auto const number = std::stoll(clip.Name.substr(stem.size() + 1), &consumed);

                    if (consumed == clip.Name.size() - stem.size() - 1)
                    {
                        used.insert(number);
                    }
                }
                catch (...)
                {
                }
            }
        }

        int64_t next{ 1 };

        while (used.contains(next))
        {
            ++next;
        }

        return stem + L" " + std::to_wstring(next);
    }

    // ---------------------------------------------------------------- canvases

    void MainWindow::CreateCanvases()
    {
        m_textPlayingLaunchedClip = res::GetString(L"PlayingLaunchedClip");
        m_textStoppedFromLauncher = res::GetString(L"StoppedFromLauncher");
        m_textBackToTimeline = res::GetString(L"BackToTimelineCaption");
        m_textBarFormat = res::GetString(L"BarFormat");
        m_textRecording = res::GetString(L"RecordingCaption");
        m_textGenerated = res::GetString(L"GeneratedAsItPlays");
        m_textEmptyLane = res::GetString(L"EmptyLaneHint");
        m_textClipLane = res::GetString(L"ClipLaneCaption");
        m_textEmptyLyrics = res::GetString(L"LyricsEmptyHint");

        auto const make = [](controls::Grid const& host, bool underOthers)
        {
            canvasXaml::CanvasControl canvas{};
            canvas.ClearColor(winrt::Microsoft::UI::Colors::Transparent());

            // These are drawn again many times a second while a sequence plays; a size that isn't a
            // whole number of DIPs and pixels makes Win2D shake them by a pixel (ThemeCanvas.h).
            themeengine::KeepWholePixels(canvas);

            if (underOthers)
            {
                host.Children().InsertAt(0, canvas);
            }
            else
            {
                host.Children().Append(canvas);
            }

            return canvas;
        };

        m_rulerCanvas = make(RulerHost(), false);
        m_pinnedLaneCanvas = make(PinnedLaneHost(), false);
        m_scrollLaneCanvas = make(ScrollLaneHost(), true);
        m_pinnedLauncherCanvas = make(PinnedLauncherHost(), false);
        m_scrollLauncherCanvas = make(ScrollLauncherHost(), false);
        m_editorCanvas = make(EditorCanvasHost(), false);
        m_editorCanvas.IsTabStop(true);
        automation::AutomationProperties::SetName(m_editorCanvas, res::GetString(L"EditorCanvasName"));
        automation::AutomationProperties::SetName(m_scrollLaneCanvas, res::GetString(L"TimelineCanvasName"));
        automation::AutomationProperties::SetName(m_scrollLauncherCanvas, res::GetString(L"LauncherCanvasName"));

        auto weak = get_weak();

        m_rulerCanvas.Draw([weak](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                try
                {
                    auto context = strong->DrawContext();
                    context.Creator = sender;
                    strong->m_steadyRuler.Draw(sender, args, [&](canvas::CanvasDrawingSession const& ds)
                    {
                        strong->m_renderer.DrawRuler(ds, static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()), context);
                    });
                }
                MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw the ruler.")
            }
        });

        auto const drawLanes = [weak](bool pinned)
        {
            return [weak, pinned](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    try
                    {
                        auto const& rows = pinned ? strong->m_layout.Pinned : strong->m_layout.Scrolling;
                        auto context = strong->DrawContext();
                        context.Creator = sender;
                        strong->m_steadyLanes[pinned ? 0 : 1].Draw(sender, args, [&](canvas::CanvasDrawingSession const& ds)
                        {
                            strong->m_renderer.DrawLanes(ds, static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()),
                                rows, pinned ? 0.0 : strong->m_scrollY, !pinned, context);
                            strong->DrawKeyboardFocus(ds, sender);
                        });
                    }
                    MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw the timeline.")
                }
            };
        };

        m_pinnedLaneCanvas.Draw(drawLanes(true));
        m_scrollLaneCanvas.Draw(drawLanes(false));

        auto const drawLauncher = [weak](bool pinned)
        {
            return [weak, pinned](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    try
                    {
                        auto const& rows = pinned ? strong->m_layout.Pinned : strong->m_layout.Scrolling;
                        auto context = strong->DrawContext();
                        context.Creator = sender;
                        strong->m_steadyLaunchers[pinned ? 0 : 1].Draw(sender, args, [&](canvas::CanvasDrawingSession const& ds)
                        {
                            strong->m_renderer.DrawLauncher(ds, static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()),
                                rows, pinned ? 0.0 : strong->m_scrollY, !pinned, context);
                            strong->DrawKeyboardFocus(ds, sender);
                        });
                    }
                    MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw the clip launcher.")
                }
            };
        };

        m_pinnedLauncherCanvas.Draw(drawLauncher(true));
        m_scrollLauncherCanvas.Draw(drawLauncher(false));

        m_editorCanvas.Draw([weak](canvasXaml::CanvasControl const& sender, canvasXaml::CanvasDrawEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                try
                {
                    strong->m_steadyEditor.Draw(sender, args, [&](canvas::CanvasDrawingSession const& ds)
                    {
                        if (strong->EditorShowsPattern())
                        {
                            strong->m_pattern.Draw(ds, static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()));
                        }
                        else
                        {
                            strong->DrawNotesEditor(ds, static_cast<float>(sender.ActualWidth()), static_cast<float>(sender.ActualHeight()));
                        }
                    });
                }
                MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to draw the clip editor.")
            }
        });

        // Lanes
        for (auto const pinned : { true, false })
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;

            canvas.PointerPressed([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLanePressed(pinned, args); }
            });

            canvas.PointerMoved([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLaneMoved(pinned, args); }
            });

            canvas.PointerReleased([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLaneReleased(pinned, args); }
            });

            canvas.PointerCaptureLost([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLaneReleased(pinned, args); }
            });

            canvas.PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLaneWheel(args); }
            });

            canvas.DoubleTapped([weak, pinned](auto&&, input::DoubleTappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLaneDoubleTapped(pinned, args); }
            });

            canvas.RightTapped([weak, pinned](auto&&, input::RightTappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLaneRightTapped(pinned, args); }
            });

            auto const launcher = pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas;

            launcher.PointerPressed([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLauncherPressed(pinned, args); }
            });

            launcher.DoubleTapped([weak, pinned](auto&&, input::DoubleTappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLauncherDoubleTapped(pinned, args); }
            });

            launcher.RightTapped([weak, pinned](auto&&, input::RightTappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLauncherRightTapped(pinned, args); }
            });

            launcher.PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLauncherWheel(args, false); }
            });
        }

        m_rulerCanvas.PointerPressed([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnRulerPressed(args); }
        });

        m_rulerCanvas.PointerMoved([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnRulerMoved(args); }
        });

        m_rulerCanvas.PointerReleased([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnRulerReleased(args); }
        });

        // Lost part way through a drag, say to Alt+Tab: what was dragged so far stays, and the drag
        // ends, so the pointer passing over later changes nothing.
        m_rulerCanvas.PointerCaptureLost([weak](auto&&, auto&&)
        {
            try
            {
                if (auto strong = weak.get(); strong && strong->m_rulerDragging)
                {
                    strong->m_rulerDragging = false;
                    strong->m_loopGrab = LoopGrab::None;

                    if (strong->m_rulerMoved)
                    {
                        strong->ApplyEngineSettings();
                    }
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to end a drag on the ruler.")
        });

        m_rulerCanvas.PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnLaneWheel(args); }
        });

        m_editorCanvas.PointerPressed([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorPressed(args); }
        });

        m_editorCanvas.PointerMoved([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorMoved(args); }
        });

        m_editorCanvas.PointerReleased([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorReleased(args); }
        });

        m_editorCanvas.PointerCaptureLost([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorReleased(args); }
        });

        m_editorCanvas.PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorWheel(args); }
        });

        m_editorCanvas.KeyDown([weak](auto&&, input::KeyRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorKeyDown(args); }
        });

        m_editorCanvas.ContextRequested([weak](auto&&, input::ContextRequestedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnEditorContextRequested(args); }
        });

        HookArrangeKeyboard();
        SetUpTimelineBar();

        m_editorCanvas.SizeChanged([weak](auto&&, xaml::SizeChangedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                // The first time the editor has a size, show the whole clip.
                if (args.PreviousSize().Height <= 1 && strong->m_roll.HasClip())
                {
                    strong->m_roll.FitToClip(static_cast<float>(args.NewSize().Width), strong->NotesEditorRollHeight());
                }

                strong->InvalidateEditor();
            }
        });

        ScrollLaneHost().SizeChanged([weak](auto&&, auto&&)
        {
            if (auto strong = weak.get()) { strong->OnArrangeSizeChanged(); }
        });

        ScrollHeaderCanvas().SizeChanged([weak](auto&&, xaml::SizeChangedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                // A canvas doesn't clip what it holds, and a header scrolled up would sit on the
                // pinned rows.
                media::RectangleGeometry clip{};
                clip.Rect(foundation::Rect{ 0, 0, args.NewSize().Width, args.NewSize().Height });
                strong->ScrollHeaderCanvas().Clip(clip);
                strong->PositionHeaders();
            }
        });

        SceneHeaderHost().SizeChanged([weak](auto&&, xaml::SizeChangedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                media::RectangleGeometry clip{};
                clip.Rect(foundation::Rect{ 0, 0, args.NewSize().Width, args.NewSize().Height });
                strong->SceneHeaderHost().Clip(clip);
            }
        });

        ScrollHeaderCanvas().PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnLaneWheel(args); }
        });

        SceneHeaderHost().PointerWheelChanged([weak](auto&&, input::PointerRoutedEventArgs const& args)
        {
            if (auto strong = weak.get()) { strong->OnLauncherWheel(args, true); }
        });

        // A wider launcher may show every scene, and a narrower one fewer.
        m_scrollLauncherCanvas.SizeChanged([weak](auto&&, xaml::SizeChangedEventArgs const&)
        {
            if (auto strong = weak.get()) { strong->ScrollScenes(strong->m_sceneScrollX); }
        });
    }

    void MainWindow::OnArrangeSizeChanged() noexcept
    {
        try
        {
            UpdateScrollBars();
            PositionHeaders();
            InvalidateArrange();
            InvalidateLaunchers();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to lay out the timeline.")
    }

    seq::ArrangeDrawContext MainWindow::DrawContext()
    {
        seq::ArrangeDrawContext context{};
        context.Doc = &m_doc;
        context.Colors = &m_palette;
        context.View.PixelsPerTick = m_barWidth / TicksPerFourFourBar;
        context.View.ScrollX = m_scrollX;

        if (m_selectedPlacement != SIZE_MAX)
        {
            context.SelectedTrackId = m_selectedTrackId;
            context.SelectedPlacement = m_selectedPlacement;
        }

        context.SelectedSlotTrackId = m_selectedSlotTrackId;
        context.SelectedSlot = m_selectedSlot;

        context.LoopEnabled = LoopToggle().IsChecked().GetBoolean();
        context.LoopStart = m_loopStart;
        context.LoopEnd = m_loopEnd;

        context.Launch = &m_launchViews;
        context.Armed = &m_armed;

        if (m_recording && !m_recordingPreviewTrackId.empty())
        {
            context.RecordingTrackId = m_recordingPreviewTrackId;
            context.RecordingStartTick = m_recordingPreviewStart;
            context.RecordingNotes = &m_recordingPreview;
        }

        context.RecordingSlot = SlotRecordingView();

        context.NowTick = m_position;
        context.SceneWidth = SceneWidth;
        context.SceneScrollX = m_sceneScrollX;

        context.PlayingLaunchedClip = m_textPlayingLaunchedClip;
        context.StoppedFromLauncher = m_textStoppedFromLauncher;
        context.BackToTimeline = m_textBackToTimeline;
        context.BarFormat = m_textBarFormat;
        context.RecordingCaption = m_textRecording;
        context.GeneratedAsItPlays = m_textGenerated;
        context.EmptyLaneHint = m_textEmptyLane;
        context.ClipLaneCaption = m_textClipLane;
        context.EmptyLyricsHint = m_textEmptyLyrics;

        context.SelectedLaneTrackId = m_selectedLaneTrackId;
        context.SelectedLaneId = m_selectedLaneId;
        context.SelectedLanePoint = m_selectedLanePoint;
        context.SelectedTempoPoint = m_selectedTempoPoint;

        if (m_clipDrag.Active)
        {
            context.ClipDrag = m_clipDrag.View;
        }

        if (m_pointDrag.What == PointDrag::Kind::TempoPoint)
        {
            context.TempoScale = m_pointDrag.Range;
        }

        return context;
    }

    double MainWindow::LaneWidth() noexcept
    {
        try
        {
            return std::max(1.0, ScrollLaneHost().ActualWidth());
        }
        catch (...)
        {
            return 1.0;
        }
    }

    double MainWindow::ScrollViewportHeight() noexcept
    {
        try
        {
            return std::max(1.0, ScrollLaneHost().ActualHeight());
        }
        catch (...)
        {
            return 1.0;
        }
    }

    void MainWindow::InvalidateArrange() noexcept
    {
        try
        {
            if (m_rulerCanvas != nullptr) { m_rulerCanvas.Invalidate(); }
            if (m_pinnedLaneCanvas != nullptr) { m_pinnedLaneCanvas.Invalidate(); }
            if (m_scrollLaneCanvas != nullptr) { m_scrollLaneCanvas.Invalidate(); }

            UpdatePlayhead();
        }
        catch (...)
        {
        }
    }

    void MainWindow::InvalidateLaunchers() noexcept
    {
        try
        {
            if (LauncherColumn().Width().Value <= 0)
            {
                return;
            }

            if (m_pinnedLauncherCanvas != nullptr) { m_pinnedLauncherCanvas.Invalidate(); }
            if (m_scrollLauncherCanvas != nullptr) { m_scrollLauncherCanvas.Invalidate(); }
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- layout and headers

    void MainWindow::RebuildLayout() noexcept
    {
        try
        {
            m_layout = seq::BuildArrangeLayout(m_doc);
            PinnedRow().Height(Pixels(m_layout.PinnedHeight));

            // A selection that no longer exists goes.
            if (!m_selectedTrackId.empty() && seq::FindTrack(m_doc, m_selectedTrackId) == nullptr)
            {
                m_selectedTrackId.clear();
                m_selectedPlacement = SIZE_MAX;
            }

            std::erase_if(m_selectedTrackIds, [this](std::wstring const& id) { return seq::FindTrack(m_doc, id) == nullptr; });

            if (!m_selectedTrackId.empty() && !IsTrackSelected(m_selectedTrackId))
            {
                m_selectedTrackIds.push_back(m_selectedTrackId);
            }

            if (m_selectedPlacement != SIZE_MAX)
            {
                auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

                if (track == nullptr || m_selectedPlacement >= track->Timeline.size())
                {
                    m_selectedPlacement = SIZE_MAX;
                }
            }

            for (auto it = m_armed.begin(); it != m_armed.end();)
            {
                it = seq::FindTrack(m_doc, *it) == nullptr ? m_armed.erase(it) : std::next(it);
            }

            RebuildHeaders();
            RebuildSceneHeaders();
            UpdateScrollBars();
            InvalidateArrange();
            InvalidateLaunchers();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to lay out the tracks.")
    }

    void MainWindow::RebuildHeaders() noexcept
    {
        try
        {
            m_headerPlates.Remove(L"rows");
            PinnedHeaderCanvas().Children().Clear();
            ScrollHeaderCanvas().Children().Clear();
            m_pinnedHeaders.clear();
            m_scrollHeaders.clear();
            m_leds.clear();
            m_laneChips.clear();

            for (auto const& row : m_layout.Pinned)
            {
                auto element = MakeHeaderRow(row);
                PinnedHeaderCanvas().Children().Append(element);
                m_pinnedHeaders.emplace_back(row, element);
            }

            for (auto const& row : m_layout.Scrolling)
            {
                auto element = MakeHeaderRow(row);
                ScrollHeaderCanvas().Children().Append(element);
                m_scrollHeaders.emplace_back(row, element);
            }

            PositionHeaders();
            UpdateLaneChips(m_position);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to build the track headers.")
    }

    void MainWindow::PositionHeaders() noexcept
    {
        try
        {
            for (auto const& [row, element] : m_pinnedHeaders)
            {
                controls::Canvas::SetTop(element, row.Top);
            }

            auto const viewport = ScrollViewportHeight();

            for (auto const& [row, element] : m_scrollHeaders)
            {
                auto const top = row.Top - m_scrollY;
                auto const visible = top + row.Height >= 0 && top <= viewport;

                element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                if (visible)
                {
                    controls::Canvas::SetTop(element, top);
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeHeaderRow(seq::ArrangeRow const& row)
    {
        if (row.Kind == seq::ArrangeRowKind::Lane)
        {
            return MakeLaneHeader(row);
        }

        if (row.Kind == seq::ArrangeRowKind::Lyrics)
        {
            return MakeLyricsHeader(row);
        }

        controls::Grid grid{};
        grid.Width(m_headerWidth);
        grid.Height(row.Height);
        grid.BorderThickness(xaml::Thickness{ 0, 0, 1, 1 });
        grid.BorderBrush(BrushFor(m_palette.Divider));
        grid.Background(BrushFor(winrt::Microsoft::UI::Colors::Transparent()));
        grid.ColumnSpacing(6);
        grid.Padding(xaml::Thickness{ 0, 0, 8, 0 });

        // A short row has room for its name only, and a compact one for smaller buttons too.
        auto const oneLine = row.Height < seq::OneLineRowHeight;
        auto const compact = row.Height < 30.0;

        auto const columns = grid.ColumnDefinitions();

        auto const addColumn = [&columns](xaml::GridLength width)
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            columns.Append(column);
        };

        // strip, indent, chevron, icon, name, buttons, light
        addColumn(Pixels(4));
        addColumn(Pixels(row.Depth > 0 ? 14.0 * static_cast<double>(row.Depth) - 6.0 : 0.0));
        addColumn(Pixels(14));
        addColumn(Auto());
        addColumn(Star());
        addColumn(Auto());
        addColumn(Pixels(7));

        auto const place = [&grid](xaml::FrameworkElement const& element, int32_t column)
        {
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
        };

        auto const text3 = BrushFor(m_palette.Text3);

        if (row.Kind == seq::ArrangeRowKind::AddTrack)
        {
            controls::Button add{};
            add.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SubtleSmallButtonStyle")).as<xaml::Style>());
            add.Margin(xaml::Thickness{ 4, 0, 0, 0 });

            controls::StackPanel content{};
            content.Orientation(controls::Orientation::Horizontal);
            content.Spacing(6);

            controls::FontIcon plus{};
            plus.Glyph(L"\uE710");
            plus.FontSize(12);
            plus.Foreground(text3);
            content.Children().Append(plus);

            controls::TextBlock label{};
            label.Text(res::GetString(L"AddTrackRowText"));
            label.Foreground(BrushFor(m_palette.Text2));
            content.Children().Append(label);

            add.Content(content);
            automation::AutomationProperties::SetName(add, res::GetString(L"AddTrackRowText"));
            add.Click([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->AddTrack(false); }
            });

            controls::Grid::SetColumnSpan(add, 4);
            place(add, 2);
            grid.BorderThickness(xaml::Thickness{ 0, 0, 1, 0 });

            return grid;
        }

        seq::Track const* track = row.Kind == seq::ArrangeRowKind::Tempo ? nullptr : seq::FindTrack(m_doc, row.TrackId);

        if (row.Kind != seq::ArrangeRowKind::Tempo && track == nullptr)
        {
            return grid;
        }

        auto const color = track != nullptr ? seq::FromRgb(track->Color) : seq::FromRgb(0xBDBDBD);
        grid.Background(HeaderBackground(row));

        // What a style draws behind the row (PlateHeaderRow).
        std::vector<std::pair<primitives::ToggleButton, std::wstring_view>> toggles{};
        shapes::Ellipse light{ nullptr };

        shapes::Rectangle strip{};
        strip.Fill(BrushFor(color));
        place(strip, 0);

        if (row.Depth > 0)
        {
            shapes::Rectangle guide{};
            guide.Width(1);
            guide.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            guide.Margin(xaml::Thickness{ 0, 0, 1, 0 });
            guide.Fill(BrushFor(m_palette.Divider));
            place(guide, 1);
        }

        // Folders open and close with the chevron, and a track with automation shows or hides it.
        if (track != nullptr && (row.Kind == seq::ArrangeRowKind::Folder || (row.Kind == seq::ArrangeRowKind::Track && !track->Lanes.empty())))
        {
            auto const open = track->IsFolder ? track->Open : track->LanesOpen;

            controls::Button chevron{};
            chevron.Width(18);
            chevron.Height(compact ? 18 : 22);
            chevron.Padding(xaml::Thickness{ 0, 0, 0, 0 });
            chevron.MinWidth(0);
            chevron.MinHeight(0);
            chevron.Background(BrushFor(winrt::Microsoft::UI::Colors::Transparent()));
            chevron.BorderThickness(xaml::Thickness{ 0, 0, 0, 0 });
            chevron.Margin(xaml::Thickness{ -2, 0, -2, 0 });

            controls::FontIcon glyph{};
            glyph.Glyph(open ? L"\uE70D" : L"\uE76C");
            glyph.FontSize(9);
            glyph.Foreground(text3);
            chevron.Content(glyph);

            auto const nameKey = track->IsFolder
                ? (open ? L"CloseFolderFormat" : L"OpenFolderFormat")
                : (open ? L"HideAutomationFormat" : L"ShowAutomationFormat");

            automation::AutomationProperties::SetName(chevron, res::FormatString(nameKey, track->Name));
            controls::ToolTipService::SetToolTip(chevron, winrt::box_value(res::FormatString(nameKey, track->Name)));
            chevron.Click([weak = get_weak(), id = track->Id, folder = track->IsFolder](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    if (folder)
                    {
                        strong->ToggleFolder(id);
                    }
                    else
                    {
                        strong->ToggleLanes(id);
                    }
                }
            });

            place(chevron, 2);
        }

        if (row.Kind == seq::ArrangeRowKind::Tempo || row.Kind == seq::ArrangeRowKind::Folder)
        {
            controls::FontIcon icon{};
            icon.Glyph(row.Kind == seq::ArrangeRowKind::Tempo ? L"\uE916" : L"\uE8B7");
            icon.FontSize(13);
            icon.Width(16);
            icon.Foreground(row.Kind == seq::ArrangeRowKind::Tempo ? BrushFor(m_palette.Text2) : BrushFor(color));
            place(icon, 3);
        }
        else if (track != nullptr && !oneLine)
        {
            // The picture of the device it plays to, from MIDI Settings or its transport, or of the
            // one it records from while it's armed, when the row has room for two lines. A track
            // without one keeps the space, so the names still line up.
            auto const& endpoint = m_armed.contains(track->Id) ? track->Source.Endpoint : track->Destination.Endpoint;
            auto const size = std::min(28.0, row.Height - 16.0);

            if (auto const picture = DevicePictureFor(endpoint, size); picture != nullptr)
            {
                place(picture, 3);
            }
            else
            {
                controls::Border space{};
                space.Width(size);
                place(space, 3);
            }
        }

        // name and what it plays to
        controls::StackPanel info{};
        info.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::TextBlock name{};
        name.FontSize(13);
        name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

        // A style can name the tracks in a face of its own (Vellum's Candara).
        if (m_palette.Theme != nullptr)
        {
            if (auto const face = m_palette.Theme->Font(L"names", {}); !face.empty())
            {
                name.FontFamily(media::FontFamily{ winrt::hstring{ face } });
                name.FontSize(14);
            }
        }
        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        name.TextWrapping(xaml::TextWrapping::NoWrap);

        auto const addRun = [&name](std::wstring_view text, media::Brush const& brush, bool regular, double size, bool glyph)
        {
            documents::Run run{};
            run.Text(winrt::hstring{ text });

            if (brush != nullptr)
            {
                run.Foreground(brush);
            }

            if (regular)
            {
                run.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Normal());
            }

            if (size > 0)
            {
                run.FontSize(size);
            }

            if (glyph)
            {
                run.FontFamily(media::FontFamily{ L"Segoe Fluent Icons,Segoe MDL2 Assets" });
            }

            name.Inlines().Append(run);
        };

        if (row.Kind == seq::ArrangeRowKind::Tempo)
        {
            addRun(res::GetString(L"TempoTrackName"), nullptr, false, 0, false);
            addRun(L"  ", nullptr, true, 0, false);
            addRun(L"\uE718", text3, true, 10, true);
        }
        else
        {
            if (!row.FolderName.empty())
            {
                addRun(row.FolderName + L" \u203A ", text3, true, 0, false);
            }

            addRun(track->Name, nullptr, false, 0, false);

            if (track->Pinned)
            {
                addRun(L"  ", nullptr, true, 0, false);
                addRun(L"\uE718", text3, true, 10, true);
            }

            if (track->IsFolder)
            {
                size_t count{ 0 };

                std::function<void(std::vector<seq::Track> const&)> countTracks = [&](std::vector<seq::Track> const& children)
                {
                    for (auto const& child : children)
                    {
                        if (child.IsFolder)
                        {
                            countTracks(child.Children);
                        }
                        else
                        {
                            ++count;
                        }
                    }
                };

                countTracks(track->Children);
                addRun(L"  " + std::wstring{ res::FormatString(count == 1 ? L"FolderCountOneFormat" : L"FolderCountFormat", count) }, text3, true, 11, false);
            }
        }

        info.Children().Append(name);

        if (!oneLine && (row.Kind == seq::ArrangeRowKind::Tempo || (track != nullptr && !track->IsFolder)))
        {
            controls::TextBlock sub{};
            sub.Tag(winrt::box_value(L"sub"));
            sub.FontSize(11);
            sub.Margin(xaml::Thickness{ 0, 1, 0, 0 });
            sub.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            sub.TextWrapping(xaml::TextWrapping::NoWrap);
            sub.Foreground(text3);

            if (row.Kind == seq::ArrangeRowKind::Tempo)
            {
                auto const bpm = m_doc.Tempo.empty() ? 120.0 : m_doc.Tempo.front().BeatsPerMinute;
                auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
                auto text = res::FormatString(L"TempoTrackSubFormat", std::format(L"{:g}", std::round(bpm * 100.0) / 100.0), meter.Numerator, meter.Denominator);

                if (!m_doc.Tags.empty())
                {
                    text = text + L" \u00B7 " + res::FormatString(m_doc.Tags.size() == 1 ? L"TagCountOneFormat" : L"TagCountFormat", m_doc.Tags.size());
                }

                sub.Text(text);
            }
            else
            {
                auto const missing = !track->Destination.Endpoint.IsEmpty() && !m_directory->Resolve(track->Destination.Endpoint).has_value();

                documents::Run glyph{};
                glyph.Text(m_armed.contains(track->Id) ? L"\uE720 " : L"\uE72A ");
                glyph.FontFamily(media::FontFamily{ L"Segoe Fluent Icons,Segoe MDL2 Assets" });
                glyph.FontSize(9);
                sub.Inlines().Append(glyph);

                documents::Run text{};
                text.Text(winrt::hstring{ m_armed.contains(track->Id) ? DescribeSource(*track) : DescribeDestination(*track) });
                sub.Inlines().Append(text);

                SetDeviceToolTip(sub, m_armed.contains(track->Id) ? track->Source.Endpoint : track->Destination.Endpoint);

                if (missing)
                {
                    sub.Foreground(ThemeBrush(L"SeqWarnTextBrush"));
                }
                else if (m_armed.contains(track->Id))
                {
                    sub.Foreground(ThemeBrush(L"SeqRecordTextBrush"));
                }
            }

            info.Children().Append(sub);
        }

        place(info, 4);

        // M, S and R
        if (row.Kind != seq::ArrangeRowKind::Tempo && track != nullptr)
        {
            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(3);
            buttons.VerticalAlignment(xaml::VerticalAlignment::Center);

            auto const makeFlag = [&](wchar_t flag, bool on, seq::Color onFill, seq::Color onText, seq::Color offText, std::wstring_view nameKey, std::wstring_view kind)
            {
                primitives::ToggleButton toggle{};
                toggle.Width(compact ? 20 : 22);
                toggle.Height(compact ? 18 : 20);
                toggle.MinWidth(0);
                toggle.MinHeight(0);
                toggle.Padding(xaml::Thickness{ 0, 0, 0, 0 });
                toggle.CornerRadius(xaml::CornerRadius{ 3, 3, 3, 3 });
                toggle.FontSize(compact ? 9 : 10);
                toggle.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
                toggle.Content(winrt::box_value(winrt::hstring{ std::wstring(1, flag) }));
                toggle.IsChecked(on);

                auto const resources = toggle.Resources();
                auto const fill = BrushFor(onFill);
                auto const text = BrushFor(onText);
                auto const off = BrushFor(offText);
                auto const offFill = BrushFor(m_palette.Light ? seq::Rgba(0, 0, 0, 0.03) : seq::Rgba(255, 255, 255, 0.04));
                auto const stroke = BrushFor(m_palette.StrokeStrong);

                resources.Insert(winrt::box_value(L"ToggleButtonBackground"), offFill);
                resources.Insert(winrt::box_value(L"ToggleButtonBackgroundPointerOver"), BrushFor(m_palette.Light ? seq::Rgba(0, 0, 0, 0.06) : seq::Rgba(255, 255, 255, 0.08)));
                resources.Insert(winrt::box_value(L"ToggleButtonBackgroundPressed"), offFill);
                resources.Insert(winrt::box_value(L"ToggleButtonBorderBrush"), stroke);
                resources.Insert(winrt::box_value(L"ToggleButtonBorderBrushPointerOver"), stroke);
                resources.Insert(winrt::box_value(L"ToggleButtonBorderBrushPressed"), stroke);
                resources.Insert(winrt::box_value(L"ToggleButtonForeground"), off);
                resources.Insert(winrt::box_value(L"ToggleButtonForegroundPointerOver"), off);
                resources.Insert(winrt::box_value(L"ToggleButtonForegroundPressed"), off);
                resources.Insert(winrt::box_value(L"ToggleButtonBackgroundChecked"), fill);
                resources.Insert(winrt::box_value(L"ToggleButtonBackgroundCheckedPointerOver"), fill);
                resources.Insert(winrt::box_value(L"ToggleButtonBackgroundCheckedPressed"), fill);
                resources.Insert(winrt::box_value(L"ToggleButtonBorderBrushChecked"), fill);
                resources.Insert(winrt::box_value(L"ToggleButtonBorderBrushCheckedPointerOver"), fill);
                resources.Insert(winrt::box_value(L"ToggleButtonBorderBrushCheckedPressed"), fill);
                resources.Insert(winrt::box_value(L"ToggleButtonForegroundChecked"), text);
                resources.Insert(winrt::box_value(L"ToggleButtonForegroundCheckedPointerOver"), text);
                resources.Insert(winrt::box_value(L"ToggleButtonForegroundCheckedPressed"), text);

                automation::AutomationProperties::SetName(toggle, res::FormatString(nameKey, track->Name));
                controls::ToolTipService::SetToolTip(toggle, winrt::box_value(res::FormatString(nameKey, track->Name)));

                toggle.Click([weak = get_weak(), id = track->Id, flag](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        auto const button = sender.try_as<primitives::ToggleButton>();
                        auto const value = button != nullptr && button.IsChecked() != nullptr && button.IsChecked().GetBoolean();
                        strong->ToggleTrackFlag(id, flag, value);
                    }
                });

                buttons.Children().Append(toggle);
                toggles.emplace_back(toggle, kind);
            };

            auto const dark = seq::FromRgb(0x1A1A1A);

            makeFlag(L'M', track->Muted, m_palette.Mute, dark, m_palette.Text3, L"MuteTrackFormat", L"mute");
            makeFlag(L'S', track->Soloed, m_palette.Solo, dark, m_palette.Text3, L"SoloTrackFormat", L"solo");

            if (!track->IsFolder)
            {
                makeFlag(L'R', m_armed.contains(track->Id), m_palette.Record, seq::FromRgb(0xFFFFFF), m_palette.Light ? m_palette.RecordText : seq::FromRgb(0xFF8A8E), L"ArmTrackFormat", L"record");
            }
            else
            {
                controls::Border blank{};
                blank.Width(compact ? 20 : 22);
                buttons.Children().Append(blank);
            }

            place(buttons, 5);

            shapes::Ellipse led{};
            led.Width(7);
            led.Height(7);
            led.VerticalAlignment(xaml::VerticalAlignment::Center);
            led.Fill(BrushFor(m_palette.Light ? seq::Rgba(0, 0, 0, 0.12) : seq::Rgba(255, 255, 255, 0.14)));
            place(led, 6);

            m_leds.emplace(track->Id, led);
            light = led;
        }

        // Selecting, opening the setup, the track menu, and dragging it somewhere else.
        if (track != nullptr)
        {
            grid.Tapped([weak = get_weak(), id = track->Id](auto&&, input::TappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    auto const control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    auto const shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

                    strong->ClickTrack(id, control, shift);

                    if (!control && !shift && strong->TrackInspector().Visibility() == xaml::Visibility::Visible)
                    {
                        strong->ShowInspector(id);
                    }

                    args.Handled(true);
                }
            });

            grid.DoubleTapped([weak = get_weak(), id = track->Id, folder = track->IsFolder](auto&&, input::DoubleTappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    if (folder)
                    {
                        strong->ToggleFolder(id);
                    }
                    else
                    {
                        strong->ShowInspector(id);
                    }

                    args.Handled(true);
                }
            });

            grid.RightTapped([weak = get_weak(), id = track->Id, element = winrt::make_weak(grid)](auto&&, input::RightTappedRoutedEventArgs const& args)
            {
                auto strong = weak.get();
                auto anchor = element.get();

                if (strong && anchor)
                {
                    // A track among several selected keeps them selected, so the menu acts on all.
                    if (!strong->IsTrackSelected(id))
                    {
                        strong->SelectTrack(id);
                    }

                    strong->ShowTrackMenu(id, anchor, args.GetPosition(anchor));
                    args.Handled(true);
                }
            });

            grid.PointerPressed([weak = get_weak(), id = track->Id, pinned = row.Pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnHeaderPressed(id, pinned, args); }
            });

            grid.PointerMoved([weak = get_weak(), pinned = row.Pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnHeaderMoved(pinned, args); }
            });

            automation::AutomationProperties::SetName(grid, winrt::hstring{ track->Name });
        }
        else
        {
            automation::AutomationProperties::SetName(grid, res::GetString(L"TempoTrackName"));
        }

        AddRowGrip(grid, row);
        PlateHeaderRow(row, track, grid, strip, toggles, light);

        return grid;
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeLaneHeader(seq::ArrangeRow const& row)
    {
        controls::Grid grid{};
        grid.Width(m_headerWidth);
        grid.Height(row.Height);
        grid.BorderThickness(xaml::Thickness{ 0, 0, 1, 1 });
        grid.BorderBrush(BrushFor(m_palette.Divider));
        grid.Background(HeaderBackground(row));
        grid.ColumnSpacing(6);
        grid.Padding(xaml::Thickness{ 0, 0, 8, 0 });

        auto const track = seq::FindTrack(m_doc, row.TrackId);

        if (track == nullptr)
        {
            return grid;
        }

        auto const found = std::find_if(track->Lanes.begin(), track->Lanes.end(), [&row](seq::AutomationLane const& lane) { return lane.Id == row.LaneId; });

        if (found == track->Lanes.end())
        {
            return grid;
        }

        auto const& lane = *found;
        auto const columns = grid.ColumnDefinitions();

        auto const addColumn = [&columns](xaml::GridLength width)
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            columns.Append(column);
        };

        // strip, indent (to line up under the track's name), name, value, add
        addColumn(Pixels(4));
        addColumn(Pixels(14.0 * static_cast<double>(row.Depth) + 22.0));
        addColumn(Star());
        addColumn(Auto());
        addColumn(Auto());

        auto const place = [&grid](xaml::FrameworkElement const& element, int32_t column)
        {
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
        };

        shapes::Rectangle strip{};
        strip.Fill(BrushFor(seq::FromRgb(track->Color)));
        strip.Opacity(0.35);
        place(strip, 0);

        controls::StackPanel info{};
        info.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::TextBlock name{};
        name.Text(winrt::hstring{ LaneName(lane) });
        name.FontSize(12);
        name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        name.TextWrapping(xaml::TextWrapping::NoWrap);
        info.Children().Append(name);

        controls::TextBlock sub{};
        sub.Text(winrt::hstring{ LaneDescription(*track, lane) });
        sub.FontSize(11);
        sub.Margin(xaml::Thickness{ 0, 1, 0, 0 });
        sub.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        sub.TextWrapping(xaml::TextWrapping::NoWrap);
        sub.Foreground(BrushFor(m_palette.Text3));

        if (auto const tip = LaneIntervalToolTip(*track); !tip.empty())
        {
            controls::ToolTipService::SetToolTip(sub, winrt::box_value(tip));
        }

        info.Children().Append(sub);

        place(info, 2);

        // The value at the playhead, in a chip like the comps'.
        controls::Grid chip{};
        chip.Height(20);
        chip.VerticalAlignment(xaml::VerticalAlignment::Center);

        shapes::Rectangle pill{};
        pill.RadiusX(9.5);
        pill.RadiusY(9.5);
        pill.StrokeThickness(1);
        pill.UseLayoutRounding(false);
        pill.Stroke(BrushFor(m_palette.StrokeStrong));
        pill.Fill(BrushFor(m_palette.Light ? seq::Rgba(0, 0, 0, 0.03) : seq::Rgba(255, 255, 255, 0.04)));
        chip.Children().Append(pill);

        controls::TextBlock value{};
        value.FontSize(11);
        value.Margin(xaml::Thickness{ 7, 0, 7, 1 });
        value.VerticalAlignment(xaml::VerticalAlignment::Center);
        value.HorizontalAlignment(xaml::HorizontalAlignment::Center);
        value.Foreground(BrushFor(m_palette.Text2));
        chip.Children().Append(value);
        place(chip, 3);

        m_laneChips.push_back(LaneChip{ track->Id, lane.Id, value });

        controls::Button add{};
        add.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SubtleSmallButtonStyle")).as<xaml::Style>());
        add.Width(24);
        add.Height(24);
        add.MinWidth(0);
        add.MinHeight(0);
        add.Padding(xaml::Thickness{ 0, 0, 0, 0 });
        add.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::FontIcon plus{};
        plus.Glyph(L"\uE710");
        plus.FontSize(11);
        add.Content(plus);

        auto const addName = res::FormatString(L"AddLaneToTrackFormat", track->Name);
        automation::AutomationProperties::SetName(add, addName);
        controls::ToolTipService::SetToolTip(add, winrt::box_value(addName));
        add.Click([weak = get_weak(), id = track->Id](auto&&, auto&&)
        {
            if (auto strong = weak.get()) { strong->ShowAddLaneDialog(id); }
        });

        place(add, 4);

        grid.Tapped([weak = get_weak(), id = track->Id](auto&&, input::TappedRoutedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                strong->SelectTrack(id);
                args.Handled(true);
            }
        });

        // Only the name renames, so a quick double click on the add button doesn't.
        info.DoubleTapped([weak = get_weak(), id = track->Id, laneId = lane.Id](auto&&, input::DoubleTappedRoutedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                strong->RenameLane(id, laneId);
                args.Handled(true);
            }
        });

        grid.RightTapped([weak = get_weak(), id = track->Id, laneId = lane.Id, element = winrt::make_weak(grid)](auto&&, input::RightTappedRoutedEventArgs const& args)
        {
            auto strong = weak.get();
            auto anchor = element.get();

            if (strong && anchor)
            {
                strong->ShowLaneRowMenu(id, laneId, std::nullopt, anchor, args.GetPosition(anchor));
                args.Handled(true);
            }
        });

        automation::AutomationProperties::SetName(grid, res::FormatString(L"LaneRowNameFormat", LaneName(lane), track->Name));
        return grid;
    }

    void MainWindow::UpdateLeds() noexcept
    {
        try
        {
            if (m_leds.empty())
            {
                return;
            }

            auto const playing = m_engine != nullptr && m_engine->IsPlaying();
            auto const lit = BrushFor(m_palette.Play);
            auto const dim = BrushFor(m_palette.Light ? seq::Rgba(0, 0, 0, 0.12) : seq::Rgba(255, 255, 255, 0.14));

            // A style that draws the lights reads whether each is on from its tag.
            auto const plated = HeaderLedsPlated();
            auto relight = false;

            for (auto const& [id, led] : m_leds)
            {
                auto on = false;

                if (playing)
                {
                    if (auto const track = seq::FindTrack(m_doc, id); track != nullptr && !track->Muted)
                    {
                        if (auto view = m_launchViews.find(id); view != m_launchViews.end() && view->second.Mode == seq::TrackPlayMode::Clip)
                        {
                            on = true;
                        }
                        else if (track->IsFolder)
                        {
                            on = false;
                        }
                        else
                        {
                            for (auto const& placement : track->Timeline)
                            {
                                if (m_position >= placement.Tick && m_position < placement.Tick + EffectiveLength(m_doc, placement))
                                {
                                    on = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (plated)
                {
                    if (winrt::unbox_value_or<bool>(led.Tag(), false) != on)
                    {
                        led.Tag(winrt::box_value(on));
                        relight = true;
                    }

                    continue;
                }

                auto const current = led.Fill().try_as<media::SolidColorBrush>();
                auto const want = on ? lit.Color() : dim.Color();

                if (current == nullptr || current.Color() != want)
                {
                    led.Fill(on ? lit : dim);
                }
            }

            if (relight)
            {
                m_headerPlates.Invalidate();
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::RebuildSceneHeaders() noexcept
    {
        try
        {
            m_headerPlates.Remove(L"scenes");
            SceneHeaderHost().Children().Clear();

            m_sceneHeaderPanel = controls::StackPanel{};
            m_sceneHeaderPanel.Orientation(controls::Orientation::Horizontal);
            m_sceneHeaderPanel.HorizontalAlignment(xaml::HorizontalAlignment::Left);

            // Scenes deleted, or a narrower launcher, can leave it scrolled past the last scene.
            auto const shown = m_scrollLauncherCanvas != nullptr ? m_scrollLauncherCanvas.ActualWidth() : 0.0;
            m_sceneScrollX = std::clamp(m_sceneScrollX, 0.0, std::max(0.0, static_cast<double>(m_doc.Scenes.size()) * SceneWidth + AddSceneWidth - shown));

            media::TranslateTransform shift{};
            shift.X(-m_sceneScrollX);
            m_sceneHeaderPanel.RenderTransform(shift);

            for (size_t scene = 0; scene < m_doc.Scenes.size(); ++scene)
            {
                auto const& value = m_doc.Scenes[scene];

                controls::Button button{};
                button.Width(SceneWidth);
                button.Height(32);
                button.MinHeight(0);
                button.Padding(xaml::Thickness{ 8, 0, 6, 0 });
                button.CornerRadius(xaml::CornerRadius{ 0, 0, 0, 0 });
                button.BorderThickness(xaml::Thickness{ 0, 0, 1, 0 });
                button.BorderBrush(BrushFor(m_palette.Divider));
                button.Background(BrushFor(winrt::Microsoft::UI::Colors::Transparent()));
                button.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);

                // seq.css .scene: the number and name, and the play triangle at the right.
                controls::Grid content{};
                content.ColumnSpacing(6);

                controls::ColumnDefinition labelColumn{};
                labelColumn.Width(Star());
                content.ColumnDefinitions().Append(labelColumn);

                controls::ColumnDefinition playColumn{};
                playColumn.Width(Auto());
                content.ColumnDefinitions().Append(playColumn);

                controls::TextBlock label{};
                label.FontSize(12);
                label.VerticalAlignment(xaml::VerticalAlignment::Center);
                label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);

                documents::Run number{};
                number.Text(winrt::hstring{ std::to_wstring(scene + 1) });
                number.Foreground(BrushFor(m_palette.Text3));
                label.Inlines().Append(number);

                if (!value.Name.empty())
                {
                    documents::Run text{};
                    text.Text(L"  " + value.Name);
                    label.Inlines().Append(text);
                }

                content.Children().Append(label);

                controls::FontIcon play{};
                play.Glyph(L"\uE768");
                play.FontSize(10);
                play.VerticalAlignment(xaml::VerticalAlignment::Center);
                play.Foreground(BrushFor(m_palette.Text2));
                controls::Grid::SetColumn(play, 1);
                content.Children().Append(play);

                button.Content(content);
                PlateScene(button, label, number, play);

                auto const accessible = value.Name.empty()
                    ? res::FormatString(L"LaunchSceneFormat", scene + 1)
                    : res::FormatString(L"LaunchNamedSceneFormat", scene + 1, value.Name);

                automation::AutomationProperties::SetName(button, accessible);
                controls::ToolTipService::SetToolTip(button, winrt::box_value(accessible));

                button.Click([weak = get_weak(), scene](auto&&, auto&&)
                {
                    if (auto strong = weak.get()) { strong->LaunchScene(scene); }
                });

                button.RightTapped([weak = get_weak(), scene, element = winrt::make_weak(button)](auto&&, input::RightTappedRoutedEventArgs const& args)
                {
                    auto strong = weak.get();
                    auto anchor = element.get();

                    if (!strong || !anchor)
                    {
                        return;
                    }

                    controls::MenuFlyout menu{};

                    controls::MenuFlyoutItem rename{};
                    rename.Text(res::GetString(L"MenuRenameScene"));
                    rename.Click([weak, scene](auto&&, auto&&)
                    {
                        if (auto window = weak.get(); window && scene < window->m_doc.Scenes.size())
                        {
                            window->ShowTextPrompt(res::GetString(L"RenameSceneTitle"), winrt::hstring{ window->m_doc.Scenes[scene].Name }, [weak, scene](std::wstring const& text)
                            {
                                if (auto inner = weak.get(); inner && scene < inner->m_doc.Scenes.size())
                                {
                                    auto scenes = inner->m_doc.Scenes;
                                    scenes[scene].Name = text.substr(0, 100);

                                    seq::ChangeList changes{};
                                    changes.push_back(seq::MakeScenesChange(inner->m_doc.Scenes, scenes));
                                    inner->m_doc.Scenes = std::move(scenes);
                                    inner->Commit(std::wstring{ res::GetString(L"UndoRenameScene") }, std::move(changes));
                                }
                            });
                        }
                    });
                    menu.Items().Append(rename);

                    controls::MenuFlyoutItem add{};
                    add.Text(res::GetString(L"MenuAddScene"));
                    add.Click([weak](auto&&, auto&&)
                    {
                        if (auto window = weak.get()) { window->AddScene(); }
                    });
                    menu.Items().Append(add);

                    controls::MenuFlyoutItem remove{};
                    remove.Text(res::GetString(L"MenuDeleteScene"));
                    remove.IsEnabled(strong->m_doc.Scenes.size() > 1);
                    remove.Click([weak, scene](auto&&, auto&&)
                    {
                        if (auto window = weak.get()) { window->DeleteScene(scene); }
                    });
                    menu.Items().Append(remove);

                    primitives::FlyoutShowOptions options{};
                    options.Position(args.GetPosition(anchor));
                    menu.ShowAt(anchor, options);
                    args.Handled(true);
                });

                m_sceneHeaderPanel.Children().Append(button);
            }

            controls::Button add{};
            add.Width(AddSceneWidth);
            add.Height(32);
            add.MinHeight(0);
            add.MinWidth(0);
            add.Padding(xaml::Thickness{ 0, 0, 0, 0 });
            add.CornerRadius(xaml::CornerRadius{ 0, 0, 0, 0 });
            add.BorderThickness(xaml::Thickness{ 0, 0, 0, 0 });
            add.Background(BrushFor(winrt::Microsoft::UI::Colors::Transparent()));

            controls::FontIcon plus{};
            plus.Glyph(L"\uE710");
            plus.FontSize(11);
            plus.Foreground(BrushFor(m_palette.Text3));
            add.Content(plus);

            automation::AutomationProperties::SetName(add, res::GetString(L"MenuAddScene"));
            controls::ToolTipService::SetToolTip(add, winrt::box_value(res::GetString(L"MenuAddScene")));
            add.Click([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->AddScene(); }
            });

            m_sceneHeaderPanel.Children().Append(add);
            SceneHeaderHost().Children().Append(m_sceneHeaderPanel);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to build the scene headers.")
    }

    void MainWindow::UpdateScrollBars() noexcept
    {
        try
        {
            m_updatingScrollBars = true;

            auto const viewport = ScrollViewportHeight();
            auto const content = m_layout.ScrollingHeight;
            auto const maximumY = std::max(0.0, content - viewport + 8.0);

            m_scrollY = std::clamp(m_scrollY, 0.0, maximumY);

            VerticalScroll().Maximum(maximumY);
            VerticalScroll().ViewportSize(viewport);
            VerticalScroll().LargeChange(std::max(1.0, viewport * 0.9));
            VerticalScroll().Value(m_scrollY);
            VerticalScroll().Visibility(maximumY > 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // Across, the timeline's own scroll bar (MainWindowTimelineBar.cpp) shows where it is.
            auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
            auto const view = TimelineView();
            auto const limits = TimelineLimits();
            m_scrollX = std::clamp(m_scrollX, 0.0, std::max(0.0, (limits.End - view.Span) * pixelsPerTick));
            m_timelineBar.Invalidate();

            m_updatingScrollBars = false;
        }
        catch (...)
        {
            m_updatingScrollBars = false;
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnVerticalScrollChanged(foundation::IInspectable const&, primitives::RangeBaseValueChangedEventArgs const& args)
    {
        if (m_updatingScrollBars)
        {
            return;
        }

        m_scrollY = args.NewValue();
        PositionHeaders();
        InvalidateArrange();
        InvalidateLaunchers();
    }



    _Use_decl_annotations_
    void MainWindow::SetZoom(double barWidth, double anchorX) noexcept
    {
        try
        {
            // Through the scroll bar's limits, so zooming out stops at the whole sequence.
            auto const tick = (anchorX + m_scrollX) / (m_barWidth / TicksPerFourFourBar);
            SetTimelineView(seq::ZoomAbout(TimelineView(), barWidth / m_barWidth, tick, TimelineLimits()));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to zoom the timeline.")
    }

    _Use_decl_annotations_
    void MainWindow::ScrollToTick(int64_t tick) noexcept
    {
        m_scrollX = std::max(0.0, static_cast<double>(tick) * (m_barWidth / TicksPerFourFourBar) - LaneWidth() * 0.1);
        UpdateScrollBars();
        InvalidateArrange();
    }

    void MainWindow::UpdatePlayhead() noexcept
    {
        try
        {
            UpdateLyricsBar();
            m_timelineBar.PlayheadMoved(m_position);

            auto const x = static_cast<double>(m_position) * (m_barWidth / TicksPerFourFourBar) - m_scrollX;
            auto const width = LaneWidth();

            if (x < -1.0 || x > width)
            {
                PlayheadLine().Visibility(xaml::Visibility::Collapsed);
                PlayheadMarker().Visibility(xaml::Visibility::Collapsed);
                return;
            }

            PlayheadLine().Visibility(xaml::Visibility::Visible);
            PlayheadMarker().Visibility(xaml::Visibility::Visible);

            PlayheadLine().Height(std::max(0.0, PlayheadLayer().ActualHeight()));
            controls::Canvas::SetLeft(PlayheadLine(), x - 0.75);
            controls::Canvas::SetTop(PlayheadLine(), 0);

            controls::Canvas::SetLeft(PlayheadMarker(), x - 5.5);
            controls::Canvas::SetTop(PlayheadMarker(), 24);
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- tracks

    _Use_decl_annotations_
    void MainWindow::SelectTrack(std::wstring const& trackId) noexcept
    {
        try
        {
            if (m_selectedTrackId == trackId)
            {
                // Already the one selected: only the others go.
                if (m_selectedTrackIds.size() > 1)
                {
                    m_selectedTrackIds.assign(1, trackId);
                    m_selectionAnchorId = trackId;
                    RefreshHeaderBackgrounds();
                }

                return;
            }

            m_selectedTrackId = trackId;
            m_selectedPlacement = SIZE_MAX;
            m_selectedTrackIds.clear();

            if (!trackId.empty())
            {
                m_selectedTrackIds.push_back(trackId);
            }

            m_selectionAnchorId = trackId;

            // In place: rebuilding the headers here would remove the one being double-clicked.
            RefreshHeaderBackgrounds();

            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to select a track.")
    }

    _Use_decl_annotations_
    void MainWindow::ToggleFolder(std::wstring const& trackId) noexcept
    {
        try
        {
            auto const folder = seq::FindTrack(m_doc, trackId);

            if (folder == nullptr || !folder->IsFolder)
            {
                return;
            }

            // Kept in the file so the sequence opens the way it was left, but not an edit to undo.
            folder->Open = !folder->Open;
            m_dirty = true;
            ++m_version;

            RebuildLayout();
            UpdateTitle();
            UpdateSavedState();

            if (m_autosaveTimer != nullptr && !m_path.empty() && !m_readOnly)
            {
                m_autosaveTimer.Stop();
                m_autosaveTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open or close a folder.")
    }

    _Use_decl_annotations_
    void MainWindow::ToggleTrackFlag(std::wstring const& trackId, wchar_t flag, bool value) noexcept
    {
        try
        {
            if (flag == L'R')
            {
                if (value)
                {
                    m_armed.insert(trackId);
                }
                else
                {
                    m_armed.erase(trackId);
                }

                UpdateEchoRoutes();
                PrepareConnectionsAsync();
                RebuildHeaders();
                InvalidateLaunchers();

                if (value)
                {
                    if (auto const track = seq::FindTrack(m_doc, trackId); track != nullptr && track->Source.Endpoint.IsEmpty())
                    {
                        ShowMessage(res::GetString(L"ArmedWithoutSource"));
                    }
                }

                return;
            }

            auto const name = std::wstring{ res::GetString(flag == L'M' ? (value ? L"UndoMute" : L"UndoUnmute") : (value ? L"UndoSolo" : L"UndoUnsolo")) };

            EditTracks(name, [&](seq::Sequence& doc)
            {
                if (auto track = seq::FindTrack(doc, trackId); track != nullptr)
                {
                    if (flag == L'M')
                    {
                        track->Muted = value;
                    }
                    else
                    {
                        track->Soloed = value;
                    }
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change a track.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowTrackMenu(std::wstring const& trackId, xaml::UIElement const& anchor, foundation::Point point)
    {
        auto const track = seq::FindTrack(m_doc, trackId);

        if (track == nullptr)
        {
            return;
        }

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action, bool enabled = true)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);
            item.IsEnabled(enabled);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        if (!track->IsFolder)
        {
            add(res::GetString(L"MenuTrackSetup"), L"\uE713", [weak, trackId]() { if (auto s = weak.get()) { s->ShowInspector(trackId); } });
            add(res::GetString(L"MenuAddLane"), L"\uE9D2", [weak, trackId]() { if (auto s = weak.get()) { s->ShowAddLaneDialog(trackId); } });

            if (!track->Lanes.empty())
            {
                add(res::GetString(track->LanesOpen ? L"MenuHideAutomation" : L"MenuShowAutomation"), nullptr, [weak, trackId]()
                {
                    if (auto s = weak.get()) { s->ToggleLanes(trackId); }
                });
            }
        }

        add(res::GetString(L"MenuRename"), L"\uE8AC", [weak, trackId]() { if (auto s = weak.get()) { s->BeginRenameTrack(trackId); } });

        add(res::GetString(track->Pinned ? L"MenuUnpin" : L"MenuPin"), L"\uE718", [weak, trackId]()
        {
            if (auto s = weak.get())
            {
                s->EditTracks(std::wstring{ res::GetString(L"UndoPin") }, [&](seq::Sequence& doc)
                {
                    if (auto t = seq::FindTrack(doc, trackId); t != nullptr)
                    {
                        t->Pinned = !t->Pinned;
                    }
                });
            }
        });

        // Compact rows are one line of the name tall. Every selected track changes when this is
        // one of them.
        {
            auto const targets = IsTrackSelected(trackId) ? SelectedTrackIds() : std::vector<std::wstring>{ trackId };
            auto allCompact{ true };
            auto anyChanged{ false };

            for (auto const& id : targets)
            {
                if (auto const target = seq::FindTrack(m_doc, id); target != nullptr)
                {
                    allCompact = allCompact && target->Height == seq::MinimumTrackRowHeight;
                    anyChanged = anyChanged || target->Height != 0;
                }
            }

            controls::ToggleMenuFlyoutItem compact{};
            compact.Text(res::GetString(targets.size() > 1 ? L"MenuCompactTracks" : L"MenuCompact"));
            compact.IsChecked(allCompact);
            compact.Click([weak, targets, allCompact](auto&&, auto&&)
            {
                if (auto s = weak.get())
                {
                    s->SetTracksHeight(targets, allCompact ? uint16_t{ 0 } : seq::MinimumTrackRowHeight);
                }
            });
            menu.Items().Append(compact);

            if (anyChanged && !allCompact)
            {
                add(res::GetString(L"MenuStandardHeight"), nullptr, [weak, targets]()
                {
                    if (auto s = weak.get()) { s->SetTracksHeight(targets, 0); }
                });
            }
        }

        // Colors
        {
            controls::MenuFlyoutSubItem colors{};
            colors.Text(res::GetString(L"MenuColor"));

            controls::FontIcon icon{};
            icon.Glyph(L"\uE790");
            colors.Icon(icon);

            auto const& swatches = seq::TrackColorSwatches();

            for (size_t i = 0; i < swatches.size(); ++i)
            {
                auto const swatch = swatches[i];

                controls::MenuFlyoutItem item{};
                item.Text(res::GetString(std::wstring{ L"SwatchName" } + std::to_wstring(i)));

                controls::FontIcon dot{};
                dot.Glyph(L"\uEA3B");
                dot.Foreground(BrushFor(seq::FromRgb(swatch)));
                item.Icon(dot);

                item.Click([weak, trackId, swatch](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->EditTracks(std::wstring{ res::GetString(L"UndoColor") }, [&](seq::Sequence& doc)
                        {
                            if (auto t = seq::FindTrack(doc, trackId); t != nullptr)
                            {
                                t->Color = swatch;
                            }
                        });
                    }
                });

                colors.Items().Append(item);
            }

            menu.Items().Append(colors);
        }

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        add(res::GetString(L"MenuMoveUp"), L"\uE74A", [weak, trackId]()
        {
            if (auto s = weak.get())
            {
                s->EditTracks(std::wstring{ res::GetString(L"UndoMoveTrack") }, [&](seq::Sequence& doc) { seq::MoveTrackBy(doc, trackId, -1); });
            }
        });

        add(res::GetString(L"MenuMoveDown"), L"\uE74B", [weak, trackId]()
        {
            if (auto s = weak.get())
            {
                s->EditTracks(std::wstring{ res::GetString(L"UndoMoveTrack") }, [&](seq::Sequence& doc) { seq::MoveTrackBy(doc, trackId, 1); });
            }
        });

        // Into a folder, or out to the top level.
        {
            controls::MenuFlyoutSubItem move{};
            move.Text(res::GetString(L"MenuMoveToFolder"));

            controls::FontIcon icon{};
            icon.Glyph(L"\uE8DE");
            move.Icon(icon);

            auto const parent = seq::ParentFolder(m_doc, trackId);

            if (parent != nullptr)
            {
                controls::MenuFlyoutItem top{};
                top.Text(res::GetString(L"MenuTopLevel"));
                top.Click([weak, trackId](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->EditTracks(std::wstring{ res::GetString(L"UndoMoveTrack") }, [&](seq::Sequence& doc) { seq::MoveTrackToFolder(doc, trackId, L""); });
                    }
                });
                move.Items().Append(top);
            }

            seq::ForEachTrack(m_doc, [&](seq::Track const& candidate, size_t)
            {
                if (candidate.IsFolder && candidate.Id != trackId && !seq::IsInside(m_doc, candidate.Id, trackId) &&
                    (parent == nullptr || parent->Id != candidate.Id))
                {
                    controls::MenuFlyoutItem item{};
                    item.Text(winrt::hstring{ candidate.Name });
                    item.Click([weak, trackId, folderId = candidate.Id](auto&&, auto&&)
                    {
                        if (auto s = weak.get())
                        {
                            s->EditTracks(std::wstring{ res::GetString(L"UndoMoveTrack") }, [&](seq::Sequence& doc) { seq::MoveTrackToFolder(doc, trackId, folderId); });
                        }
                    });
                    move.Items().Append(item);
                }

                return true;
            });

            move.IsEnabled(move.Items().Size() > 0);
            menu.Items().Append(move);
        }

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        add(res::GetString(track->IsFolder ? L"MenuDeleteFolder" : L"MenuDeleteTrack"), L"\uE74D", [weak, trackId]()
        {
            if (auto s = weak.get()) { s->DeleteTrack(trackId); }
        });

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    _Use_decl_annotations_
    void MainWindow::AddTrack(bool folder) noexcept
    {
        try
        {
            auto const stem = std::wstring{ res::GetString(folder ? L"DefaultFolderName" : L"DefaultTrackName") };
            auto track = seq::MakeTrack(m_doc, seq::NextName(m_doc, stem), folder);
            track.Slots.resize(m_doc.Scenes.size());

            if (!folder)
            {
                // Plays to what the selected track plays to, on the next free channel, so adding
                // a part for the same synth is one click.
                seq::EndpointRef endpoint{};
                uint8_t group{ 0 };

                if (auto const selected = seq::FindTrack(m_doc, m_selectedTrackId); selected != nullptr && !selected->IsFolder && !selected->Destination.Endpoint.IsEmpty())
                {
                    endpoint = selected->Destination.Endpoint;
                    group = selected->Destination.Group;
                }
                else if (auto const synth = m_directory->Resolve(seq::EndpointRef{ L"", m_directory->SynthEndpointId() }); synth.has_value())
                {
                    endpoint = seq::EndpointDirectory::MakeRef(synth->Live);
                }

                track.Destination.Endpoint = endpoint;
                track.Destination.Group = group;
                track.Destination.Channel = endpoint.IsEmpty() ? 0 : seq::NextFreeChannel(m_doc, endpoint, group);
            }

            auto const id = track.Id;
            auto const after = m_selectedTrackId;

            EditTracks(std::wstring{ res::GetString(folder ? L"UndoAddFolder" : L"UndoAddTrack") }, [&](seq::Sequence& doc)
            {
                // A new track goes after the selected one; inside a folder that's selected and open,
                // unless that's deeper than a sequence file keeps.
                seq::InsertTrackAfter(doc, after, std::move(track));
            });

            SelectTrack(id);

            if (auto const row = m_layout.FindRow(id); row != nullptr && !row->Pinned)
            {
                if (row->Top < m_scrollY || row->Top + row->Height > m_scrollY + ScrollViewportHeight())
                {
                    m_scrollY = std::max(0.0, row->Top + row->Height - ScrollViewportHeight() + 8.0);
                    UpdateScrollBars();
                    PositionHeaders();
                    InvalidateArrange();
                    InvalidateLaunchers();
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a track.")
    }

    _Use_decl_annotations_
    void MainWindow::DeleteTrack(std::wstring const& trackId) noexcept
    {
        try
        {
            auto const before = m_doc.Tracks;

            if (!seq::RemoveTrack(m_doc, trackId).has_value())
            {
                return;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& clip : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
            }

            m_armed.erase(trackId);

            if (m_inspectorTrackId == trackId)
            {
                HideInspector();
            }

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            Commit(std::wstring{ res::GetString(L"UndoDeleteTrack") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete a track.")
    }

    _Use_decl_annotations_
    void MainWindow::BeginRenameTrack(std::wstring const& trackId) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr)
            {
                return;
            }

            ShowTextPrompt(res::GetString(track->IsFolder ? L"RenameFolderTitle" : L"RenameTrackTitle"), winrt::hstring{ track->Name },
                [weak = get_weak(), trackId](std::wstring const& text)
            {
                auto strong = weak.get();

                if (!strong || text.empty())
                {
                    return;
                }

                strong->EditTracks(std::wstring{ res::GetString(L"UndoRenameTrack") }, [&](seq::Sequence& doc)
                {
                    if (auto t = seq::FindTrack(doc, trackId); t != nullptr)
                    {
                        t->Name = text.substr(0, 100);
                    }
                });
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to rename a track.")
    }

    _Use_decl_annotations_
    void MainWindow::OnAddTrackClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AddTrack(false);
    }

    _Use_decl_annotations_
    void MainWindow::OnAddFolderClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        AddTrack(true);
    }

    _Use_decl_annotations_
    void MainWindow::SetLauncherVisible(bool visible) noexcept
    {
        try
        {
            SceneHeaderHost().Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            seq::AppSettings::Current().ShowLauncher(visible);
            ApplyArrangeColumns();
            InvalidateLaunchers();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show or hide the clip launcher.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherToggleClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        SetLauncherVisible(LauncherToggle().IsChecked().GetBoolean());
    }

    // ---------------------------------------------------------------- the timeline

    _Use_decl_annotations_
    MainWindow::LaneHit MainWindow::HitTestLane(bool pinned, foundation::Point point) noexcept
    {
        LaneHit hit{};

        try
        {
            auto const row = pinned ? m_layout.PinnedRowAt(point.Y) : m_layout.ScrollingRowAt(point.Y + m_scrollY);

            if (row == nullptr)
            {
                return hit;
            }

            hit.Row = row;

            auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
            hit.Tick = std::max<int64_t>(0, static_cast<int64_t>(std::floor((point.X + m_scrollX) / pixelsPerTick)));

            if (row->Kind != seq::ArrangeRowKind::Track)
            {
                return hit;
            }

            auto const top = static_cast<float>(row->Top - (pinned ? 0.0 : m_scrollY));

            // A launched clip covers the lane, with a button to go back.
            if (auto view = m_launchViews.find(row->TrackId); view != m_launchViews.end() && view->second.Mode == seq::TrackPlayMode::Clip)
            {
                auto const bounds = m_renderer.BackToTimelineButtonBounds(top, static_cast<float>(row->Height));

                if (point.X >= bounds.X && point.X <= bounds.X + bounds.Width && point.Y >= bounds.Y && point.Y <= bounds.Y + bounds.Height)
                {
                    hit.OnBackButton = true;
                }

                return hit;
            }

            auto const track = seq::FindTrack(m_doc, row->TrackId);

            if (track == nullptr)
            {
                return hit;
            }

            // Last placement wins, which is the one drawn on top.
            for (size_t i = track->Timeline.size(); i-- > 0;)
            {
                auto const& placement = track->Timeline[i];
                auto const left = static_cast<double>(placement.Tick) * pixelsPerTick - m_scrollX;
                auto const right = static_cast<double>(placement.Tick + EffectiveLength(m_doc, placement)) * pixelsPerTick - m_scrollX;

                if (point.X >= left && point.X < right)
                {
                    hit.Placement = i;
                    hit.OnRightEdge = right - point.X <= ResizeEdge && right - left > ResizeEdge * 2;
                    break;
                }
            }
        }
        catch (...)
        {
        }

        return hit;
    }

    _Use_decl_annotations_
    void MainWindow::OnLanePressed(bool pinned, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const point = args.GetCurrentPoint(canvas);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            auto const hit = HitTestLane(pinned, point.Position());

            if (hit.Row == nullptr)
            {
                m_selectedPlacement = SIZE_MAX;
                ClearAutomationSelection();
                InvalidateArrange();
                return;
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Lane)
            {
                OnAutomationPressed(pinned, *hit.Row, args);
                return;
            }

            auto const rowTop = static_cast<float>(hit.Row->Top - (pinned ? 0.0 : m_scrollY));

            // Tags sit on top of everything else in their row.
            if (hit.Row->Kind == seq::ArrangeRowKind::Tempo || (hit.Row->Kind == seq::ArrangeRowKind::Track && !hit.OnBackButton))
            {
                if (auto const tag = HitTestTag(*hit.Row, rowTop, point.Position()); tag.Valid())
                {
                    BeginTagDrag(pinned, tag, args);
                    return;
                }
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Tempo)
            {
                // The band along the top holds tags, and the tempo line is under it. A double click
                // in the band adds a tag.
                if (point.Position().Y < rowTop + seq::TempoTagBandHeight)
                {
                    ClearAutomationSelection();
                    m_selectedPlacement = SIZE_MAX;
                    InvalidateArrange();
                    args.Handled(true);
                    return;
                }

                if (OnTempoPressed(pinned, *hit.Row, args))
                {
                    return;
                }
            }

            ClearAutomationSelection();

            if (hit.OnBackButton)
            {
                if (m_engine != nullptr)
                {
                    m_engine->ReturnToTimeline(hit.Row->TrackId, LaunchQuantize());
                }

                args.Handled(true);
                return;
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Track || hit.Row->Kind == seq::ArrangeRowKind::Folder)
            {
                SelectTrack(hit.Row->TrackId);
            }

            if (hit.Placement == SIZE_MAX)
            {
                m_selectedPlacement = SIZE_MAX;
                InvalidateArrange();
                return;
            }

            auto const track = seq::FindTrack(m_doc, hit.Row->TrackId);

            if (track == nullptr)
            {
                return;
            }

            m_selectedPlacement = hit.Placement;

            auto const& placement = track->Timeline[hit.Placement];

            if (hit.OnRightEdge)
            {
                m_laneDrag = LaneDrag::Resize;
                m_lanePinned = pinned;
                m_dragStart = point.Position();
                m_dragOriginalLength = EffectiveLength(m_doc, placement);
                m_dragLength = m_dragOriginalLength;
                m_dragTracksBefore = m_doc.Tracks;
            }
            else
            {
                // Picked up: it can go anywhere on the timeline or into a launcher slot.
                ClipDrag drag{};
                drag.Pressed = true;
                drag.Pinned = pinned;
                drag.Start = args.GetCurrentPoint(ArrangeGrid()).Position();
                drag.ClipId = placement.ClipId;
                drag.FromTrackId = track->Id;
                drag.FromPlacement = hit.Placement;
                drag.GrabOffset = std::max<int64_t>(0, hit.Tick - placement.Tick);
                drag.Length = placement.Length;
                m_clipDrag = std::move(drag);
            }

            canvas.CapturePointer(args.Pointer());

            if (m_editorClipId != placement.ClipId)
            {
                OpenClipInEditor(placement.ClipId, track->Id);
            }

            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a click on the timeline.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaneMoved(bool pinned, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (m_pointDrag.What != PointDrag::Kind::None)
            {
                if (pinned == m_pointDrag.Pinned)
                {
                    OnPointDragMoved(args);
                }

                return;
            }

            if (m_tagDrag.Active)
            {
                if (pinned == m_tagDrag.Pinned)
                {
                    OnTagDragMoved(args);
                }

                return;
            }

            if (m_clipDrag.Pressed)
            {
                if (pinned != m_clipDrag.Pinned)
                {
                    return;
                }

                auto const point = args.GetCurrentPoint(ArrangeGrid()).Position();

                if (!m_clipDrag.Active)
                {
                    auto const dx = static_cast<double>(point.X) - static_cast<double>(m_clipDrag.Start.X);
                    auto const dy = static_cast<double>(point.Y) - static_cast<double>(m_clipDrag.Start.Y);

                    if (std::hypot(dx, dy) < DragThreshold)
                    {
                        return;
                    }

                    m_clipDrag.Active = true;
                    StartDragScroll();
                }

                m_dragPoint = point;
                m_dragCopy = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Control) == winrt::Windows::System::VirtualKeyModifiers::Control;
                m_dragKeepTrack = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Shift) == winrt::Windows::System::VirtualKeyModifiers::Shift;
                UpdateClipDrop(m_dragPoint, m_dragCopy, m_dragKeepTrack);
                args.Handled(true);
                return;
            }

            if (m_laneDrag == LaneDrag::None)
            {
                // Pointing at a clip's end shows that it can be dragged to change its length.
                auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
                auto const hit = HitTestLane(pinned, args.GetCurrentPoint(canvas).Position());
                auto const sizing = hit.OnRightEdge;
                auto& shown = m_laneCursorSizing[pinned ? 0 : 1];

                if (shown != static_cast<int8_t>(sizing))
                {
                    shown = static_cast<int8_t>(sizing);

                    if (auto const element = canvas.try_as<xaml::IUIElementProtected>())
                    {
                        element.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(sizing
                            ? winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast
                            : winrt::Microsoft::UI::Input::InputSystemCursorShape::Arrow));
                    }
                }

                return;
            }

            if (pinned != m_lanePinned)
            {
                return;
            }

            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const position = args.GetCurrentPoint(canvas).Position();
            auto const dx = static_cast<double>(position.X) - static_cast<double>(m_dragStart.X);

            if (std::abs(dx) < DragThreshold && m_dragLength == m_dragOriginalLength)
            {
                return;
            }

            auto const ticks = static_cast<int64_t>(std::llround(dx / (m_barWidth / TicksPerFourFourBar)));

            // Lengths change by whole beats unless snap is finer, so clips stay on the grid.
            auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);
            auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

            if (track == nullptr || m_selectedPlacement >= track->Timeline.size())
            {
                return;
            }

            auto& placement = track->Timeline[m_selectedPlacement];
            m_dragLength = std::max<int64_t>(grid, seq::SnapTick(m_dragOriginalLength + ticks, grid));
            placement.Length = m_dragLength;

            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaneReleased(bool pinned, input::PointerRoutedEventArgs const&)
    {
        try
        {
            if (m_pointDrag.What != PointDrag::Kind::None)
            {
                // Finished first: letting go of the capture comes straight back here.
                OnPointDragReleased();
                (pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas).ReleasePointerCaptures();
                return;
            }

            if (m_tagDrag.Active)
            {
                FinishTagDrag();
                return;
            }

            if (m_clipDrag.Pressed)
            {
                FinishClipDrag(true);
                return;
            }

            if (m_laneDrag == LaneDrag::None)
            {
                return;
            }

            m_laneDrag = LaneDrag::None;

            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            canvas.ReleasePointerCaptures();

            auto const changed = m_dragLength != m_dragOriginalLength;

            if (!changed)
            {
                m_dragTracksBefore.clear();
                return;
            }

            // Keep the timeline in order, and the moved placement selected.
            if (auto track = seq::FindTrack(m_doc, m_selectedTrackId); track != nullptr && m_selectedPlacement < track->Timeline.size())
            {
                auto const moved = track->Timeline[m_selectedPlacement];
                std::stable_sort(track->Timeline.begin(), track->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });

                for (size_t i = 0; i < track->Timeline.size(); ++i)
                {
                    if (track->Timeline[i] == moved)
                    {
                        m_selectedPlacement = i;
                        break;
                    }
                }
            }

            // The tracks as they were when the drag began, and as they are now.
            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(std::move(m_dragTracksBefore), m_doc.Tracks));
            m_dragTracksBefore.clear();

            Commit(std::wstring{ res::GetString(L"UndoResizeClip") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish dragging a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaneWheel(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            using winrt::Windows::System::VirtualKeyModifiers;

            auto const point = args.GetCurrentPoint(m_scrollLaneCanvas);
            auto const delta = point.Properties().MouseWheelDelta();
            auto const modifiers = args.KeyModifiers();
            auto const control = (modifiers & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control;
            auto const shift = (modifiers & VirtualKeyModifiers::Shift) == VirtualKeyModifiers::Shift;

            if (control)
            {
                // A touchpad's pinch comes as Ctrl and the wheel, in small steps.
                m_timelineBar.ForgetFit();
                SetZoom(m_barWidth * std::pow(1.25, static_cast<double>(delta) / 120.0), std::clamp(static_cast<double>(point.Position().X), 0.0, LaneWidth()));
            }
            else if (shift || point.Properties().IsHorizontalMouseWheel())
            {
                auto const direction = point.Properties().IsHorizontalMouseWheel() ? 1.0 : -1.0;
                m_scrollX = std::max(0.0, m_scrollX + direction * static_cast<double>(delta) * 0.5);
                UpdateScrollBars();
                InvalidateArrange();
            }
            else
            {
                m_scrollY = m_scrollY - static_cast<double>(delta) * 0.5;
                UpdateScrollBars();
                PositionHeaders();
                InvalidateArrange();
                InvalidateLaunchers();
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to scroll the timeline.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherWheel(input::PointerRoutedEventArgs const& args, bool sceneNames)
    {
        try
        {
            using winrt::Windows::System::VirtualKeyModifiers;

            auto const properties = args.GetCurrentPoint(m_scrollLauncherCanvas).Properties();
            auto const modifiers = args.KeyModifiers();
            auto const control = (modifiers & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control;
            auto const shift = (modifiers & VirtualKeyModifiers::Shift) == VirtualKeyModifiers::Shift;

            // Sideways, the launcher goes through its scenes; up and down, it goes with the tracks.
            // Over the scene names there's only sideways.
            if (!control && (shift || sceneNames || properties.IsHorizontalMouseWheel()))
            {
                auto const direction = properties.IsHorizontalMouseWheel() ? 1.0 : -1.0;
                ScrollScenes(m_sceneScrollX + direction * static_cast<double>(properties.MouseWheelDelta()) * 0.5);
                args.Handled(true);
                return;
            }

            OnLaneWheel(args);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to scroll the clip launcher.")
    }

    _Use_decl_annotations_
    void MainWindow::ScrollScenes(double x) noexcept
    {
        try
        {
            auto const shown = m_scrollLauncherCanvas != nullptr ? m_scrollLauncherCanvas.ActualWidth() : 0.0;
            auto const furthest = std::max(0.0, static_cast<double>(m_doc.Scenes.size()) * SceneWidth + AddSceneWidth - shown);
            auto const scrolled = std::clamp(x, 0.0, furthest);

            if (scrolled == m_sceneScrollX)
            {
                return;
            }

            m_sceneScrollX = scrolled;

            if (m_sceneHeaderPanel != nullptr)
            {
                if (auto const moved = m_sceneHeaderPanel.RenderTransform().try_as<media::TranslateTransform>())
                {
                    moved.X(-m_sceneScrollX);
                }
            }

            // Moving them isn't a layout, so their plates are drawn again here.
            m_headerPlates.Invalidate();
            InvalidateLaunchers();
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowScene(size_t scene) noexcept
    {
        try
        {
            auto const shown = m_scrollLauncherCanvas != nullptr ? m_scrollLauncherCanvas.ActualWidth() : 0.0;
            auto const left = static_cast<double>(scene) * SceneWidth;

            if (left < m_sceneScrollX)
            {
                ScrollScenes(left);
            }
            else if (left + SceneWidth > m_sceneScrollX + shown)
            {
                ScrollScenes(left + SceneWidth - shown);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnLaneDoubleTapped(bool pinned, input::DoubleTappedRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const position = args.GetPosition(canvas);
            auto const hit = HitTestLane(pinned, position);

            if (hit.Row == nullptr || hit.OnBackButton)
            {
                return;
            }

            auto const top = static_cast<float>(hit.Row->Top - (pinned ? 0.0 : m_scrollY));
            auto const height = static_cast<float>(hit.Row->Height);

            // A double click on a point that was already there deletes it. One whose first press
            // added the point keeps it.
            auto const justAdded = std::chrono::steady_clock::now() - m_pointAddedAt < std::chrono::milliseconds{ ::GetDoubleClickTime() + 100 };

            if (hit.Row->Kind == seq::ArrangeRowKind::Lane)
            {
                args.Handled(true);

                auto const track = seq::FindTrack(m_doc, hit.Row->TrackId);
                auto const lane = track != nullptr ? seq::FindLane(track->Lanes, hit.Row->LaneId) : nullptr;

                if (!justAdded && lane != nullptr)
                {
                    if (auto const index = HitTestLanePoint(*lane, top, height, position); index != SIZE_MAX)
                    {
                        ClearAutomationSelection();
                        m_selectedLaneTrackId = hit.Row->TrackId;
                        m_selectedLaneId = hit.Row->LaneId;
                        m_selectedLanePoint = index;
                        DeleteSelectedAutomationPoint();
                    }
                }

                return;
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Tempo || hit.Row->Kind == seq::ArrangeRowKind::Track)
            {
                if (auto const tag = HitTestTag(*hit.Row, top, position); tag.Valid())
                {
                    args.Handled(true);
                    RenameTag(tag);
                    return;
                }
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Lyrics)
            {
                args.Handled(true);
                ShowLyricsEditor();
                return;
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Tempo)
            {
                args.Handled(true);

                if (position.Y < top + seq::TempoTagBandHeight)
                {
                    AddTagAt(std::wstring{}, seq::SnapTick(hit.Tick, std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote)));
                    return;
                }

                auto const point = HitTestTempoPoint(top, height, position);

                if (point == SIZE_MAX || justAdded)
                {
                    return;
                }

                // The first tempo can't go, so a double click there asks for a new one instead.
                if (point == 0)
                {
                    PromptForTempo(m_doc.Tempo[point].Tick, point);
                    return;
                }

                ClearAutomationSelection();
                m_selectedTempoPoint = point;
                DeleteSelectedAutomationPoint();
                return;
            }

            if (hit.Row->Kind != seq::ArrangeRowKind::Track)
            {
                return;
            }

            if (hit.Placement == SIZE_MAX)
            {
                CreateClipAt(hit.Row->TrackId, hit.Tick);
            }
            else if (m_editorCanvas != nullptr)
            {
                ShowMixer(false);
                m_editorCanvas.Focus(xaml::FocusState::Programmatic);
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a double click on the timeline.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLaneRightTapped(bool pinned, input::RightTappedRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const position = args.GetPosition(canvas);
            auto const hit = HitTestLane(pinned, position);

            if (hit.Row == nullptr)
            {
                return;
            }

            if (hit.Row->Kind == seq::ArrangeRowKind::Tempo || (hit.Row->Kind == seq::ArrangeRowKind::Track && !hit.OnBackButton))
            {
                auto const top = static_cast<float>(hit.Row->Top - (pinned ? 0.0 : m_scrollY));

                if (auto const tag = HitTestTag(*hit.Row, top, position); tag.Valid())
                {
                    ShowTagMenu(tag, canvas, position);
                    args.Handled(true);
                    return;
                }
            }

            if ((hit.Row->Kind == seq::ArrangeRowKind::Lane || hit.Row->Kind == seq::ArrangeRowKind::Tempo) && OnAutomationRightTapped(pinned, *hit.Row, args))
            {
                return;
            }

            if (hit.Placement != SIZE_MAX)
            {
                SelectTrack(hit.Row->TrackId);
                m_selectedPlacement = hit.Placement;
                InvalidateArrange();
                ShowPlacementMenu(hit.Row->TrackId, hit.Placement, canvas, position);
            }
            else
            {
                ShowLaneMenu(*hit.Row, hit.Tick, canvas, position);
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the timeline menu.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowPlacementMenu(std::wstring const& trackId, size_t placement, xaml::UIElement const& anchor, foundation::Point point)
    {
        auto const track = seq::FindTrack(m_doc, trackId);

        if (track == nullptr || placement >= track->Timeline.size())
        {
            return;
        }

        auto const clipId = track->Timeline[placement].ClipId;
        auto const uses = seq::CountClipUses(m_doc, clipId);

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action, bool enabled = true)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);
            item.IsEnabled(enabled);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        add(res::GetString(L"MenuEditClip"), L"\uE70F", [weak, clipId, trackId]() { if (auto s = weak.get()) { s->ShowMixer(false); s->OpenClipInEditor(clipId, trackId); } });
        add(res::GetString(L"MenuRenameClip"), L"\uE8AC", [weak, clipId]() { if (auto s = weak.get()) { s->RenameClip(clipId); } });
        add(res::GetString(L"MenuDuplicateClip"), L"\uE8C8", [weak]() { if (auto s = weak.get()) { s->DuplicateSelectedPlacement(); } });
        add(res::GetString(L"MenuMakeUnique"), L"\uE71B", [weak, trackId, placement]() { if (auto s = weak.get()) { s->MakePlacementUnique(trackId, placement); } }, uses > 1);

        menu.Items().Append(controls::MenuFlyoutSeparator{});
        add(res::GetString(L"MenuDeleteClip"), L"\uE74D", [weak]() { if (auto s = weak.get()) { s->DeleteSelectedPlacement(); } });

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    _Use_decl_annotations_
    void MainWindow::ShowLaneMenu(seq::ArrangeRow const& row, int64_t tick, xaml::UIElement const& anchor, foundation::Point point)
    {
        controls::MenuFlyout menu{};
        auto weak = get_weak();
        auto const trackId = row.TrackId;
        auto const snapped = seq::SnapTick(tick, std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote));

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        // A new clip, pattern or generator shows the same symbol its slot will (design section 27).
        auto const addKind = [&menu](winrt::hstring const& text, seq::ClipIcon kind, std::function<void()> action)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);
            item.Icon(seq::MakeClipIcon(kind));
            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        if (row.Kind == seq::ArrangeRowKind::Track)
        {
            addKind(res::GetString(L"MenuNewClipHere"), seq::ClipIcon::Notes, [weak, trackId, tick]() { if (auto s = weak.get()) { s->CreateClipAt(trackId, tick); } });
            addKind(res::GetString(L"MenuNewDrumPatternHere"), seq::ClipIcon::DrumPattern, [weak, trackId, tick]() { if (auto s = weak.get()) { s->CreateClipAt(trackId, tick, seq::PatternMode::Drums); } });
            addKind(res::GetString(L"MenuNewMelodicPatternHere"), seq::ClipIcon::MelodicPattern, [weak, trackId, tick]() { if (auto s = weak.get()) { s->CreateClipAt(trackId, tick, seq::PatternMode::Melodic); } });

            // Every generator, in groups, with the symbol its clip will have (design section 28).
            menu.Items().Append(MakeNewGeneratorMenu(res::GetString(L"MenuNewGeneratorHere"), [weak, trackId, tick](seq::GeneratorType type)
            {
                if (auto s = weak.get()) { s->CreateClipAt(trackId, tick, std::nullopt, type); }
            }));
        }

        if (row.Kind == seq::ArrangeRowKind::Track || row.Kind == seq::ArrangeRowKind::Tempo)
        {
            add(res::GetString(L"MenuAddTagHere"), L"\uE8EC", [weak, trackId, snapped]() { if (auto s = weak.get()) { s->AddTagAt(trackId, snapped); } });
        }

        if (row.Kind == seq::ArrangeRowKind::Tempo)
        {
            AddTempoMenuItems(menu, tick);
        }

        if (row.Kind == seq::ArrangeRowKind::Lyrics)
        {
            AddLyricsMenuItems(menu);
        }

        add(res::GetString(L"MenuPlayFromHere"), L"\uE768", [weak, snapped]() { if (auto s = weak.get()) { s->Play(snapped); } });

        if (menu.Items().Size() == 0)
        {
            return;
        }

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    // ---------------------------------------------------------------- the ruler

    _Use_decl_annotations_
    void MainWindow::OnRulerPressed(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const point = args.GetCurrentPoint(m_rulerCanvas);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            m_rulerDragging = true;
            m_rulerMoved = false;
            m_rulerPressX = point.Position().X;

            // On the loop: its ends change its length, and its middle moves it.
            m_loopGrab = HitTestLoop(point.Position());
            m_loopGrabStart = m_loopStart;
            m_loopGrabEnd = m_loopEnd;

            auto const tick = std::max<int64_t>(0, static_cast<int64_t>((point.Position().X + m_scrollX) / (m_barWidth / TicksPerFourFourBar)));
            m_loopDragStart = seq::SnapTick(tick, seq::TicksPerQuarterNote);

            m_rulerCanvas.CapturePointer(args.Pointer());
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a click on the ruler.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRulerMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_rulerDragging)
            {
                // Pointing at the loop shows what dragging it does.
                auto const grab = HitTestLoop(args.GetCurrentPoint(m_rulerCanvas).Position());

                if (auto const element = m_rulerCanvas.try_as<xaml::IUIElementProtected>())
                {
                    using winrt::Microsoft::UI::Input::InputSystemCursorShape;

                    auto const shape = grab == LoopGrab::Start || grab == LoopGrab::End ? InputSystemCursorShape::SizeWestEast :
                        grab == LoopGrab::Whole ? InputSystemCursorShape::SizeAll : InputSystemCursorShape::Arrow;

                    element.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(shape));
                }

                return;
            }

            auto const x = args.GetCurrentPoint(m_rulerCanvas).Position().X;

            if (!m_rulerMoved && std::abs(x - m_rulerPressX) < DragThreshold)
            {
                return;
            }

            m_rulerMoved = true;

            if (m_loopGrab != LoopGrab::None)
            {
                auto const delta = seq::SnapTick(static_cast<int64_t>(std::llround((x - m_rulerPressX) / (m_barWidth / TicksPerFourFourBar))), seq::TicksPerQuarterNote);

                switch (m_loopGrab)
                {
                case LoopGrab::Start:
                    m_loopStart = std::clamp<int64_t>(m_loopGrabStart + delta, 0, m_loopGrabEnd - seq::TicksPerQuarterNote);
                    break;

                case LoopGrab::End:
                    m_loopEnd = std::max<int64_t>(m_loopGrabEnd + delta, m_loopGrabStart + seq::TicksPerQuarterNote);
                    break;

                default:
                {
                    auto const moved = std::max<int64_t>(-m_loopGrabStart, delta);
                    m_loopStart = m_loopGrabStart + moved;
                    m_loopEnd = m_loopGrabEnd + moved;
                    break;
                }
                }

                InvalidateArrange();
                return;
            }

            // Dragging along the ruler sets the loop, in whole beats.
            auto const tick = std::max<int64_t>(0, static_cast<int64_t>((x + m_scrollX) / (m_barWidth / TicksPerFourFourBar)));
            auto const snapped = seq::SnapTick(tick, seq::TicksPerQuarterNote);

            m_loopStart = std::min(m_loopDragStart, snapped);
            m_loopEnd = std::max(m_loopDragStart, snapped);

            if (m_loopEnd - m_loopStart < seq::TicksPerQuarterNote)
            {
                m_loopEnd = m_loopStart + seq::TicksPerQuarterNote;
            }

            LoopToggle().IsChecked(true);
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag on the ruler.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRulerReleased(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_rulerDragging)
            {
                return;
            }

            m_rulerDragging = false;
            m_loopGrab = LoopGrab::None;
            m_rulerCanvas.ReleasePointerCaptures();

            if (m_rulerMoved)
            {
                ApplyEngineSettings();
                return;
            }

            // A click moves the playhead, snapped to the grid.
            auto const x = args.GetCurrentPoint(m_rulerCanvas).Position().X;
            auto const tick = std::max<int64_t>(0, static_cast<int64_t>((x + m_scrollX) / (m_barWidth / TicksPerFourFourBar)));
            SetPosition(seq::SnapTick(tick, std::max<int64_t>(SnapGrid(), 1)));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish a click on the ruler.")
    }

    // ---------------------------------------------------------------- clips on the timeline

    _Use_decl_annotations_
    void MainWindow::CreateClipAt(std::wstring trackId, int64_t tick, std::optional<seq::PatternMode> pattern, std::optional<seq::GeneratorType> generator) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || track->IsFolder)
            {
                return;
            }

            // A new clip starts on the bar it was asked for, and lasts a bar.
            auto const position = seq::BarPositionAtTick(m_doc.Meter, std::max<int64_t>(0, tick));
            auto const start = seq::TickAtBar(m_doc.Meter, position.Bar);
            auto const length = seq::TicksPerBar(seq::MeterAtTick(m_doc.Meter, start));

            // A chance melody gets two bars, as in comp 4, and so does Learn.
            auto const melody = generator.has_value() && seq::StartsTwoBarsLong(*generator);
            auto const generatorName = generator.has_value() ? seq::GeneratorClipNameKey(*generator) : L"DefaultEuclidName";

            auto clip = generator.has_value() ? seq::MakeGeneratorClip(NextClipName(generatorName), *generator, melody ? 2 * length : length) :
                pattern.has_value() ? seq::MakePatternClip(NextClipName(L"DefaultPatternName"), *pattern) : seq::MakeNotesClip(NextClipName(), length);
            clip.Origin = generator.has_value() ? seq::ClipOrigin::Generated : seq::ClipOrigin::Drawn;
            clip.Color = std::nullopt;

            if (clip.Pattern.has_value())
            {
                seq::SetPatternStepCount(*clip.Pattern, static_cast<uint16_t>(std::clamp<int64_t>(length / std::max<int64_t>(1, clip.Pattern->StepTicks), 1, seq::MaximumPatternSteps)));
                seq::RefreshPatternPreview(clip);
            }

            auto const clipId = clip.Id;
            auto const before = m_doc.Tracks;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(clip, true));
            m_doc.Clips.push_back(std::move(clip));

            auto target = seq::FindTrack(m_doc, trackId);
            target->Timeline.push_back(seq::Placement{ clipId, start, 0 });
            std::stable_sort(target->Timeline.begin(), target->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (size_t i = 0; i < target->Timeline.size(); ++i)
            {
                if (target->Timeline[i].ClipId == clipId)
                {
                    m_selectedPlacement = i;
                    break;
                }
            }

            m_selectedTrackId = trackId;
            Commit(std::wstring{ res::GetString(L"UndoNewClip") }, std::move(changes));
            OpenClipInEditor(clipId, trackId);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to make a clip.")
    }

    void MainWindow::DeleteSelectedPlacement() noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

            if (track == nullptr || m_selectedPlacement >= track->Timeline.size())
            {
                return;
            }

            auto const before = m_doc.Tracks;
            auto target = seq::FindTrack(m_doc, m_selectedTrackId);
            target->Timeline.erase(target->Timeline.begin() + static_cast<ptrdiff_t>(m_selectedPlacement));

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& clip : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
            }

            m_selectedPlacement = SIZE_MAX;

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            Commit(std::wstring{ res::GetString(L"UndoDeleteClip") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete a clip.")
    }

    void MainWindow::DuplicateSelectedPlacement() noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

            if (track == nullptr || m_selectedPlacement >= track->Timeline.size())
            {
                return;
            }

            // The copy is the same clip, placed again right after: edit one and both change.
            auto const placement = track->Timeline[m_selectedPlacement];
            auto const length = EffectiveLength(m_doc, placement);

            seq::Placement copy{ placement.ClipId, placement.Tick + length, placement.Length };

            size_t index{ SIZE_MAX };

            EditTracks(std::wstring{ res::GetString(L"UndoDuplicateClip") }, [&](seq::Sequence& doc)
            {
                if (auto t = seq::FindTrack(doc, m_selectedTrackId); t != nullptr)
                {
                    t->Timeline.push_back(copy);
                    std::stable_sort(t->Timeline.begin(), t->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });

                    for (size_t i = 0; i < t->Timeline.size(); ++i)
                    {
                        if (t->Timeline[i] == copy)
                        {
                            index = i;
                        }
                    }
                }
            });

            if (index != SIZE_MAX)
            {
                m_selectedPlacement = index;
                InvalidateArrange();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to duplicate a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::MakePlacementUnique(std::wstring const& trackId, size_t placement) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || placement >= track->Timeline.size())
            {
                return;
            }

            auto const original = seq::FindClip(m_doc, track->Timeline[placement].ClipId);

            if (original == nullptr)
            {
                return;
            }

            auto copy = *original;
            copy.Id = seq::NewId(L"c");
            copy.Name = res::FormatString(L"UniqueClipNameFormat", original->Name);
            copy.Settings = original->Settings != nullptr ? json::JsonObject::Parse(original->Settings.Stringify()) : nullptr;
            copy.Unknown = original->Unknown != nullptr ? json::JsonObject::Parse(original->Unknown.Stringify()) : nullptr;

            auto const copyId = copy.Id;
            auto const before = m_doc.Tracks;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(copy, true));
            m_doc.Clips.push_back(std::move(copy));

            seq::FindTrack(m_doc, trackId)->Timeline[placement].ClipId = copyId;
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoMakeUnique") }, std::move(changes));
            OpenClipInEditor(copyId, trackId);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to make a clip unique.")
    }

    _Use_decl_annotations_
    void MainWindow::AddTagAt(std::wstring const& trackId, int64_t tick) noexcept
    {
        try
        {
            ShowTextPrompt(res::GetString(L"AddTagTitle"), winrt::hstring{}, [weak = get_weak(), trackId, tick](std::wstring const& text)
            {
                auto strong = weak.get();

                if (!strong || text.empty())
                {
                    return;
                }

                seq::Tag tag{ tick, text.substr(0, 200), std::nullopt };
                auto const name = std::wstring{ res::GetString(L"UndoAddTag") };

                if (trackId.empty())
                {
                    auto tags = strong->m_doc.Tags;
                    tags.push_back(tag);
                    std::stable_sort(tags.begin(), tags.end(), [](seq::Tag const& a, seq::Tag const& b) { return a.Tick < b.Tick; });

                    seq::ChangeList changes{};
                    changes.push_back(seq::MakeSequenceTagsChange(strong->m_doc.Tags, tags));
                    strong->m_doc.Tags = std::move(tags);
                    strong->Commit(name, std::move(changes));
                }
                else
                {
                    strong->EditTracks(name, [&](seq::Sequence& doc)
                    {
                        if (auto t = seq::FindTrack(doc, trackId); t != nullptr)
                        {
                            t->Tags.push_back(tag);
                            std::stable_sort(t->Tags.begin(), t->Tags.end(), [](seq::Tag const& a, seq::Tag const& b) { return a.Tick < b.Tick; });
                        }
                    });
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a tag.")
    }

    _Use_decl_annotations_
    void MainWindow::RenameClip(std::wstring const& clipId) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, clipId);

            if (clip == nullptr)
            {
                return;
            }

            ShowTextPrompt(res::GetString(L"RenameClipTitle"), winrt::hstring{ clip->Name }, [weak = get_weak(), clipId](std::wstring const& text)
            {
                auto strong = weak.get();

                if (!strong || text.empty())
                {
                    return;
                }

                auto const current = seq::FindClip(strong->m_doc, clipId);

                if (current == nullptr)
                {
                    return;
                }

                auto renamed = *current;
                renamed.Name = text.substr(0, 100);

                seq::ChangeList changes{};
                changes.push_back(seq::MakeClipSettingsChange(*current, renamed));
                strong->m_undo.ApplyAndCommit(strong->m_doc, std::wstring{ res::GetString(L"UndoRenameClip") }, std::move(changes));
                strong->DocumentChanged();
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to rename a clip.")
    }

    // ---------------------------------------------------------------- scenes and the launcher

    void MainWindow::AddScene() noexcept
    {
        try
        {
            auto scenes = m_doc.Scenes;
            scenes.push_back(seq::Scene{ seq::NewId(L"s"), std::wstring{}, nullptr });

            auto const before = m_doc.Tracks;
            seq::ChangeList changes{};
            changes.push_back(seq::MakeScenesChange(m_doc.Scenes, scenes));
            m_doc.Scenes = std::move(scenes);

            ForEachTrackMutable(m_doc.Tracks, [&](seq::Track& track) { track.Slots.resize(m_doc.Scenes.size()); });
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            Commit(std::wstring{ res::GetString(L"UndoAddScene") }, std::move(changes));

            // The new scene is the last, so the end of the launcher shows it and the button again.
            ScrollScenes(std::numeric_limits<double>::max());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a scene.")
    }

    _Use_decl_annotations_
    void MainWindow::DeleteScene(size_t scene) noexcept
    {
        try
        {
            if (scene >= m_doc.Scenes.size() || m_doc.Scenes.size() <= 1)
            {
                return;
            }

            auto scenes = m_doc.Scenes;
            scenes.erase(scenes.begin() + static_cast<ptrdiff_t>(scene));

            auto const before = m_doc.Tracks;
            seq::ChangeList changes{};
            changes.push_back(seq::MakeScenesChange(m_doc.Scenes, scenes));
            m_doc.Scenes = std::move(scenes);

            ForEachTrackMutable(m_doc.Tracks, [&](seq::Track& track)
            {
                if (scene < track.Slots.size())
                {
                    track.Slots.erase(track.Slots.begin() + static_cast<ptrdiff_t>(scene));
                }

                track.Slots.resize(m_doc.Scenes.size());
            });

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& clip : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
            }

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            m_selectedSlot = SIZE_MAX;
            Commit(std::wstring{ res::GetString(L"UndoDeleteScene") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete a scene.")
    }

    _Use_decl_annotations_
    void MainWindow::LaunchScene(size_t scene) noexcept
    {
        try
        {
            if (m_engine == nullptr || scene >= m_doc.Scenes.size())
            {
                return;
            }

            // Every track takes part: an empty slot in the scene stops its track.
            std::vector<seq::PlaybackEngine::SceneLaunch> launches{};

            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                if (!track.IsFolder)
                {
                    launches.push_back({ track.Id, scene < track.Slots.size() ? track.Slots[scene] : std::wstring{} });
                }

                return true;
            });

            m_engine->LaunchScene(launches, LaunchQuantize(), static_cast<int32_t>(scene));

            UpdateTransport();

            if (m_frameTimer != nullptr)
            {
                m_frameTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to launch a scene.")
    }

    _Use_decl_annotations_
    void MainWindow::LaunchSlot(std::wstring const& trackId, size_t scene) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (m_engine == nullptr || track == nullptr || scene >= track->Slots.size())
            {
                return;
            }

            auto const& clipId = track->Slots[scene];
            auto const quantize = LaunchQuantize();

            if (clipId.empty())
            {
                m_engine->StopTrack(trackId, quantize);
            }
            else
            {
                // A second click on the playing clip stops it.
                auto view = m_launchViews.find(trackId);

                if (view != m_launchViews.end() && view->second.Plays(clipId, scene) && !view->second.Pending)
                {
                    m_engine->StopTrack(trackId, quantize);
                }
                else
                {
                    m_engine->LaunchClip(trackId, clipId, quantize, static_cast<int32_t>(scene));
                }
            }

            UpdateTransport();

            if (m_frameTimer != nullptr)
            {
                m_frameTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to launch a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::CreateClipInSlot(std::wstring const& trackId, size_t scene, std::optional<seq::PatternMode> pattern, std::optional<seq::GeneratorType> generator) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || track->IsFolder || scene >= m_doc.Scenes.size())
            {
                return;
            }

            auto const length = seq::TicksPerBar(m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front());
            auto const melody = generator.has_value() && seq::StartsTwoBarsLong(*generator);
            auto const generatorName = generator.has_value() ? seq::GeneratorClipNameKey(*generator) : L"DefaultEuclidName";

            auto clip = generator.has_value() ? seq::MakeGeneratorClip(NextClipName(generatorName), *generator, melody ? 2 * length : length) :
                pattern.has_value() ? seq::MakePatternClip(NextClipName(L"DefaultPatternName"), *pattern) : seq::MakeNotesClip(NextClipName(), length);
            clip.Origin = generator.has_value() ? seq::ClipOrigin::Generated : seq::ClipOrigin::Drawn;

            if (clip.Pattern.has_value())
            {
                seq::SetPatternStepCount(*clip.Pattern, static_cast<uint16_t>(std::clamp<int64_t>(length / std::max<int64_t>(1, clip.Pattern->StepTicks), 1, seq::MaximumPatternSteps)));
                seq::RefreshPatternPreview(clip);
            }

            auto const clipId = clip.Id;
            auto const before = m_doc.Tracks;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(clip, true));
            m_doc.Clips.push_back(std::move(clip));

            auto target = seq::FindTrack(m_doc, trackId);
            target->Slots.resize(m_doc.Scenes.size());
            target->Slots[scene] = clipId;

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            m_selectedSlotTrackId = trackId;
            m_selectedSlot = scene;

            Commit(std::wstring{ res::GetString(L"UndoNewClip") }, std::move(changes));
            OpenClipInEditor(clipId, trackId);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to make a clip in a slot.")
    }

    _Use_decl_annotations_
    void MainWindow::PlaceSlotClipOnTimeline(std::wstring const& trackId, size_t scene) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || scene >= track->Slots.size() || track->Slots[scene].empty())
            {
                return;
            }

            auto const clipId = track->Slots[scene];
            auto const bar = seq::BarPositionAtTick(m_doc.Meter, m_position).Bar;
            auto const tick = seq::TickAtBar(m_doc.Meter, bar);

            EditTracks(std::wstring{ res::GetString(L"UndoPlaceClip") }, [&](seq::Sequence& doc)
            {
                if (auto t = seq::FindTrack(doc, trackId); t != nullptr)
                {
                    t->Timeline.push_back(seq::Placement{ clipId, tick, 0 });
                    std::stable_sort(t->Timeline.begin(), t->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });
                }
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to place a clip on the timeline.")
    }

    _Use_decl_annotations_
    void MainWindow::DeleteSlotClip(std::wstring const& trackId, size_t scene) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || scene >= track->Slots.size() || track->Slots[scene].empty())
            {
                return;
            }

            auto const before = m_doc.Tracks;
            seq::FindTrack(m_doc, trackId)->Slots[scene].clear();

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            for (auto& clip : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
            }

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            Commit(std::wstring{ res::GetString(L"UndoDeleteClip") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete a clip from a slot.")
    }

    _Use_decl_annotations_
    std::optional<std::pair<std::wstring, size_t>> MainWindow::HitTestSlot(bool pinned, foundation::Point point) noexcept
    {
        try
        {
            auto const row = pinned ? m_layout.PinnedRowAt(point.Y) : m_layout.ScrollingRowAt(point.Y + m_scrollY);

            if (row == nullptr || row->Kind != seq::ArrangeRowKind::Track)
            {
                return std::nullopt;
            }

            auto const scene = SceneAtLauncherX(point.X);

            if (!scene.has_value())
            {
                return std::nullopt;
            }

            return std::make_pair(row->TrackId, *scene);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    std::optional<size_t> MainWindow::SceneAtLauncherX(double x) const noexcept
    {
        auto const scrolled = x + m_sceneScrollX;

        if (scrolled < 0)
        {
            return std::nullopt;
        }

        auto const scene = static_cast<size_t>(scrolled / SceneWidth);

        if (scene >= m_doc.Scenes.size())
        {
            return std::nullopt;
        }

        return scene;
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherPressed(bool pinned, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas;
            auto const point = args.GetCurrentPoint(canvas);

            if (!point.Properties().IsLeftButtonPressed())
            {
                return;
            }

            auto const hit = HitTestSlot(pinned, point.Position());

            if (!hit.has_value())
            {
                return;
            }

            auto const& [trackId, scene] = *hit;
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr)
            {
                return;
            }

            m_selectedSlotTrackId = trackId;
            m_selectedSlot = scene;
            SelectTrack(trackId);

            auto const clipId = scene < track->Slots.size() ? track->Slots[scene] : std::wstring{};

            // The triangle at the cell's top left plays it; the rest of the cell opens it.
            auto const xInSlot = static_cast<double>(point.Position().X) + m_sceneScrollX - static_cast<double>(scene) * SceneWidth;

            if (m_slotTake.has_value() && m_slotTake->TrackId == trackId && m_slotTake->Scene == scene)
            {
                // The slot being recorded into: the take ends at the end of this bar. The second
                // press of a double click that started it doesn't count.
                auto const sinceStart = std::chrono::steady_clock::now() - m_slotTake->StartedAt;

                if (sinceStart > std::chrono::milliseconds(::GetDoubleClickTime()))
                {
                    FinishSlotRecording(SlotTakeEnd::Early);
                }
            }
            else if (clipId.empty())
            {
                if (m_armed.contains(trackId) && !track->IsFolder)
                {
                    StartSlotRecording(trackId, scene);
                }
            }
            else
            {
                if (xInSlot < 22.0)
                {
                    LaunchSlot(trackId, scene);
                }
                else
                {
                    if (m_editorClipId != clipId)
                    {
                        OpenClipInEditor(clipId, trackId);
                    }

                    // Dragged, it goes to another slot or onto the timeline.
                    BeginSlotDrag(pinned, trackId, scene, args);
                }
            }

            InvalidateLaunchers();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a click in the clip launcher.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherDoubleTapped(bool pinned, input::DoubleTappedRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas;
            auto const hit = HitTestSlot(pinned, args.GetPosition(canvas));

            if (!hit.has_value())
            {
                return;
            }

            auto const& [trackId, scene] = *hit;
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr)
            {
                return;
            }

            // The first press already started or ended a recording here.
            if (m_slotTake.has_value() && m_slotTake->TrackId == trackId && m_slotTake->Scene == scene)
            {
                args.Handled(true);
                return;
            }

            if (scene >= track->Slots.size() || track->Slots[scene].empty())
            {
                CreateClipInSlot(trackId, scene);
            }
            else
            {
                LaunchSlot(trackId, scene);
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a double click in the clip launcher.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherRightTapped(bool pinned, input::RightTappedRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas;
            auto const position = args.GetPosition(canvas);
            auto const hit = HitTestSlot(pinned, position);

            if (!hit.has_value())
            {
                return;
            }

            m_selectedSlotTrackId = hit->first;
            m_selectedSlot = hit->second;
            InvalidateLaunchers();

            ShowSlotMenu(hit->first, hit->second, canvas, position);
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the clip launcher menu.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowSlotMenu(std::wstring const& trackId, size_t scene, xaml::UIElement const& anchor, foundation::Point point)
    {
        auto const track = seq::FindTrack(m_doc, trackId);

        if (track == nullptr)
        {
            return;
        }

        auto const clipId = scene < track->Slots.size() ? track->Slots[scene] : std::wstring{};

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        if (clipId.empty())
        {
            if (m_armed.contains(trackId) && !track->IsFolder)
            {
                add(res::GetString(L"MenuRecordIntoSlot"), L"\uE7C8", [weak, trackId, scene]() { if (auto s = weak.get()) { s->StartSlotRecording(trackId, scene); } });

                controls::MenuFlyoutSubItem length{};
                length.Text(res::GetString(L"MenuRecordLength"));

                for (uint32_t const bars : { 1u, 2u, 4u, 8u, 16u })
                {
                    controls::RadioMenuFlyoutItem item{};
                    item.Text(bars == 1 ? res::GetString(L"RecordLengthOneBar") : res::FormatString(L"RecordLengthBarsFormat", bars));
                    item.GroupName(L"SlotRecordLength");
                    item.IsChecked(seq::AppSettings::Current().SlotRecordBars() == bars);
                    item.Click([bars](auto&&, auto&&) { seq::AppSettings::Current().SlotRecordBars(bars); });
                    length.Items().Append(item);
                }

                menu.Items().Append(length);
                menu.Items().Append(controls::MenuFlyoutSeparator{});
            }

            // A new clip, pattern or generator shows the same symbol its slot will (design section 27).
            auto const addKind = [&menu](winrt::hstring const& text, seq::ClipIcon kind, std::function<void()> action)
            {
                controls::MenuFlyoutItem item{};
                item.Text(text);
                item.Icon(seq::MakeClipIcon(kind));
                item.Click([action](auto&&, auto&&) { action(); });
                menu.Items().Append(item);
            };

            addKind(res::GetString(L"MenuNewClip"), seq::ClipIcon::Notes, [weak, trackId, scene]() { if (auto s = weak.get()) { s->CreateClipInSlot(trackId, scene); } });
            addKind(res::GetString(L"MenuNewDrumPattern"), seq::ClipIcon::DrumPattern, [weak, trackId, scene]() { if (auto s = weak.get()) { s->CreateClipInSlot(trackId, scene, seq::PatternMode::Drums); } });
            addKind(res::GetString(L"MenuNewMelodicPattern"), seq::ClipIcon::MelodicPattern, [weak, trackId, scene]() { if (auto s = weak.get()) { s->CreateClipInSlot(trackId, scene, seq::PatternMode::Melodic); } });

            menu.Items().Append(MakeNewGeneratorMenu(res::GetString(L"MenuNewGenerator"), [weak, trackId, scene](seq::GeneratorType type)
            {
                if (auto s = weak.get()) { s->CreateClipInSlot(trackId, scene, std::nullopt, type); }
            }));

            add(res::GetString(L"MenuStopTrack"), L"\uE71A", [weak, trackId, scene]() { if (auto s = weak.get()) { s->LaunchSlot(trackId, scene); } });
        }
        else
        {
            add(res::GetString(L"MenuLaunchClip"), L"\uE768", [weak, trackId, scene]() { if (auto s = weak.get()) { s->LaunchSlot(trackId, scene); } });
            add(res::GetString(L"MenuEditClip"), L"\uE70F", [weak, trackId, clipId]() { if (auto s = weak.get()) { s->ShowMixer(false); s->OpenClipInEditor(clipId, trackId); } });
            add(res::GetString(L"MenuRenameClip"), L"\uE8AC", [weak, clipId]() { if (auto s = weak.get()) { s->RenameClip(clipId); } });
            add(res::GetString(L"MenuPlaceOnTimeline"), L"\uE8A5", [weak, trackId, scene]() { if (auto s = weak.get()) { s->PlaceSlotClipOnTimeline(trackId, scene); } });
            menu.Items().Append(controls::MenuFlyoutSeparator{});
            add(res::GetString(L"MenuDeleteClip"), L"\uE74D", [weak, trackId, scene]() { if (auto s = weak.get()) { s->DeleteSlotClip(trackId, scene); } });
        }

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    // ---------------------------------------------------------------- prompts

    _Use_decl_annotations_
    void MainWindow::ShowTextPrompt(winrt::hstring const& title, winrt::hstring const& initial, std::function<void(std::wstring const&)> done)
    {
        [](winrt::com_ptr<MainWindow> strong, winrt::hstring title, winrt::hstring initial, std::function<void(std::wstring const&)> done) -> winrt::fire_and_forget
        {
            try
            {
                controls::TextBox box{};
                box.Text(initial);
                box.SelectAll();
                box.MinWidth(280);
                automation::AutomationProperties::SetName(box, title);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(title));
                dialog.Content(box);
                dialog.PrimaryButtonText(res::GetString(L"DialogOk"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                // Enter accepts. Hide reports None, the same as Escape, so Enter says so first.
                auto const accepted = std::make_shared<bool>(false);

                box.KeyDown([accepted, weak = winrt::make_weak(dialog)](auto&&, input::KeyRoutedEventArgs const& args)
                {
                    if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
                    {
                        *accepted = true;
                        args.Handled(true);

                        if (auto d = weak.get())
                        {
                            d.Hide();
                        }
                    }
                });

                auto const result = co_await dialog.ShowAsync();

                if (result == controls::ContentDialogResult::Primary)
                {
                    *accepted = true;
                }

                std::wstring text{ box.Text() };

                // Trim, so "  Verse " is "Verse".
                auto const first = text.find_first_not_of(L" \t");
                auto const last = text.find_last_not_of(L" \t");
                text = first == std::wstring::npos ? std::wstring{} : text.substr(first, last - first + 1);

                if (*accepted && done)
                {
                    done(text);
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to ask for text.")
        }(get_strong(), title, initial, std::move(done));
    }
}
