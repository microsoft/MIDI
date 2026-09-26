// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "XamlElementRenderer.h"
#include "ShapeBuilder.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace gspike
{
    namespace
    {
        uint32_t CountVisualTree(DependencyObject const& root)
        {
            uint32_t total = 1;
            const int32_t children = VisualTreeHelper::GetChildrenCount(root);

            for (int32_t i = 0; i < children; i++)
            {
                total += CountVisualTree(VisualTreeHelper::GetChild(root, i));
            }

            return total;
        }

        hstring StyleKeyFor(ControlKind kind)
        {
            switch (kind)
            {
            case ControlKind::Knob: return L"GlassKnobStyle";
            case ControlKind::Pad: return L"GlassPadStyle";
            default: return L"GlassFaderStyle";
            }
        }
    }

    void XamlElementRenderer::Build(Panel const& host, PageModel const& page)
    {
        m_host = host;

        // Six hue brushes plus a plate and a track, shared by every control. Sharing the brush
        // object is what makes a theme swap a recolor rather than a walk of two hundred elements.
        for (uint8_t slot = 0; slot < HueSlotCount; slot++)
        {
            m_hueBrushes[slot] = SolidColorBrush(HueColor(slot));

            auto bloom = HueColor(slot);
            bloom.A = 0x5A;
            m_bloomBrushes[slot] = SolidColorBrush(bloom);
        }

        m_plateBrush = SolidColorBrush(PlateColor());
        m_trackBrush = SolidColorBrush(TrackColor());

        auto resources = host.Resources();
        auto children = host.Children();

        m_controls.reserve(page.Controls.size());
        m_keepAlive.reserve(page.Controls.size());
        m_slots.reserve(page.Controls.size());

        uint32_t index = 0;

        for (auto const& d : page.Controls)
        {
            glassspike::GlassControl control{};

            control.Kind(static_cast<int32_t>(d.Kind));
            control.HueSlot(d.HueSlot);
            control.Width(d.Width);
            control.Height(d.Height);
            control.SurfaceName(hstring{ std::format(L"Control {}", index + 1) });
            control.IsTabStop(true);
            control.UseLayoutRounding(true);

            auto style = resources.Lookup(box_value(StyleKeyFor(d.Kind))).as<xaml::Style>();
            control.Style(style);

            Canvas::SetLeft(control, d.X);
            Canvas::SetTop(control, d.Y);

            children.Append(control);

            auto* impl = get_self<glassspike::implementation::GlassControl>(control);

            // The template is applied during the first measure pass, so brushes and the initial
            // value are pushed after the layout pass the caller runs.
            m_controls.push_back(impl);
            m_keepAlive.push_back(control);
            m_slots.push_back(static_cast<uint8_t>(d.HueSlot % HueSlotCount));

            index++;
        }

        host.UpdateLayout();

        for (size_t i = 0; i < m_controls.size(); i++)
        {
            const uint8_t slot = page.Controls[i].HueSlot;

            m_controls[i]->ApplyBrushes(
                m_plateBrush,
                m_hueBrushes[slot % HueSlotCount],
                m_trackBrush,
                m_hueBrushes[slot % HueSlotCount],
                m_bloomBrushes[slot % HueSlotCount]);

            m_controls[i]->SetValueDirect(page.Controls[i].Value, 0.0f);
        }
    }

    void XamlElementRenderer::Teardown()
    {
        if (m_host)
        {
            m_host.Children().Clear();
        }

        m_controls.clear();
        m_keepAlive.clear();
        m_slots.clear();
        m_host = nullptr;
    }

    void XamlElementRenderer::SetValue(uint32_t index, float value, float bloom) noexcept
    {
        if (index >= m_controls.size())
        {
            return;
        }

        m_controls[index]->SetValueDirect(value, bloom);
    }

    int32_t XamlElementRenderer::HitTest(float x, float y) const noexcept
    {
        // XAML would normally do this for us through routed pointer events. The index lookup is
        // here only so the three renderers can be driven through one interface.
        for (size_t i = 0; i < m_keepAlive.size(); i++)
        {
            auto const& control = m_keepAlive[i];

            const float left = static_cast<float>(Canvas::GetLeft(control));
            const float top = static_cast<float>(Canvas::GetTop(control));

            if (x >= left && x < left + static_cast<float>(control.Width()) &&
                y >= top && y < top + static_cast<float>(control.Height()))
            {
                return static_cast<int32_t>(i);
            }
        }

        return -1;
    }

    uint32_t XamlElementRenderer::XamlElementCount() const
    {
        return m_host ? CountVisualTree(m_host) : 0u;
    }

    void XamlElementRenderer::ApplyTheme(bool alternate)
    {
        if (!m_plateBrush)
        {
            return;
        }

        for (uint8_t slot = 0; slot < HueSlotCount; slot++)
        {
            const auto color = alternate ? AlternateHueColor(slot) : HueColor(slot);

            m_hueBrushes[slot].Color(color);

            auto bloom = color;
            bloom.A = 0x5A;
            m_bloomBrushes[slot].Color(bloom);
        }

        m_plateBrush.Color(alternate ? AlternatePlateColor() : PlateColor());
    }

    bool XamlElementRenderer::VerifyTheme(bool alternate) const
    {
        if (m_controls.empty())
        {
            return false;
        }

        // Every control, not a sample. A theme that reaches 199 of 200 is exactly the defect
        // worth catching, and at this size the walk is affordable.
        for (size_t i = 0; i < m_controls.size(); i++)
        {
            const auto actual = m_controls[i]->RimColorInUse();
            const auto expected = alternate ? AlternateHueColor(m_slots[i]) : HueColor(m_slots[i]);

            if (actual.R != expected.R || actual.G != expected.G || actual.B != expected.B)
            {
                return false;
            }
        }

        return true;
    }

    std::unique_ptr<ISurfaceRenderer> MakeXamlElementRenderer()
    {
        return std::make_unique<XamlElementRenderer>();
    }
}
