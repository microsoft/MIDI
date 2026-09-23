// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "GlassControl.h"
#include "GlassControl.g.cpp"
#include "SurfaceModel.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace Shapes = winrt::Microsoft::UI::Xaml::Shapes;

namespace winrt::glassspike::implementation
{
    void GlassControl::OnApplyTemplate()
    {
        m_plate = GetTemplateChild(L"PART_Plate").try_as<Border>();
        m_bloomLayer = GetTemplateChild(L"PART_Bloom").try_as<Border>();
        m_track = GetTemplateChild(L"PART_Track").try_as<Shapes::Rectangle>();
        m_pipe = GetTemplateChild(L"PART_Pipe").try_as<Shapes::Rectangle>();
        m_ring = GetTemplateChild(L"PART_Ring").try_as<Shapes::Ellipse>();
        m_arc = GetTemplateChild(L"PART_Arc").try_as<Shapes::Path>();

        const float width = static_cast<float>(Width());
        const float height = static_cast<float>(Height());

        if (m_arc)
        {
            // Built once. From here only the segment's end point and its large-arc flag change,
            // which is the cheapest way XAML shapes can express a swept arc.
            m_arcRadius = std::min(width, height) * 0.5f - gspike::PipeInset;
            m_arcCenterX = width * 0.5f;
            m_arcCenterY = height * 0.5f;

            const double startRadians = gspike::KnobStartDegrees * 3.14159265358979 / 180.0;

            Windows::Foundation::Point start{
                m_arcCenterX + static_cast<float>(std::cos(startRadians)) * m_arcRadius,
                m_arcCenterY + static_cast<float>(std::sin(startRadians)) * m_arcRadius };

            m_arcSegment = ArcSegment();
            m_arcSegment.Size(Windows::Foundation::Size{ m_arcRadius, m_arcRadius });
            m_arcSegment.SweepDirection(SweepDirection::Clockwise);
            m_arcSegment.Point(start);

            PathFigure figure{};
            figure.StartPoint(start);
            figure.IsClosed(false);
            figure.IsFilled(false);
            figure.Segments().Append(m_arcSegment);

            PathGeometry geometry{};
            geometry.Figures().Append(figure);

            m_arc.Data(geometry);
            m_arc.Width(width);
            m_arc.Height(height);
        }

        if (m_pipe)
        {
            m_trackLength = (m_kind == 0)
                ? height - gspike::PipeInset * 2.0f
                : width - gspike::PipeInset * 2.0f;
        }

        m_templateApplied = true;

        UpdatePipe(static_cast<float>(m_value));

        if (m_bloomLayer)
        {
            m_bloomLayer.Opacity(m_bloom);
        }
    }

    void GlassControl::ApplyBrushes(
        Brush const& plate,
        Brush const& rim,
        Brush const& track,
        Brush const& pipe,
        Brush const& bloom)
    {
        if (m_plate)
        {
            m_plate.Background(plate);
            m_plate.BorderBrush(rim);
        }

        if (m_track)
        {
            m_track.Fill(track);
        }

        if (m_ring)
        {
            m_ring.Stroke(track);
        }

        if (m_pipe)
        {
            m_pipe.Fill(pipe);
        }

        if (m_arc)
        {
            m_arc.Stroke(pipe);
        }

        if (m_bloomLayer)
        {
            m_bloomLayer.Background(bloom);
        }
    }

    void GlassControl::SurfaceValue(double value)
    {
        m_value = value;
        UpdatePipe(static_cast<float>(value));
    }
    void GlassControl::Bloom(double value)
    {
        m_bloom = value;

        if (m_bloomLayer)
        {
            m_bloomLayer.Opacity(value);
        }
    }

    void GlassControl::SetValueDirect(float value, float bloom) noexcept
    {
        m_value = value;
        m_bloom = bloom;

        UpdatePipe(value);

        if (m_bloomLayer)
        {
            m_bloomLayer.Opacity(bloom);
        }
    }

    void GlassControl::UpdatePipe(float value) noexcept
    {
        if (!m_templateApplied)
        {
            return;
        }

        const float clamped = std::clamp(value, 0.0f, 1.0f);

        if (m_arcSegment)
        {
            const double sweep = gspike::KnobSweepDegrees * clamped;
            const double endRadians = (gspike::KnobStartDegrees + sweep) * 3.14159265358979 / 180.0;

            m_arcSegment.Point(Windows::Foundation::Point{
                m_arcCenterX + static_cast<float>(std::cos(endRadians)) * m_arcRadius,
                m_arcCenterY + static_cast<float>(std::sin(endRadians)) * m_arcRadius });

            m_arcSegment.IsLargeArc(sweep > 180.0);
        }
        else if (m_pipe)
        {
            if (m_kind == 0)
            {
                m_pipe.Height(m_trackLength * clamped);
            }
            else
            {
                m_pipe.Width(m_trackLength * clamped);
            }
        }
    }

    Windows::UI::Color GlassControl::RimColorInUse() const
    {
        if (m_plate)
        {
            if (auto brush = m_plate.BorderBrush().try_as<SolidColorBrush>())
            {
                return brush.Color();
            }
        }

        return Windows::UI::Color{};
    }

    Automation::Peers::AutomationPeer GlassControl::OnCreateAutomationPeer()
    {
        auto self = get_strong();

        return glassspike::SurfaceAutomationPeer(
            self.as<FrameworkElement>(),
            self.as<glassspike::ISurfaceValueHost>());
    }
}
