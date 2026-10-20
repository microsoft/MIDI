// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Sizing the arrangement's columns and rows, selecting several tracks, and dragging things in it:
// track headers into folders, clips along and between tracks and launcher slots, and tags.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceRender.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        using winrt::Microsoft::UI::Input::InputSystemCursorShape;

        constexpr double TicksPerFourFourBar = 3840.0;

        // How far a press has to move before it's a drag rather than a click.
        constexpr double DragThreshold = 4.0;

        constexpr double SplitterWidth = 8.0;
        constexpr double RowGripHeight = 5.0;

        // However wide the columns are dragged, the timeline keeps this much.
        constexpr double MinimumTimelineWidth = 160.0;

        // Narrower than this, the track list's buttons drop their words.
        constexpr double NarrowHeaderWidth = 252.0;

        // How near an edge a drag has to come before the view scrolls, and how far each step goes.
        constexpr double ScrollMargin = 28.0;
        constexpr double ScrollStep = 16.0;

        // Each level of folders indents its rows this far.
        constexpr double DepthIndent = 14.0;

        xaml::GridLength Pixels(double value) noexcept
        {
            return xaml::GridLength{ value, xaml::GridUnitType::Pixel };
        }

        void SetCursor(xaml::UIElement const& element, InputSystemCursorShape shape)
        {
            // ProtectedCursor is not on the projected type, but the interface carrying it is.
            if (auto const protectedElement = element.try_as<xaml::IUIElementProtected>())
            {
                protectedElement.ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(shape));
            }
        }

        // The accent line inside a splitter, shown while it's pointed at, dragged or focused.
        void Highlight(controls::Grid const& splitter, bool on)
        {
            if (splitter != nullptr && splitter.Children().Size() > 0)
            {
                splitter.Children().GetAt(0).Opacity(on ? 1.0 : 0.0);
            }
        }

        bool IsHeaderSplitter(foundation::IInspectable const& sender)
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();
            return element != nullptr && winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") == L"header";
        }

        bool Contains(foundation::Rect const& rect, foundation::Point point) noexcept
        {
            return point.X >= rect.X && point.X <= rect.X + rect.Width && point.Y >= rect.Y && point.Y <= rect.Y + rect.Height;
        }

        // A header's second line, which a short row hides.
        void ShowSubLine(xaml::DependencyObject const& element, bool show)
        {
            if (auto const block = element.try_as<controls::TextBlock>(); block != nullptr && winrt::unbox_value_or<winrt::hstring>(block.Tag(), L"") == L"sub")
            {
                block.Visibility(show ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                return;
            }

            if (auto const panel = element.try_as<controls::Panel>(); panel != nullptr)
            {
                for (auto const& child : panel.Children())
                {
                    ShowSubLine(child, show);
                }
            }
        }

        int64_t PlacementLength(seq::Sequence const& doc, seq::Placement const& placement) noexcept
        {
            if (placement.Length > 0)
            {
                return placement.Length;
            }

            auto const clip = seq::FindClip(doc, placement.ClipId);
            return clip != nullptr ? std::max<int64_t>(1, clip->Length) : seq::TicksPerQuarterNote * 4;
        }

        void SortTags(std::vector<seq::Tag>& tags)
        {
            std::stable_sort(tags.begin(), tags.end(), [](seq::Tag const& a, seq::Tag const& b) { return a.Tick < b.Tick; });
        }
    }

    // ---------------------------------------------------------------- the columns

    void MainWindow::InitializeArrangeSplitters()
    {
        auto weak = get_weak();

        for (auto const& splitter : { HeaderSplitter(), LauncherSplitter() })
        {
            SetCursor(splitter, InputSystemCursorShape::SizeWestEast);

            auto const header = splitter == HeaderSplitter();
            auto const nameKey = header ? L"HeaderSplitterName" : L"LauncherSplitterName";
            controls::ToolTipService::SetToolTip(splitter, winrt::box_value(res::GetString(header ? L"HeaderSplitterTip" : L"LauncherSplitterTip")));
            automation::AutomationProperties::SetName(splitter, res::GetString(nameKey));
            automation::AutomationProperties::SetHelpText(splitter, res::GetString(L"SplitterHelp"));

            splitter.PointerEntered([](foundation::IInspectable const& sender, auto&&)
            {
                Highlight(sender.try_as<controls::Grid>(), true);
            });

            splitter.PointerExited([weak](foundation::IInspectable const& sender, auto&&)
            {
                auto strong = weak.get();
                auto const grid = sender.try_as<controls::Grid>();

                if (strong && grid && !strong->m_columnDragging && grid.FocusState() == xaml::FocusState::Unfocused)
                {
                    Highlight(grid, false);
                }
            });

            splitter.GotFocus([](foundation::IInspectable const& sender, auto&&) { Highlight(sender.try_as<controls::Grid>(), true); });
            splitter.LostFocus([](foundation::IInspectable const& sender, auto&&) { Highlight(sender.try_as<controls::Grid>(), false); });

            splitter.PointerPressed([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnColumnSplitterPressed(sender.as<controls::Grid>(), args); }
            });

            splitter.PointerMoved([weak](foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnColumnSplitterMoved(sender.as<controls::Grid>(), args); }
            });

            splitter.PointerReleased([weak](foundation::IInspectable const& sender, auto&&)
            {
                if (auto strong = weak.get()) { strong->OnColumnSplitterReleased(sender.as<controls::Grid>()); }
            });

            splitter.PointerCaptureLost([weak](foundation::IInspectable const& sender, auto&&)
            {
                if (auto strong = weak.get()) { strong->OnColumnSplitterReleased(sender.as<controls::Grid>()); }
            });

            // A double click puts the column back to its first width.
            splitter.DoubleTapped([weak](foundation::IInspectable const& sender, input::DoubleTappedRoutedEventArgs const& args)
            {
                if (auto strong = weak.get())
                {
                    auto const header = IsHeaderSplitter(sender);
                    strong->SetColumnWidth(header, header ? seq::AppSettings::DefaultHeaderWidth : seq::AppSettings::DefaultLauncherWidth, true);
                    args.Handled(true);
                }
            });

            // Left and Right move it ten pixels, or one with Shift.
            splitter.KeyDown([weak](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
            {
                using winrt::Windows::System::VirtualKey;

                auto strong = weak.get();

                if (!strong || (args.Key() != VirtualKey::Left && args.Key() != VirtualKey::Right))
                {
                    return;
                }

                auto const header = IsHeaderSplitter(sender);
                auto const step = (::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? 1.0 : 10.0;
                auto const width = header ? strong->m_headerWidth : strong->m_launcherWidth;

                strong->SetColumnWidth(header, width + (args.Key() == VirtualKey::Left ? -step : step), true);
                args.Handled(true);
            });
        }

        // A row's grip or a dragged header hands its pointer to the canvas the headers sit on, so
        // rebuilding the headers mid-drag doesn't end it.
        for (auto const pinned : { true, false })
        {
            auto const canvas = pinned ? PinnedHeaderCanvas() : ScrollHeaderCanvas();

            canvas.PointerMoved([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnHeaderCanvasMoved(pinned, args); }
            });

            canvas.PointerReleased([weak, pinned](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->OnHeaderCanvasReleased(pinned, false); }
            });

            canvas.PointerCaptureLost([weak, pinned](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->OnHeaderCanvasReleased(pinned, true); }
            });

            auto const launcher = pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas;

            launcher.PointerMoved([weak, pinned](auto&&, input::PointerRoutedEventArgs const& args)
            {
                if (auto strong = weak.get()) { strong->OnLauncherMoved(pinned, args); }
            });

            launcher.PointerReleased([weak, pinned](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->OnLauncherReleased(pinned, false); }
            });

            launcher.PointerCaptureLost([weak, pinned](auto&&, auto&&)
            {
                if (auto strong = weak.get()) { strong->OnLauncherReleased(pinned, true); }
            });
        }

        m_dragScrollTimer = xaml::DispatcherTimer{};
        m_dragScrollTimer.Interval(std::chrono::milliseconds{ 30 });
        m_dragScrollTimer.Tick([weak](auto&&, auto&&)
        {
            if (auto strong = weak.get()) { strong->OnDragScrollTick(); }
        });

        SetCursor(EditorSplitter(), InputSystemCursorShape::SizeNorthSouth);
    }

    void MainWindow::ApplyArrangeColumns() noexcept
    {
        try
        {
            auto const launcherShown = LauncherToggle().IsChecked() != nullptr && LauncherToggle().IsChecked().GetBoolean();

            HeaderColumn().Width(Pixels(m_headerWidth));
            LauncherColumn().Width(Pixels(launcherShown ? m_launcherWidth : 0.0));

            HeaderSplitter().Margin(xaml::Thickness{ m_headerWidth - SplitterWidth / 2.0, 0, 0, 0 });
            LauncherSplitter().Margin(xaml::Thickness{ m_headerWidth + m_launcherWidth - SplitterWidth / 2.0, 0, 0, 0 });
            LauncherSplitter().Visibility(launcherShown ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            for (auto const* headers : { &m_pinnedHeaders, &m_scrollHeaders })
            {
                for (auto const& [row, element] : *headers)
                {
                    if (auto const framework = element.try_as<xaml::FrameworkElement>())
                    {
                        framework.Width(m_headerWidth);
                    }
                }
            }

            if (m_dropLine != nullptr)
            {
                m_dropBox.Width(m_headerWidth);
            }

            auto const words = m_headerWidth >= NarrowHeaderWidth ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
            AddTrackButtonText().Visibility(words);
            AddFolderButtonText().Visibility(words);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::SetColumnWidth(bool header, double width, bool save) noexcept
    {
        try
        {
            auto const launcherShown = LauncherToggle().IsChecked() != nullptr && LauncherToggle().IsChecked().GetBoolean();
            auto const total = ArrangeGrid().ActualWidth() - InspectorColumn().ActualWidth();
            auto& settings = seq::AppSettings::Current();

            if (header)
            {
                auto const room = total > 1 ? total - (launcherShown ? m_launcherWidth : 0.0) - MinimumTimelineWidth : seq::AppSettings::MaximumHeaderWidth;
                auto const maximum = std::max(seq::AppSettings::MinimumHeaderWidth, std::min(seq::AppSettings::MaximumHeaderWidth, room));
                m_headerWidth = std::clamp(std::round(width), seq::AppSettings::MinimumHeaderWidth, maximum);

                if (save)
                {
                    settings.HeaderWidth(m_headerWidth);
                }
            }
            else
            {
                auto const room = total > 1 ? total - m_headerWidth - MinimumTimelineWidth : seq::AppSettings::MaximumLauncherWidth;
                auto const maximum = std::max(seq::AppSettings::MinimumLauncherWidth, std::min(seq::AppSettings::MaximumLauncherWidth, room));
                m_launcherWidth = std::clamp(std::round(width), seq::AppSettings::MinimumLauncherWidth, maximum);

                if (save)
                {
                    settings.LauncherWidth(m_launcherWidth);
                }
            }

            ApplyArrangeColumns();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to resize a column.")
    }

    _Use_decl_annotations_
    void MainWindow::OnColumnSplitterPressed(controls::Grid const& splitter, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!args.GetCurrentPoint(splitter).Properties().IsLeftButtonPressed())
            {
                return;
            }

            // Against the window, not the bar: the bar moves under the pointer as it's dragged.
            m_columnDragHeader = IsHeaderSplitter(splitter);
            m_columnDragStartX = args.GetCurrentPoint(RootGrid()).Position().X;
            m_columnDragStartWidth = m_columnDragHeader ? m_headerWidth : m_launcherWidth;
            m_columnDragging = splitter.CapturePointer(args.Pointer());

            Highlight(splitter, true);
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start resizing a column.")
    }

    _Use_decl_annotations_
    void MainWindow::OnColumnSplitterMoved(controls::Grid const&, input::PointerRoutedEventArgs const& args)
    {
        if (!m_columnDragging)
        {
            return;
        }

        try
        {
            auto const x = args.GetCurrentPoint(RootGrid()).Position().X;
            SetColumnWidth(m_columnDragHeader, m_columnDragStartWidth + (x - m_columnDragStartX), false);
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to resize a column.")
    }

    _Use_decl_annotations_
    void MainWindow::OnColumnSplitterReleased(controls::Grid const& splitter)
    {
        if (!m_columnDragging)
        {
            return;
        }

        try
        {
            // Cleared first: letting go of the capture comes straight back here.
            m_columnDragging = false;
            splitter.ReleasePointerCaptures();
            Highlight(splitter, false);

            // Every window opens with the widths last dragged to.
            if (m_columnDragHeader)
            {
                seq::AppSettings::Current().HeaderWidth(m_headerWidth);
            }
            else
            {
                seq::AppSettings::Current().LauncherWidth(m_launcherWidth);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish resizing a column.")
    }

    // ---------------------------------------------------------------- the rows

    _Use_decl_annotations_
    void MainWindow::AddRowGrip(controls::Grid const& header, seq::ArrangeRow const& row)
    {
        if (row.Kind != seq::ArrangeRowKind::Track && row.Kind != seq::ArrangeRowKind::Folder && row.Kind != seq::ArrangeRowKind::Tempo)
        {
            return;
        }

        controls::Border grip{};
        grip.Height(RowGripHeight);
        grip.VerticalAlignment(xaml::VerticalAlignment::Bottom);
        grip.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
        controls::Grid::SetColumnSpan(grip, static_cast<int32_t>(std::max<uint32_t>(1, header.ColumnDefinitions().Size())));
        SetCursor(grip, InputSystemCursorShape::SizeNorthSouth);
        controls::ToolTipService::SetToolTip(grip, winrt::box_value(res::GetString(L"RowGripTip")));

        auto weak = get_weak();
        auto const tempo = row.Kind == seq::ArrangeRowKind::Tempo;

        grip.PointerPressed([weak, tempo, trackId = row.TrackId, pinned = row.Pinned, height = row.Height](auto&&, input::PointerRoutedEventArgs const& args)
        {
            auto strong = weak.get();

            if (!strong)
            {
                return;
            }

            try
            {
                auto const canvas = pinned ? strong->PinnedHeaderCanvas() : strong->ScrollHeaderCanvas();

                if (!args.GetCurrentPoint(canvas).Properties().IsLeftButtonPressed())
                {
                    return;
                }

                RowResize resize{};
                resize.Active = true;
                resize.Tempo = tempo;
                resize.TrackId = trackId;
                resize.StartY = args.GetCurrentPoint(strong->RootGrid()).Position().Y;
                resize.StartHeight = height;
                strong->m_rowResize = std::move(resize);
                strong->m_trackDrag = TrackDrag{};

                canvas.CapturePointer(args.Pointer());

                // Not a press on the header, so it doesn't start dragging the track.
                args.Handled(true);
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start resizing a row.")
        });

        // A double click puts the row back to its standard height.
        grip.DoubleTapped([weak, tempo, trackId = row.TrackId](auto&&, input::DoubleTappedRoutedEventArgs const& args)
        {
            if (auto strong = weak.get())
            {
                args.Handled(true);

                if (tempo)
                {
                    if (strong->m_doc.TempoRowHeight != 0)
                    {
                        strong->m_doc.TempoRowHeight = 0;
                        strong->KeepViewChange();
                        strong->RebuildLayout();
                    }

                    return;
                }

                strong->SetTracksHeight({ trackId }, 0);
            }
        });

        grip.Tapped([](auto&&, input::TappedRoutedEventArgs const& args) { args.Handled(true); });

        header.Children().Append(grip);
    }

    _Use_decl_annotations_
    void MainWindow::OnHeaderCanvasMoved(bool pinned, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (m_rowResize.Active)
            {
                auto const y = args.GetCurrentPoint(RootGrid()).Position().Y;
                auto const dy = static_cast<double>(y) - m_rowResize.StartY;

                if (!m_rowResize.Moved && std::abs(dy) < 2.0)
                {
                    return;
                }

                m_rowResize.Moved = true;

                auto const minimum = m_rowResize.Tempo ? seq::TempoRowHeight : static_cast<double>(seq::MinimumTrackRowHeight);
                auto const height = std::clamp(std::round(m_rowResize.StartHeight + dy), minimum, static_cast<double>(seq::MaximumTrackRowHeight));

                if (m_rowResize.Tempo)
                {
                    m_doc.TempoRowHeight = height == seq::TempoRowHeight ? uint16_t{ 0 } : static_cast<uint16_t>(height);
                }
                else if (auto const track = seq::FindTrack(m_doc, m_rowResize.TrackId); track != nullptr)
                {
                    auto const standard = track->IsFolder ? seq::FolderRowHeight : seq::TrackRowHeight;
                    track->Height = height == standard ? uint16_t{ 0 } : static_cast<uint16_t>(height);
                }

                RelayoutRows();
                args.Handled(true);
                return;
            }

            if (m_trackDrag.Pressed)
            {
                OnHeaderMoved(pinned, args);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to resize a row.")
    }

    _Use_decl_annotations_
    void MainWindow::OnHeaderCanvasReleased(bool, bool canceled)
    {
        try
        {
            if (m_rowResize.Active)
            {
                // Cleared first: letting go of the capture comes straight back here.
                auto const moved = m_rowResize.Moved;
                m_rowResize = RowResize{};

                PinnedHeaderCanvas().ReleasePointerCaptures();
                ScrollHeaderCanvas().ReleasePointerCaptures();

                if (moved)
                {
                    KeepViewChange();

                    // A row that changed between one line and two needs its header made again.
                    RebuildLayout();
                }

                return;
            }

            if (m_trackDrag.Pressed)
            {
                FinishTrackDrag(!canceled);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish resizing a row.")
    }

    void MainWindow::RelayoutRows() noexcept
    {
        try
        {
            auto layout = seq::BuildArrangeLayout(m_doc);

            // Heights only: the same rows, so the headers stay and just move. Anything else needs
            // them made again.
            if (layout.Pinned.size() != m_pinnedHeaders.size() || layout.Scrolling.size() != m_scrollHeaders.size())
            {
                RebuildLayout();
                return;
            }

            m_layout = std::move(layout);
            PinnedRow().Height(Pixels(m_layout.PinnedHeight));

            auto const update = [](std::vector<seq::ArrangeRow> const& rows, std::vector<std::pair<seq::ArrangeRow, xaml::UIElement>>& headers)
            {
                for (size_t i = 0; i < rows.size(); ++i)
                {
                    headers[i].first = rows[i];

                    if (auto const framework = headers[i].second.try_as<xaml::FrameworkElement>())
                    {
                        framework.Height(rows[i].Height);
                        ShowSubLine(framework, rows[i].Height >= seq::OneLineRowHeight);
                    }
                }
            };

            update(m_layout.Pinned, m_pinnedHeaders);
            update(m_layout.Scrolling, m_scrollHeaders);

            PositionHeaders();
            UpdateScrollBars();
            InvalidateArrange();
            InvalidateLaunchers();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to lay out the rows.")
    }

    _Use_decl_annotations_
    void MainWindow::SetTracksHeight(std::vector<std::wstring> const& trackIds, uint16_t height) noexcept
    {
        try
        {
            auto changed{ false };

            for (auto const& id : trackIds)
            {
                if (auto const track = seq::FindTrack(m_doc, id); track != nullptr && track->Height != height)
                {
                    track->Height = height;
                    changed = true;
                }
            }

            if (changed)
            {
                KeepViewChange();
                RebuildLayout();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the height of tracks.")
    }

    void MainWindow::KeepViewChange() noexcept
    {
        try
        {
            // Kept in the file so the sequence opens the way it was left, but not an edit to undo,
            // the same as opening a folder.
            m_dirty = true;
            ++m_version;

            UpdateTitle();
            UpdateSavedState();

            if (m_autosaveTimer != nullptr && !m_path.empty() && !m_readOnly)
            {
                m_autosaveTimer.Stop();
                m_autosaveTimer.Start();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to keep a change to the view.")
    }

    // ---------------------------------------------------------------- selecting tracks

    _Use_decl_annotations_
    bool MainWindow::IsTrackSelected(std::wstring_view trackId) const noexcept
    {
        return !trackId.empty() && std::find(m_selectedTrackIds.begin(), m_selectedTrackIds.end(), trackId) != m_selectedTrackIds.end();
    }

    std::vector<std::wstring> MainWindow::SelectedTrackIds() const
    {
        // In the order the rows are drawn.
        std::vector<std::wstring> ids{};

        seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
        {
            if (IsTrackSelected(track.Id))
            {
                ids.push_back(track.Id);
            }

            return true;
        });

        return ids;
    }

    _Use_decl_annotations_
    void MainWindow::ClickTrack(std::wstring const& trackId, bool control, bool shift) noexcept
    {
        try
        {
            if ((!control && !shift) || (shift && m_selectionAnchorId.empty()))
            {
                SelectTrack(trackId);
                return;
            }

            if (shift)
            {
                // Every track and folder row from the anchor to this one, as they're drawn.
                std::vector<std::wstring> rows{};

                for (auto const* list : { &m_layout.Pinned, &m_layout.Scrolling })
                {
                    for (auto const& row : *list)
                    {
                        if ((row.Kind == seq::ArrangeRowKind::Track || row.Kind == seq::ArrangeRowKind::Folder) &&
                            std::find(rows.begin(), rows.end(), row.TrackId) == rows.end())
                        {
                            rows.push_back(row.TrackId);
                        }
                    }
                }

                auto const anchor = std::find(rows.begin(), rows.end(), m_selectionAnchorId);
                auto const clicked = std::find(rows.begin(), rows.end(), trackId);

                if (anchor == rows.end() || clicked == rows.end())
                {
                    SelectTrack(trackId);
                    return;
                }

                if (!control)
                {
                    m_selectedTrackIds.clear();
                }

                for (auto it = std::min(anchor, clicked); it <= std::max(anchor, clicked); ++it)
                {
                    if (!IsTrackSelected(*it))
                    {
                        m_selectedTrackIds.push_back(*it);
                    }
                }

                m_selectedTrackId = trackId;
            }
            else if (IsTrackSelected(trackId))
            {
                std::erase(m_selectedTrackIds, trackId);

                if (m_selectedTrackId == trackId)
                {
                    m_selectedTrackId = m_selectedTrackIds.empty() ? std::wstring{} : m_selectedTrackIds.back();
                }

                m_selectionAnchorId = trackId;
            }
            else
            {
                m_selectedTrackIds.push_back(trackId);
                m_selectedTrackId = trackId;
                m_selectionAnchorId = trackId;
            }

            m_selectedPlacement = SIZE_MAX;
            RefreshHeaderBackgrounds();
            FollowSelectionInInspector();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to select tracks.")
    }

    void MainWindow::RefreshHeaderBackgrounds() noexcept
    {
        try
        {
            for (auto const* headers : { &m_pinnedHeaders, &m_scrollHeaders })
            {
                for (auto const& [row, element] : *headers)
                {
                    if (auto const grid = element.try_as<controls::Grid>(); grid != nullptr && row.Kind != seq::ArrangeRowKind::AddTrack)
                    {
                        grid.Background(HeaderBackground(row));
                    }
                }
            }

            // A style that draws the rows reads which are selected when it draws them.
            m_headerPlates.Invalidate();
            UpdateMixerSelection();
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- dragging tracks

    _Use_decl_annotations_
    void MainWindow::OnHeaderPressed(std::wstring const& trackId, bool pinned, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? PinnedHeaderCanvas() : ScrollHeaderCanvas();

            if (!args.GetCurrentPoint(canvas).Properties().IsLeftButtonPressed() || m_rowResize.Active)
            {
                return;
            }

            TrackDrag drag{};
            drag.Pressed = true;
            drag.Pinned = pinned;
            drag.Start = args.GetCurrentPoint(ArrangeGrid()).Position();

            // The selected tracks go together when this is one of them.
            drag.TrackIds = IsTrackSelected(trackId) && m_selectedTrackIds.size() > 1 ? SelectedTrackIds() : std::vector<std::wstring>{ trackId };
            m_trackDrag = std::move(drag);

            // Not handled: a click still selects the track, and a double click still opens it.
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start dragging a track.")
    }

    _Use_decl_annotations_
    void MainWindow::OnHeaderMoved(bool, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_trackDrag.Pressed)
            {
                return;
            }

            auto const point = args.GetCurrentPoint(ArrangeGrid());

            // The button went up somewhere this didn't see.
            if (!point.Properties().IsLeftButtonPressed())
            {
                FinishTrackDrag(false);
                return;
            }

            if (!m_trackDrag.Active)
            {
                auto const dx = static_cast<double>(point.Position().X) - static_cast<double>(m_trackDrag.Start.X);
                auto const dy = static_cast<double>(point.Position().Y) - static_cast<double>(m_trackDrag.Start.Y);

                if (std::hypot(dx, dy) < DragThreshold)
                {
                    return;
                }

                auto const canvas = m_trackDrag.Pinned ? PinnedHeaderCanvas() : ScrollHeaderCanvas();

                if (!canvas.CapturePointer(args.Pointer()))
                {
                    m_trackDrag = TrackDrag{};
                    return;
                }

                m_trackDrag.Active = true;

                // The tracks being moved fade until they land.
                for (auto const* headers : { &m_pinnedHeaders, &m_scrollHeaders })
                {
                    for (auto const& [row, element] : *headers)
                    {
                        auto const moving = std::find(m_trackDrag.TrackIds.begin(), m_trackDrag.TrackIds.end(), row.TrackId) != m_trackDrag.TrackIds.end();
                        element.Opacity(moving ? 0.45 : 1.0);
                    }
                }

                m_headerPlates.Invalidate();
                StartDragScroll();
            }

            m_dragPoint = point.Position();
            UpdateTrackDrop(m_dragPoint);
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag a track.")
    }

    _Use_decl_annotations_
    void MainWindow::UpdateTrackDrop(foundation::Point inArrange) noexcept
    {
        try
        {
            auto& drag = m_trackDrag;
            drag.Valid = false;
            drag.FolderId.clear();
            drag.BeforeId.clear();
            drag.IntoFolderId.clear();

            auto const canvas = ScrollHeaderCanvas();
            auto const origin = canvas.TransformToVisual(ArrangeGrid()).TransformPoint(foundation::Point{ 0, 0 });
            auto const y = static_cast<double>(inArrange.Y) - static_cast<double>(origin.Y);

            auto const parentOf = [this](std::wstring const& id)
            {
                auto const parent = seq::ParentFolder(m_doc, id);
                return parent != nullptr ? parent->Id : std::wstring{};
            };

            // The next track in the same list, or none at its end.
            auto const nextSibling = [this](std::wstring const& id)
            {
                auto location = seq::LocateTrack(m_doc, id);

                if (location.Container == nullptr || location.Index + 1 >= location.Container->size())
                {
                    return std::wstring{};
                }

                return (*location.Container)[location.Index + 1].Id;
            };

            // Where the line is drawn, in the canvas, and how deep its tracks would sit.
            double lineY{ -1 };
            seq::ArrangeRow const* box{ nullptr };

            auto const& rows = m_layout.Scrolling;
            auto const bottomOf = [&rows](size_t index)
            {
                // A track's lanes go with it.
                auto end = index;

                while (end + 1 < rows.size() && rows[end + 1].Kind == seq::ArrangeRowKind::Lane && rows[end + 1].TrackId == rows[index].TrackId)
                {
                    ++end;
                }

                return rows[end].Top + rows[end].Height;
            };

            auto const first = std::find_if(rows.begin(), rows.end(), [](seq::ArrangeRow const& row)
            {
                return row.Kind == seq::ArrangeRowKind::Track || row.Kind == seq::ArrangeRowKind::Folder;
            });

            if (y < 0 && first != rows.end())
            {
                // Above the rows that scroll: before the first of them.
                drag.FolderId = parentOf(first->TrackId);
                drag.BeforeId = first->TrackId;
                lineY = first->Top;
            }
            else
            {
                auto const at = std::clamp(y, 0.0, std::max(0.0, canvas.ActualHeight() - 1.0)) + m_scrollY;
                auto const row = m_layout.ScrollingRowAt(at);
                auto const index = row != nullptr ? static_cast<size_t>(row - rows.data()) : SIZE_MAX;

                if (row == nullptr || row->Kind == seq::ArrangeRowKind::AddTrack || row->TrackId.empty())
                {
                    // Past the last track: the end of the top level.
                    auto const add = std::find_if(rows.begin(), rows.end(), [](seq::ArrangeRow const& candidate) { return candidate.Kind == seq::ArrangeRowKind::AddTrack; });
                    lineY = add != rows.end() ? add->Top : m_layout.ScrollingHeight;
                }
                else if (row->Kind == seq::ArrangeRowKind::Lane)
                {
                    // Under a track's lanes: after the track.
                    drag.FolderId = parentOf(row->TrackId);
                    drag.BeforeId = nextSibling(row->TrackId);

                    auto owner = index;

                    while (owner > 0 && rows[owner].Kind == seq::ArrangeRowKind::Lane)
                    {
                        --owner;
                    }

                    lineY = bottomOf(owner);
                }
                else
                {
                    auto const fraction = (at - row->Top) / std::max(1.0, row->Height);
                    auto const folder = seq::FindTrack(m_doc, row->TrackId);
                    auto const isFolder = folder != nullptr && folder->IsFolder;

                    if (isFolder && fraction >= 0.25 && fraction <= 0.75)
                    {
                        // Over a folder's own row: into it, at its end.
                        drag.FolderId = row->TrackId;
                        drag.IntoFolderId = row->TrackId;
                        box = row;
                    }
                    else if (fraction < (isFolder ? 0.25 : 0.5))
                    {
                        drag.FolderId = parentOf(row->TrackId);
                        drag.BeforeId = row->TrackId;
                        lineY = row->Top;
                    }
                    else if (isFolder && folder->Open && !folder->Children.empty())
                    {
                        // Under an open folder's row is its first track.
                        drag.FolderId = row->TrackId;
                        drag.BeforeId = folder->Children.front().Id;
                        lineY = row->Top + row->Height;
                    }
                    else
                    {
                        drag.FolderId = parentOf(row->TrackId);
                        drag.BeforeId = nextSibling(row->TrackId);
                        lineY = isFolder ? row->Top + row->Height : bottomOf(index);
                    }
                }
            }

            // Not into a track that's moving, or into a folder inside one.
            drag.Valid = true;

            for (auto const& id : drag.TrackIds)
            {
                if (!drag.FolderId.empty() && seq::IsInside(m_doc, drag.FolderId, id))
                {
                    drag.Valid = false;
                }
            }

            // How deep the tracks would sit, for the line's indent.
            size_t depth{ 0 };

            if (!drag.FolderId.empty())
            {
                seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t level)
                {
                    if (track.Id == drag.FolderId)
                    {
                        depth = level + 1;
                        return false;
                    }

                    return true;
                });
            }

            if (m_dropLine == nullptr)
            {
                m_dropLine = shapes::Rectangle{};
                m_dropLine.Height(2);
                m_dropLine.IsHitTestVisible(false);
                m_dropLine.Fill(BrushFor(m_palette.Accent));

                m_dropBox = shapes::Rectangle{};
                m_dropBox.StrokeThickness(2);
                m_dropBox.IsHitTestVisible(false);
                m_dropBox.Stroke(BrushFor(m_palette.Accent));
                m_dropBox.Fill(BrushFor(seq::WithAlpha(m_palette.Accent, 0.10)));
            }

            // Rebuilding the headers clears the canvas, so they go back on if they're gone.
            for (auto const& shape : { m_dropLine, m_dropBox })
            {
                if (shape.Parent() == nullptr)
                {
                    canvas.Children().Append(shape);
                }
            }

            auto const indent = 4.0 + DepthIndent * static_cast<double>(depth);
            m_dropLine.Width(std::max(10.0, m_headerWidth - indent - 4.0));
            controls::Canvas::SetLeft(m_dropLine, indent);
            controls::Canvas::SetTop(m_dropLine, lineY - m_scrollY - 1.0);
            m_dropLine.Visibility(drag.Valid && box == nullptr && lineY >= 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (box != nullptr)
            {
                m_dropBox.Width(m_headerWidth);
                m_dropBox.Height(box->Height);
                controls::Canvas::SetLeft(m_dropBox, 0);
                controls::Canvas::SetTop(m_dropBox, box->Top - m_scrollY);
            }

            m_dropBox.Visibility(drag.Valid && box != nullptr ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show where a track would go.")
    }

    _Use_decl_annotations_
    void MainWindow::FinishTrackDrag(bool drop) noexcept
    {
        try
        {
            // Cleared first: letting go of the capture comes straight back here.
            auto drag = std::move(m_trackDrag);
            m_trackDrag = TrackDrag{};

            PinnedHeaderCanvas().ReleasePointerCaptures();
            ScrollHeaderCanvas().ReleasePointerCaptures();

            for (auto const& shape : { m_dropLine, m_dropBox })
            {
                if (shape != nullptr)
                {
                    if (auto const parent = shape.Parent().try_as<controls::Panel>())
                    {
                        uint32_t index{ 0 };

                        if (parent.Children().IndexOf(shape, index))
                        {
                            parent.Children().RemoveAt(index);
                        }
                    }
                }
            }

            for (auto const* headers : { &m_pinnedHeaders, &m_scrollHeaders })
            {
                for (auto const& [row, element] : *headers)
                {
                    element.Opacity(1.0);
                }
            }

            m_headerPlates.Invalidate();

            if (!drop || !drag.Active || !drag.Valid || drag.TrackIds.empty())
            {
                return;
            }

            auto before = m_doc.Tracks;

            if (!seq::MoveTracks(m_doc, drag.TrackIds, drag.FolderId, drag.BeforeId))
            {
                return;
            }

            // Dropped into a closed folder, it opens so the tracks can still be seen.
            if (!drag.IntoFolderId.empty())
            {
                if (auto const folder = seq::FindTrack(m_doc, drag.IntoFolderId); folder != nullptr)
                {
                    folder->Open = true;
                }
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(std::move(before), m_doc.Tracks));
            Commit(std::wstring{ res::GetString(L"UndoMoveTrack") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move tracks.")
    }

    // ---------------------------------------------------------------- dragging clips

    _Use_decl_annotations_
    MainWindow::ArrangeSpot MainWindow::ArrangeSpotAt(foundation::Point inArrange) noexcept
    {
        try
        {
            auto const within = [&](canvasXaml::CanvasControl const& canvas, ArrangeSpot::Area area, bool pinned) -> std::optional<ArrangeSpot>
            {
                if (canvas == nullptr || canvas.ActualWidth() < 1 || canvas.ActualHeight() < 1)
                {
                    return std::nullopt;
                }

                auto const origin = canvas.TransformToVisual(ArrangeGrid()).TransformPoint(foundation::Point{ 0, 0 });
                foundation::Point const point{ inArrange.X - origin.X, inArrange.Y - origin.Y };

                if (point.X < 0 || point.Y < 0 || point.X >= canvas.ActualWidth() || point.Y >= canvas.ActualHeight())
                {
                    return std::nullopt;
                }

                return ArrangeSpot{ area, pinned, point };
            };

            for (auto const pinned : { true, false })
            {
                if (auto spot = within(pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas, ArrangeSpot::Area::Timeline, pinned))
                {
                    return *spot;
                }

                if (auto spot = within(pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas, ArrangeSpot::Area::Launcher, pinned))
                {
                    return *spot;
                }
            }
        }
        catch (...)
        {
        }

        return ArrangeSpot{};
    }

    _Use_decl_annotations_
    void MainWindow::BeginSlotDrag(bool pinned, std::wstring const& trackId, size_t scene, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || scene >= track->Slots.size() || track->Slots[scene].empty())
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, track->Slots[scene]);

            if (clip == nullptr)
            {
                return;
            }

            ClipDrag drag{};
            drag.Pressed = true;
            drag.FromLauncher = true;
            drag.Pinned = pinned;
            drag.Start = args.GetCurrentPoint(ArrangeGrid()).Position();
            drag.ClipId = clip->Id;
            drag.FromTrackId = trackId;
            drag.FromSlot = scene;
            drag.Length = 0;
            m_clipDrag = std::move(drag);

            (pinned ? m_pinnedLauncherCanvas : m_scrollLauncherCanvas).CapturePointer(args.Pointer());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start dragging a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherMoved(bool, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_clipDrag.Pressed)
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
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag a clip.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLauncherReleased(bool, bool canceled)
    {
        if (m_clipDrag.Pressed)
        {
            FinishClipDrag(!canceled);
        }
    }

    _Use_decl_annotations_
    void MainWindow::UpdateClipDrop(foundation::Point inArrange, bool copy, bool keepTrack) noexcept
    {
        try
        {
            auto& view = m_clipDrag.View;
            view = seq::ClipDragView{};
            view.ClipId = m_clipDrag.ClipId;
            view.FromTrackId = m_clipDrag.FromTrackId;
            view.FromPlacement = m_clipDrag.FromLauncher ? SIZE_MAX : m_clipDrag.FromPlacement;
            view.FromSlot = m_clipDrag.FromLauncher ? m_clipDrag.FromSlot : SIZE_MAX;
            view.Length = m_clipDrag.Length;

            if (keepTrack)
            {
                // Where the pointer is across a canvas, from its left edge, when it's over its
                // columns. Up and down don't matter here.
                auto const across = [&](canvasXaml::CanvasControl const& canvas) -> std::optional<double>
                {
                    if (canvas == nullptr || canvas.ActualWidth() < 1)
                    {
                        return std::nullopt;
                    }

                    auto const origin = canvas.TransformToVisual(ArrangeGrid()).TransformPoint(foundation::Point{ 0, 0 });
                    auto const x = static_cast<double>(inArrange.X) - static_cast<double>(origin.X);

                    return x >= 0 && x < canvas.ActualWidth() ? std::optional<double>{ x } : std::nullopt;
                };

                // Shift keeps the clip on the track it came from, wherever the pointer goes up or
                // down. Left and right still move it along the timeline, between slots, and from
                // one to the other.
                if (auto const track = seq::FindTrack(m_doc, m_clipDrag.FromTrackId); track != nullptr && !track->IsFolder)
                {
                    auto lane = across(m_scrollLaneCanvas);
                    auto launcher = across(m_scrollLauncherCanvas);

                    if (!lane.has_value())
                    {
                        lane = across(m_pinnedLaneCanvas);
                    }

                    if (!launcher.has_value())
                    {
                        launcher = across(m_pinnedLauncherCanvas);
                    }

                    if (lane.has_value())
                    {
                        auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);

                        view.ToTrackId = track->Id;
                        view.ToTick = std::max<int64_t>(0, seq::SnapTick(TickAtLaneX(*lane) - m_clipDrag.GrabOffset, grid));
                        view.Copy = copy || m_clipDrag.FromLauncher;
                    }
                    else if (launcher.has_value())
                    {
                        if (auto const scene = SceneAtLauncherX(*launcher); scene.has_value())
                        {
                            view.ToTrackId = track->Id;
                            view.ToSlot = *scene;
                            view.Copy = copy || !m_clipDrag.FromLauncher;
                        }
                    }
                }

                if (view.ToTrackId.empty())
                {
                    view.Copy = true;
                }

                InvalidateArrange();
                InvalidateLaunchers();
                return;
            }

            auto const spot = ArrangeSpotAt(inArrange);

            if (spot.Where == ArrangeSpot::Area::Timeline)
            {
                auto const hit = HitTestLane(spot.Pinned, spot.Position);

                if (hit.Row != nullptr && hit.Row->Kind == seq::ArrangeRowKind::Track)
                {
                    // Clips move by whole beats unless snap is coarser, so they stay on the grid.
                    auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);

                    view.ToTrackId = hit.Row->TrackId;
                    view.ToTick = std::max<int64_t>(0, seq::SnapTick(hit.Tick - m_clipDrag.GrabOffset, grid));

                    // Out of the launcher it's another placement; the slot keeps its clip.
                    view.Copy = copy || m_clipDrag.FromLauncher;
                }
            }
            else if (spot.Where == ArrangeSpot::Area::Launcher)
            {
                if (auto const slot = HitTestSlot(spot.Pinned, spot.Position); slot.has_value())
                {
                    view.ToTrackId = slot->first;
                    view.ToSlot = slot->second;

                    // Off the timeline it's linked into the slot; the placement stays.
                    view.Copy = copy || !m_clipDrag.FromLauncher;
                }
            }

            if (view.ToTrackId.empty())
            {
                view.Copy = true;
            }

            InvalidateArrange();
            InvalidateLaunchers();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show where a clip would go.")
    }

    _Use_decl_annotations_
    void MainWindow::FinishClipDrag(bool drop) noexcept
    {
        try
        {
            // Cleared first: letting go of the capture comes straight back here.
            auto drag = std::move(m_clipDrag);
            m_clipDrag = ClipDrag{};

            for (auto const& canvas : { m_pinnedLaneCanvas, m_scrollLaneCanvas, m_pinnedLauncherCanvas, m_scrollLauncherCanvas })
            {
                if (canvas != nullptr)
                {
                    canvas.ReleasePointerCaptures();
                }
            }

            InvalidateArrange();
            InvalidateLaunchers();

            if (!drop || !drag.Active || drag.View.ToTrackId.empty())
            {
                return;
            }

            auto const& view = drag.View;
            auto before = m_doc.Tracks;
            auto target = seq::FindTrack(m_doc, view.ToTrackId);
            auto source = seq::FindTrack(m_doc, drag.FromTrackId);

            if (target == nullptr || target->IsFolder || seq::FindClip(m_doc, view.ClipId) == nullptr)
            {
                return;
            }

            auto const undoName = std::wstring{ res::GetString(
                !view.Copy ? L"UndoMoveClip" :
                (view.ToTick >= 0) == drag.FromLauncher ? L"UndoPlaceClip" :
                L"UndoCopyClip") };

            if (view.ToTick >= 0)
            {
                seq::Placement placed{ view.ClipId, view.ToTick, view.Length };

                if (!drag.FromLauncher && !view.Copy)
                {
                    if (source == nullptr || drag.FromPlacement >= source->Timeline.size() || source->Timeline[drag.FromPlacement].ClipId != view.ClipId)
                    {
                        return;
                    }

                    // Where it already was: nothing to do.
                    if (source == target && source->Timeline[drag.FromPlacement].Tick == view.ToTick)
                    {
                        return;
                    }

                    placed.Length = source->Timeline[drag.FromPlacement].Length;
                    source->Timeline.erase(source->Timeline.begin() + static_cast<ptrdiff_t>(drag.FromPlacement));
                }
                else if (!drag.FromLauncher && source != nullptr && drag.FromPlacement < source->Timeline.size())
                {
                    placed.Length = source->Timeline[drag.FromPlacement].Length;
                }

                target->Timeline.push_back(placed);
                std::stable_sort(target->Timeline.begin(), target->Timeline.end(), [](seq::Placement const& a, seq::Placement const& b) { return a.Tick < b.Tick; });

                SelectTrack(view.ToTrackId);

                for (size_t i = target->Timeline.size(); i-- > 0;)
                {
                    if (target->Timeline[i] == placed)
                    {
                        m_selectedPlacement = i;
                        break;
                    }
                }

                seq::ChangeList changes{};
                changes.push_back(seq::MakeTracksChange(std::move(before), m_doc.Tracks));
                Commit(undoName, std::move(changes));
                return;
            }

            if (view.ToSlot == SIZE_MAX || view.ToSlot >= m_doc.Scenes.size())
            {
                return;
            }

            if (drag.FromLauncher && target == source && view.ToSlot == drag.FromSlot)
            {
                return;
            }

            if (target->Slots.size() < m_doc.Scenes.size())
            {
                target->Slots.resize(m_doc.Scenes.size());
            }

            if (drag.FromLauncher && !view.Copy && source != nullptr && drag.FromSlot < source->Slots.size() && source->Slots[drag.FromSlot] == view.ClipId)
            {
                source->Slots[drag.FromSlot].clear();
            }

            target->Slots[view.ToSlot] = view.ClipId;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeTracksChange(std::move(before), m_doc.Tracks));

            // A clip the slot held, and nothing else uses, goes with it.
            for (auto& clip : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(clip), false));
            }

            if (!m_editorClipId.empty() && seq::FindClip(m_doc, m_editorClipId) == nullptr)
            {
                CloseEditor();
            }

            m_selectedSlotTrackId = view.ToTrackId;
            m_selectedSlot = view.ToSlot;
            Commit(undoName, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move a clip.")
    }

    void MainWindow::StartDragScroll() noexcept
    {
        try
        {
            if (m_dragScrollTimer != nullptr)
            {
                m_dragScrollTimer.Start();
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::OnDragScrollTick() noexcept
    {
        try
        {
            if (!m_trackDrag.Active && !m_clipDrag.Active)
            {
                m_dragScrollTimer.Stop();
                return;
            }

            // Up or down near the top or the bottom of the rows that scroll.
            auto const area = ScrollHeaderCanvas();
            auto const origin = area.TransformToVisual(ArrangeGrid()).TransformPoint(foundation::Point{ 0, 0 });
            auto const y = static_cast<double>(m_dragPoint.Y) - static_cast<double>(origin.Y);
            auto const height = area.ActualHeight();

            double dy{ 0 };

            if (y < ScrollMargin && y > -ScrollMargin * 3)
            {
                dy = -ScrollStep * std::clamp((ScrollMargin - y) / ScrollMargin, 0.25, 2.0);
            }
            else if (y > height - ScrollMargin)
            {
                dy = ScrollStep * std::clamp((y - (height - ScrollMargin)) / ScrollMargin, 0.25, 2.0);
            }

            // A clip Shift keeps on its own track has nowhere to go up or down.
            if (m_clipDrag.Active && m_dragKeepTrack)
            {
                dy = 0;
            }

            // Left or right near the timeline's ends, for a clip.
            double dx{ 0 };

            if (m_clipDrag.Active && m_scrollLaneCanvas != nullptr)
            {
                auto const lanes = m_scrollLaneCanvas.TransformToVisual(ArrangeGrid()).TransformPoint(foundation::Point{ 0, 0 });
                auto const x = static_cast<double>(m_dragPoint.X) - static_cast<double>(lanes.X);
                auto const width = LaneWidth();

                if (x >= 0 && x < ScrollMargin)
                {
                    dx = -ScrollStep * std::clamp((ScrollMargin - x) / ScrollMargin, 0.25, 2.0);
                }
                else if (x > width - ScrollMargin && x < width + ScrollMargin * 3)
                {
                    dx = ScrollStep * std::clamp((x - (width - ScrollMargin)) / ScrollMargin, 0.25, 2.0);
                }
            }

            if (dy == 0 && dx == 0)
            {
                return;
            }

            auto const scrollY = m_scrollY;
            auto const scrollX = m_scrollX;

            m_scrollY = std::max(0.0, m_scrollY + dy);
            m_scrollX = std::max(0.0, m_scrollX + dx);
            UpdateScrollBars();

            if (m_scrollY == scrollY && m_scrollX == scrollX)
            {
                return;
            }

            PositionHeaders();
            InvalidateArrange();
            InvalidateLaunchers();

            if (m_trackDrag.Active)
            {
                UpdateTrackDrop(m_dragPoint);
            }

            if (m_clipDrag.Active)
            {
                UpdateClipDrop(m_dragPoint, m_dragCopy, m_dragKeepTrack);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to scroll while dragging.")
    }

    // ---------------------------------------------------------------- tags

    _Use_decl_annotations_
    MainWindow::TagHit MainWindow::HitTestTag(seq::ArrangeRow const& row, float rowTop, foundation::Point point) noexcept
    {
        TagHit hit{};

        try
        {
            auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
            auto const height = static_cast<float>(row.Height);

            auto const search = [&](std::vector<seq::Tag> const& tags, bool atTop)
            {
                // The last one is drawn on top.
                for (size_t i = tags.size(); i-- > 0;)
                {
                    auto const x = static_cast<float>(static_cast<double>(tags[i].Tick) * pixelsPerTick - m_scrollX);

                    if (Contains(m_renderer.TagLabelBounds(tags[i], x, rowTop, height, atTop), point))
                    {
                        return i;
                    }
                }

                return SIZE_MAX;
            };

            if (row.Kind == seq::ArrangeRowKind::Tempo)
            {
                hit.Index = search(m_doc.Tags, true);
            }
            else if (row.Kind == seq::ArrangeRowKind::Track)
            {
                if (auto const track = seq::FindTrack(m_doc, row.TrackId); track != nullptr)
                {
                    hit.Index = search(track->Tags, false);
                    hit.TrackId = track->Id;
                }
            }
        }
        catch (...)
        {
            hit = TagHit{};
        }

        return hit;
    }

    _Use_decl_annotations_
    void MainWindow::BeginTagDrag(bool pinned, TagHit const& hit, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            std::vector<seq::Tag> const* tags{ &m_doc.Tags };

            if (!hit.TrackId.empty())
            {
                auto const track = seq::FindTrack(m_doc, hit.TrackId);
                tags = track != nullptr ? &track->Tags : nullptr;
            }

            if (tags == nullptr || hit.Index >= tags->size())
            {
                return;
            }

            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;

            TagDrag drag{};
            drag.Active = true;
            drag.Pinned = pinned;
            drag.Tag = hit;
            drag.Start = args.GetCurrentPoint(canvas).Position();
            drag.OriginalTick = (*tags)[hit.Index].Tick;
            drag.Tick = drag.OriginalTick;
            m_tagDrag = std::move(drag);

            ClearAutomationSelection();
            m_selectedPlacement = SIZE_MAX;
            canvas.CapturePointer(args.Pointer());
            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to pick up a tag.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTagDragMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = m_tagDrag.Pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const x = static_cast<double>(args.GetCurrentPoint(canvas).Position().X);
            auto const dx = x - static_cast<double>(m_tagDrag.Start.X);

            if (!m_tagDrag.Moved && std::abs(dx) < DragThreshold)
            {
                return;
            }

            m_tagDrag.Moved = true;

            // The same grid a new tag snaps to.
            auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);
            auto const ticks = static_cast<int64_t>(std::llround(dx / (m_barWidth / TicksPerFourFourBar)));
            m_tagDrag.Tick = std::max<int64_t>(0, seq::SnapTick(m_tagDrag.OriginalTick + ticks, grid));

            // Moved in place while it's dragged, and made one edit when it's let go.
            auto const track = m_tagDrag.Tag.TrackId.empty() ? nullptr : seq::FindTrack(m_doc, m_tagDrag.Tag.TrackId);
            auto& tags = track != nullptr ? track->Tags : m_doc.Tags;

            if ((m_tagDrag.Tag.TrackId.empty() || track != nullptr) && m_tagDrag.Tag.Index < tags.size())
            {
                tags[m_tagDrag.Tag.Index].Tick = m_tagDrag.Tick;
            }

            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag a tag.")
    }

    void MainWindow::FinishTagDrag() noexcept
    {
        try
        {
            // Cleared first: letting go of the capture comes straight back here.
            auto drag = std::move(m_tagDrag);
            m_tagDrag = TagDrag{};

            if (m_pinnedLaneCanvas != nullptr) { m_pinnedLaneCanvas.ReleasePointerCaptures(); }
            if (m_scrollLaneCanvas != nullptr) { m_scrollLaneCanvas.ReleasePointerCaptures(); }

            // It moved in place as it was dragged: back where it was first, so undo has the start.
            auto const track = drag.Tag.TrackId.empty() ? nullptr : seq::FindTrack(m_doc, drag.Tag.TrackId);

            if (!drag.Tag.TrackId.empty() && track == nullptr)
            {
                InvalidateArrange();
                return;
            }

            auto& tags = track != nullptr ? track->Tags : m_doc.Tags;

            if (drag.Tag.Index < tags.size())
            {
                tags[drag.Tag.Index].Tick = drag.OriginalTick;
            }

            if (!drag.Moved || drag.Tick == drag.OriginalTick)
            {
                InvalidateArrange();
                return;
            }

            EditTag(drag.Tag, std::wstring{ res::GetString(L"UndoMoveTag") }, [&drag](std::vector<seq::Tag>& edited, size_t index)
            {
                edited[index].Tick = drag.Tick;
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move a tag.")
    }

    _Use_decl_annotations_
    void MainWindow::EditTag(TagHit const& hit, std::wstring const& undoName, std::function<void(std::vector<seq::Tag>&, size_t)> const& edit)
    {
        if (hit.TrackId.empty())
        {
            if (hit.Index >= m_doc.Tags.size())
            {
                return;
            }

            auto before = m_doc.Tags;
            auto after = before;
            edit(after, hit.Index);
            SortTags(after);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeSequenceTagsChange(std::move(before), after));
            m_doc.Tags = std::move(after);
            Commit(undoName, std::move(changes));
            return;
        }

        auto const track = seq::FindTrack(m_doc, hit.TrackId);

        if (track == nullptr || hit.Index >= track->Tags.size())
        {
            return;
        }

        EditTracks(undoName, [&](seq::Sequence& doc)
        {
            if (auto const t = seq::FindTrack(doc, hit.TrackId); t != nullptr && hit.Index < t->Tags.size())
            {
                edit(t->Tags, hit.Index);
                SortTags(t->Tags);
            }
        });
    }

    _Use_decl_annotations_
    void MainWindow::RenameTag(TagHit const& hit)
    {
        std::vector<seq::Tag> const* tags{ &m_doc.Tags };

        if (!hit.TrackId.empty())
        {
            auto const track = seq::FindTrack(m_doc, hit.TrackId);
            tags = track != nullptr ? &track->Tags : nullptr;
        }

        if (tags == nullptr || hit.Index >= tags->size())
        {
            return;
        }

        // Found again by tick and text when the prompt closes, in case the tags moved meanwhile.
        auto const original = (*tags)[hit.Index];

        ShowTextPrompt(res::GetString(L"RenameTagTitle"), winrt::hstring{ original.Text }, [weak = get_weak(), trackId = hit.TrackId, original](std::wstring const& text)
        {
            auto strong = weak.get();

            if (!strong || text.empty())
            {
                return;
            }

            std::vector<seq::Tag> const* current{ &strong->m_doc.Tags };

            if (!trackId.empty())
            {
                auto const track = seq::FindTrack(strong->m_doc, trackId);
                current = track != nullptr ? &track->Tags : nullptr;
            }

            if (current == nullptr)
            {
                return;
            }

            auto const found = std::find(current->begin(), current->end(), original);

            if (found == current->end())
            {
                return;
            }

            TagHit again{ trackId, static_cast<size_t>(std::distance(current->begin(), found)) };
            auto const renamed = text.substr(0, 200);

            strong->EditTag(again, std::wstring{ res::GetString(L"UndoRenameTag") }, [&renamed](std::vector<seq::Tag>& tags, size_t index)
            {
                tags[index].Text = renamed;
            });
        });
    }

    _Use_decl_annotations_
    void MainWindow::ShowTagMenu(TagHit const& hit, xaml::UIElement const& anchor, foundation::Point point)
    {
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

            item.Click([action = std::move(action)](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        add(res::GetString(L"MenuRenameTag"), L"\uE8AC", [weak, hit]() { if (auto s = weak.get()) { s->RenameTag(hit); } });

        // Colors, the same swatches as tracks, and none for the standard gray.
        {
            controls::MenuFlyoutSubItem colors{};
            colors.Text(res::GetString(L"MenuColor"));

            controls::FontIcon icon{};
            icon.Glyph(L"\uE790");
            colors.Icon(icon);

            auto const setColor = [weak, hit](std::optional<uint32_t> color)
            {
                if (auto s = weak.get())
                {
                    s->EditTag(hit, std::wstring{ res::GetString(L"UndoTagColor") }, [color](std::vector<seq::Tag>& tags, size_t index)
                    {
                        tags[index].Color = color;
                    });
                }
            };

            controls::MenuFlyoutItem standard{};
            standard.Text(res::GetString(L"MenuTagStandardColor"));
            standard.Click([setColor](auto&&, auto&&) { setColor(std::nullopt); });
            colors.Items().Append(standard);

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

                item.Click([setColor, swatch](auto&&, auto&&) { setColor(swatch); });
                colors.Items().Append(item);
            }

            menu.Items().Append(colors);
        }

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        add(res::GetString(L"MenuDeleteTag"), L"\uE74D", [weak, hit]()
        {
            auto s = weak.get();

            if (!s)
            {
                return;
            }

            if (hit.TrackId.empty())
            {
                if (hit.Index >= s->m_doc.Tags.size())
                {
                    return;
                }

                auto before = s->m_doc.Tags;
                auto after = before;
                after.erase(after.begin() + static_cast<ptrdiff_t>(hit.Index));

                seq::ChangeList changes{};
                changes.push_back(seq::MakeSequenceTagsChange(std::move(before), after));
                s->m_doc.Tags = std::move(after);
                s->Commit(std::wstring{ res::GetString(L"UndoDeleteTag") }, std::move(changes));
                return;
            }

            s->EditTracks(std::wstring{ res::GetString(L"UndoDeleteTag") }, [&](seq::Sequence& doc)
            {
                if (auto const t = seq::FindTrack(doc, hit.TrackId); t != nullptr && hit.Index < t->Tags.size())
                {
                    t->Tags.erase(t->Tags.begin() + static_cast<ptrdiff_t>(hit.Index));
                }
            });
        });

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    // ---------------------------------------------------------------- the loop

    _Use_decl_annotations_
    MainWindow::LoopGrab MainWindow::HitTestLoop(foundation::Point point) const noexcept
    {
        try
        {
            if (m_rulerCanvas == nullptr || m_loopEnd <= m_loopStart)
            {
                return LoopGrab::None;
            }

            // The loop is the band along the ruler's bottom.
            auto const height = m_rulerCanvas.ActualHeight();

            if (point.Y < height - 12.0)
            {
                return LoopGrab::None;
            }

            auto const pixelsPerTick = m_barWidth / TicksPerFourFourBar;
            auto const x0 = static_cast<double>(m_loopStart) * pixelsPerTick - m_scrollX;
            auto const x1 = static_cast<double>(m_loopEnd) * pixelsPerTick - m_scrollX;
            auto const x = static_cast<double>(point.X);

            if (std::abs(x - x0) <= 5.0)
            {
                return LoopGrab::Start;
            }

            if (std::abs(x - x1) <= 5.0)
            {
                return LoopGrab::End;
            }

            return x > x0 && x < x1 ? LoopGrab::Whole : LoopGrab::None;
        }
        catch (...)
        {
            return LoopGrab::None;
        }
    }
}
