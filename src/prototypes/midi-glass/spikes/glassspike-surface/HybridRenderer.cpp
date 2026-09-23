// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "HybridRenderer.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Hosting;
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
    }

    void HybridRenderer::Build(Panel const& host, PageModel const& page)
    {
        m_host = host;

        auto compositor = ElementCompositionPreview::GetElementVisual(host).Compositor();
        m_palette.Create(compositor, false);

        auto children = host.Children();

        m_visuals.reserve(page.Controls.size());
        m_controls.reserve(page.Controls.size());
        m_keepAlive.reserve(page.Controls.size());
        m_bounds.reserve(page.Controls.size());

        uint32_t index = 0;

        for (auto const& d : page.Controls)
        {
            glassspike::HybridControl control{};

            control.Width(d.Width);
            control.Height(d.Height);
            control.SurfaceName(hstring{ std::format(L"Control {}", index + 1) });
            control.IsTabStop(true);

            // Nothing is drawn by XAML, so the element must still be hit testable. A transparent
            // background is the usual way and it is why the element is a Control rather than a
            // bare FrameworkElement.
            control.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));

            Canvas::SetLeft(control, d.X);
            Canvas::SetTop(control, d.Y);

            children.Append(control);

            auto visual = BuildCompositionControl(compositor, m_palette, d);

            // Built in page coordinates; the XAML element already carries the position.
            visual.Root.Offset(Windows::Foundation::Numerics::float3{ 0.0f, 0.0f, 0.0f });

            ElementCompositionPreview::SetElementChildVisual(control, visual.Root);

            auto* impl = get_self<glassspike::implementation::HybridControl>(control);
            impl->SetValueDirect(d.Value);

            m_visuals.push_back(std::move(visual));
            m_controls.push_back(impl);
            m_keepAlive.push_back(control);
            m_bounds.push_back(Bounds{ d.X, d.Y, d.X + d.Width, d.Y + d.Height });

            index++;
        }

        host.UpdateLayout();
    }

    void HybridRenderer::Teardown()
    {
        for (auto const& control : m_keepAlive)
        {
            ElementCompositionPreview::SetElementChildVisual(control, nullptr);
        }

        if (m_host)
        {
            m_host.Children().Clear();
        }

        m_visuals.clear();
        m_controls.clear();
        m_keepAlive.clear();
        m_bounds.clear();
        m_host = nullptr;
    }

    void HybridRenderer::SetValue(uint32_t index, float value, float bloom) noexcept
    {
        if (index >= m_visuals.size())
        {
            return;
        }

        SetCompositionValue(m_visuals[index], value, bloom);
        m_controls[index]->SetValueDirect(value);
    }

    int32_t HybridRenderer::HitTest(float x, float y) const noexcept
    {
        for (size_t i = 0; i < m_bounds.size(); i++)
        {
            auto const& b = m_bounds[i];
            if (x >= b.X && x < b.Right && y >= b.Y && y < b.Bottom)
            {
                return static_cast<int32_t>(i);
            }
        }

        return -1;
    }

    uint32_t HybridRenderer::XamlElementCount() const
    {
        return m_host ? CountVisualTree(m_host) : 0u;
    }

    void HybridRenderer::ApplyTheme(bool alternate)
    {
        if (m_palette.IsCreated())
        {
            m_palette.Recolor(alternate);
        }
    }

    bool HybridRenderer::VerifyTheme(bool alternate) const
    {
        return VerifyCompositionTheme(m_visuals, alternate);
    }

    std::unique_ptr<ISurfaceRenderer> MakeHybridRenderer()
    {
        return std::make_unique<HybridRenderer>();
    }
}
