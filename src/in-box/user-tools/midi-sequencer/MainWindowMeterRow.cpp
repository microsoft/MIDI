// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Meter row (design section 37; Pete, November 7 2026, after Cubase's Signature track):
// showing and hiding it, its header, and picking, dragging, adding and removing meter changes.
// Moving a meter change moves bar lines, not notes: notes keep their time.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double MeterDragThreshold = 4.0;
        constexpr double MeterFlagWidth = 36.0;
        constexpr double TicksPerFourFourBar = static_cast<double>(seq::TicksPerQuarterNote * 4);

        double PixelsPerTick(double barWidth) noexcept
        {
            return barWidth / TicksPerFourFourBar;
        }

        // The bar whose start is nearest a tick, counted from 1.
        int64_t NearestBar(std::vector<seq::MeterChange> const& meter, int64_t tick) noexcept
        {
            auto const bar = seq::BarPositionAtTick(meter, std::max<int64_t>(0, tick)).Bar;
            auto const start = seq::TickAtBar(meter, bar);
            auto const next = seq::TickAtBar(meter, bar + 1);
            return (tick - start) * 2 >= (next - start) ? bar + 1 : bar;
        }
    }

    // ---------------------------------------------------------------- showing the row

    _Use_decl_annotations_
    void MainWindow::ShowMeterRow(bool show) noexcept
    {
        try
        {
            if (m_doc.ShowMeterRow == show)
            {
                return;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeMeterRowChange(m_doc.ShowMeterRow, show));
            m_doc.ShowMeterRow = show;
            m_selectedMeter = SIZE_MAX;
            Commit(std::wstring{ res::GetString(show ? L"UndoShowMeterRow" : L"UndoHideMeterRow") }, std::move(changes));
            RebuildLayout();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show or hide the Meter row.")
    }

    // ---------------------------------------------------------------- the header

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeMeterHeader(seq::ArrangeRow const& row)
    {
        controls::Grid grid{};
        grid.Width(m_headerWidth);
        grid.Height(row.Height);
        grid.BorderThickness(xaml::Thickness{ 0, 0, 1, 1 });
        grid.BorderBrush(BrushFor(m_palette.Divider));
        grid.Background(HeaderBackground(row));
        grid.ColumnSpacing(6);
        grid.Padding(xaml::Thickness{ 0, 0, 8, 0 });

        // The same columns as the other headers, so the icon and name line up with theirs.
        for (auto const width : { xaml::GridLength{ 4, xaml::GridUnitType::Pixel }, xaml::GridLength{ 0, xaml::GridUnitType::Pixel },
                 xaml::GridLength{ 14, xaml::GridUnitType::Pixel }, xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                 xaml::GridLength{ 1, xaml::GridUnitType::Star } })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            grid.ColumnDefinitions().Append(column);
        }

        auto const place = [&grid](xaml::FrameworkElement const& element, int32_t column)
        {
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
        };

        shapes::Rectangle strip{};
        strip.Fill(BrushFor(m_palette.Accent));
        place(strip, 0);

        controls::FontIcon icon{};
        icon.Glyph(L"\uE7C1");
        icon.FontSize(13);
        icon.Width(16);
        icon.Foreground(BrushFor(m_palette.Text2));
        place(icon, 3);

        // One line: the row is short. The meter at the playhead, and how many changes there are.
        auto const& meter = seq::MeterAtTick(m_doc.Meter, m_position);
        auto const changes = m_doc.Meter.size() - 1;

        controls::TextBlock name{};
        name.Text(res::GetString(L"MeterRowName") + L"  " + winrt::hstring{ std::format(L"{}/{}", meter.Numerator, meter.Denominator) } +
            L" \u00B7 " + res::FormatString(changes == 1 ? L"MeterRowOneChange" : L"MeterRowChangeCountFormat", changes));
        name.FontSize(12);
        name.VerticalAlignment(xaml::VerticalAlignment::Center);
        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        name.TextWrapping(xaml::TextWrapping::NoWrap);
        place(name, 4);

        controls::ToolTipService::SetToolTip(grid, winrt::box_value(res::GetString(L"MeterRowHelp")));
        automation::AutomationProperties::SetName(grid, res::GetString(L"MeterRowName"));
        automation::AutomationProperties::SetHelpText(grid, res::GetString(L"MeterRowHelp"));
        return grid;
    }

    // ---------------------------------------------------------------- picking and dragging

    _Use_decl_annotations_
    size_t MainWindow::HitTestMeter(foundation::Point point) const noexcept
    {
        auto const pixelsPerTick = PixelsPerTick(m_barWidth);
        auto const x = static_cast<double>(point.X);

        // Last first: a later flag is drawn over an earlier one.
        for (size_t i = m_doc.Meter.size(); i-- > 0;)
        {
            auto const left = static_cast<double>(m_doc.Meter[i].Tick) * pixelsPerTick - m_scrollX;

            if (x >= left - 3.0 && x < left + MeterFlagWidth)
            {
                return i;
            }
        }

        return SIZE_MAX;
    }

    _Use_decl_annotations_
    void MainWindow::OnMeterPressed(bool pinned, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const position = args.GetCurrentPoint(canvas).Position();
            auto const hit = HitTestMeter(position);

            ClearAutomationSelection();
            m_selectedPlacement = SIZE_MAX;
            m_selectedMeter = hit;

            // The first meter starts the song, so it can be changed but not moved.
            if (hit != SIZE_MAX && hit > 0)
            {
                MeterDrag drag{};
                drag.Active = true;
                drag.Pinned = pinned;
                drag.Index = hit;
                drag.StartX = position.X;
                drag.ToBar = seq::BarPositionAtTick(m_doc.Meter, m_doc.Meter[hit].Tick).Bar;
                drag.ToTick = m_doc.Meter[hit].Tick;
                m_meterDrag = drag;
                canvas.CapturePointer(args.Pointer());
            }

            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a click on the Meter row.")
    }

    _Use_decl_annotations_
    void MainWindow::OnMeterDragMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto& drag = m_meterDrag;

            if (drag.Index >= m_doc.Meter.size())
            {
                drag = MeterDrag{};
                return;
            }

            auto const canvas = drag.Pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const x = static_cast<double>(args.GetCurrentPoint(canvas).Position().X);

            if (!drag.Moved && std::abs(x - drag.StartX) < MeterDragThreshold)
            {
                return;
            }

            drag.Moved = true;

            // Bars as they're drawn now, so the flag lands on the bar line under the pointer.
            auto const pointerTick = static_cast<int64_t>(std::llround((x + m_scrollX) / PixelsPerTick(m_barWidth)));
            drag.ToBar = std::max<int64_t>(2, NearestBar(m_doc.Meter, pointerTick));

            // Where the flag would be once moved: later changes keep their bar numbers, so the
            // bars in between can change length.
            auto preview = m_doc.Meter;

            if (seq::MoveMeterChange(preview, drag.Index, drag.ToBar))
            {
                drag.ToTick = seq::TickAtBar(preview, drag.ToBar);
            }
            else
            {
                drag.ToTick = m_doc.Meter[drag.Index].Tick;
            }

            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag on the Meter row.")
    }

    void MainWindow::OnMeterDragReleased() noexcept
    {
        try
        {
            // Finished first: letting go of the capture comes straight back here.
            auto drag = std::exchange(m_meterDrag, MeterDrag{});

            if (!drag.Moved || drag.Index >= m_doc.Meter.size())
            {
                InvalidateArrange();
                return;
            }

            auto before = m_doc.Meter;

            if (!seq::MoveMeterChange(m_doc.Meter, drag.Index, drag.ToBar))
            {
                InvalidateArrange();
                return;
            }

            auto const tick = seq::TickAtBar(m_doc.Meter, drag.ToBar);
            m_selectedMeter = SIZE_MAX;

            for (size_t i = 0; i < m_doc.Meter.size(); ++i)
            {
                if (m_doc.Meter[i].Tick == tick)
                {
                    m_selectedMeter = i;
                    break;
                }
            }

            auto const selected = m_selectedMeter;
            seq::ChangeList changes{};
            changes.push_back(seq::MakeMeterChange(std::move(before), m_doc.Meter));
            Commit(std::wstring{ res::GetString(L"UndoMoveMeter") }, std::move(changes));
            m_selectedMeter = selected;
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish a drag on the Meter row.")
    }

    // ---------------------------------------------------------------- adding, changing and removing

    _Use_decl_annotations_
    void MainWindow::OnMeterDoubleTapped(foundation::Point point, int64_t tick)
    {
        try
        {
            // On a flag: change that meter. Anywhere else: a new meter from the nearest bar.
            if (auto const hit = HitTestMeter(point); hit != SIZE_MAX)
            {
                m_selectedMeter = hit;
                PromptForMeter(seq::BarPositionAtTick(m_doc.Meter, m_doc.Meter[hit].Tick).Bar);
                return;
            }

            PromptForMeter(std::max<int64_t>(1, NearestBar(m_doc.Meter, tick)));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a double click on the Meter row.")
    }

    void MainWindow::DeleteSelectedMeter() noexcept
    {
        try
        {
            auto const index = std::exchange(m_selectedMeter, SIZE_MAX);
            auto before = m_doc.Meter;

            // The first meter can't go.
            if (index == 0 || index >= m_doc.Meter.size() || !seq::RemoveMeterChange(m_doc.Meter, index))
            {
                InvalidateArrange();
                return;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeMeterChange(std::move(before), m_doc.Meter));
            Commit(std::wstring{ res::GetString(L"UndoChangeMeter") }, std::move(changes));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to remove a meter change.")
    }

    // ---------------------------------------------------------------- the menu

    _Use_decl_annotations_
    void MainWindow::AddMeterMenuItems(controls::MenuFlyout const& menu, foundation::Point point, int64_t tick)
    {
        auto weak = get_weak();
        auto const hit = HitTestMeter(point);
        auto const bar = hit != SIZE_MAX ? seq::BarPositionAtTick(m_doc.Meter, m_doc.Meter[hit].Tick).Bar : std::max<int64_t>(1, NearestBar(m_doc.Meter, tick));

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

        menu.Items().Append(controls::MenuFlyoutSeparator{});
        m_selectedMeter = hit;

        add(res::FormatString(L"MenuChangeMeterFormat", bar), L"\uE70F", [weak, bar]() { if (auto s = weak.get()) { s->PromptForMeter(bar); } });

        if (hit != SIZE_MAX && hit > 0)
        {
            add(res::FormatString(L"MenuRemoveMeterFormat", bar), L"\uE74D", [weak, hit]()
            {
                if (auto s = weak.get())
                {
                    s->m_selectedMeter = hit;
                    s->DeleteSelectedMeter();
                }
            });
        }

        add(res::GetString(L"MenuHideMeterRow"), L"\uED1A", [weak]() { if (auto s = weak.get()) { s->ShowMeterRow(false); } });
        InvalidateArrange();
    }
}