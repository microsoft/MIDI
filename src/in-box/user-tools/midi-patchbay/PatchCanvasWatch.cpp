// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The numbered dots that show which connections a monitor window watches.

#include "pch.h"
#include "PatchCanvas.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        constexpr double WatchDotSize = 18.0;

        // Side by side when more than one window watches the same connection.
        constexpr double WatchDotSpacing = 22.0;
    }

    _Use_decl_annotations_
    void PatchCanvas::SetWatchMarks(std::vector<WatchMark> marks) noexcept
    {
        try
        {
            if (!m_initialized || m_shuttingDown || m_overlayLayer == nullptr)
            {
                return;
            }

            for (auto const& dot : m_watchDots)
            {
                uint32_t index{ 0 };

                if (dot.Root != nullptr && m_overlayLayer.Children().IndexOf(dot.Root, index))
                {
                    m_overlayLayer.Children().RemoveAt(index);
                }
            }

            m_watchDots.clear();

            for (auto& mark : marks)
            {
                WatchDotVisual dot{};
                dot.Mark = std::move(mark);

                auto const number = dot.Mark.Number;
                auto const color = dot.Mark.Color;

                controls::Grid root{};
                root.Width(WatchDotSize);
                root.Height(WatchDotSize);

                shapes::Ellipse circle{};
                circle.Fill(media::SolidColorBrush{ color });
                circle.Stroke(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0x20, 0x20, 0x20 } });
                circle.StrokeThickness(1.5);
                root.Children().Append(circle);

                controls::TextBlock text{};
                text.Text(winrt::to_hstring(number));
                text.FontSize(11);
                text.FontWeight(winrt::Microsoft::UI::Text::FontWeights::Bold());
                text.Foreground(media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0, 0, 0 } });
                text.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                text.VerticalAlignment(xaml::VerticalAlignment::Center);
                text.IsHitTestVisible(false);
                root.Children().Append(text);

                auto const tip = resources::FormatString(L"WatchDotTipFormat", number);

                controls::ToolTipService::SetToolTip(root, winrt::box_value(tip));
                xaml::Automation::AutomationProperties::SetName(root, tip);

                root.PointerPressed([this, number](auto&&, input::PointerRoutedEventArgs const& args)
                    {
                        args.Handled(true);

                        if (m_callbacks.WatchMarkClicked)
                        {
                            m_callbacks.WatchMarkClicked(number);
                        }
                    });

                root.Visibility(xaml::Visibility::Collapsed);

                m_overlayLayer.Children().Append(root);

                dot.Root = root;
                m_watchDots.push_back(std::move(dot));
            }

            RedrawWatchMarks();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show which connections are watched.")
    }

    void PatchCanvas::RedrawWatchMarks() noexcept
    {
        try
        {
            std::unordered_map<std::wstring, uint32_t> placed{};

            for (auto& dot : m_watchDots)
            {
                if (dot.Root == nullptr)
                {
                    continue;
                }

                auto const visual = std::find_if(m_connections.begin(), m_connections.end(),
                    [&dot](ConnectionVisual const& candidate) { return candidate.ConnectionId == dot.Mark.ConnectionId; });

                if (visual == m_connections.end() || visual->Samples.empty() || visual->Line == nullptr ||
                    visual->Line.Visibility() != xaml::Visibility::Visible)
                {
                    dot.Root.Visibility(xaml::Visibility::Collapsed);
                    continue;
                }

                // The middle of the line, unless its group label is there already.
                auto const pillShown = visual->Pill != nullptr && visual->Pill.Visibility() == xaml::Visibility::Visible;
                auto const& samples = visual->Samples;
                auto const point = samples[pillShown ? samples.size() / 4 : samples.size() / 2];

                auto const order = placed[dot.Mark.ConnectionId]++;

                controls::Canvas::SetLeft(dot.Root, point.X - WatchDotSize / 2 + order * WatchDotSpacing);
                controls::Canvas::SetTop(dot.Root, point.Y - WatchDotSize / 2);

                dot.Root.Visibility(xaml::Visibility::Visible);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to place the watched connections' dots.")
    }
}
