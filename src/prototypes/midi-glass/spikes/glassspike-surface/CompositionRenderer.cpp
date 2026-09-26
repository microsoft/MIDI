// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "CompositionRenderer.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace gspike
{
    void CompositionRenderer::Build(Controls::Panel const& host, PageModel const& page)
    {
        m_host = host;

        auto hostVisual = ElementCompositionPreview::GetElementVisual(host);
        auto compositor = hostVisual.Compositor();

        m_palette.Create(compositor, false);

        m_root = compositor.CreateContainerVisual();
        m_root.Size(Windows::Foundation::Numerics::float2{ page.Width, page.Height });

        m_controls.reserve(page.Controls.size());
        m_bounds.reserve(page.Controls.size());

        auto children = m_root.Children();

        for (auto const& descriptor : page.Controls)
        {
            auto control = BuildCompositionControl(compositor, m_palette, descriptor);
            children.InsertAtTop(control.Root);

            m_bounds.push_back(Bounds{
                descriptor.X,
                descriptor.Y,
                descriptor.X + descriptor.Width,
                descriptor.Y + descriptor.Height });

            m_controls.push_back(std::move(control));
        }

        ElementCompositionPreview::SetElementChildVisual(host, m_root);
    }

    void CompositionRenderer::Teardown()
    {
        if (m_host)
        {
            ElementCompositionPreview::SetElementChildVisual(m_host, nullptr);
        }

        m_controls.clear();
        m_bounds.clear();
        m_root = nullptr;
        m_host = nullptr;
    }

    void CompositionRenderer::SetValue(uint32_t index, float value, float bloom) noexcept
    {
        if (index >= m_controls.size())
        {
            return;
        }

        SetCompositionValue(m_controls[index], value, bloom);
    }

    int32_t CompositionRenderer::HitTest(float x, float y) const noexcept
    {
        // Linear for the spike. A real implementation would bucket by row; the point here is that
        // hit testing is work this approach has to do for itself, not that a scan is the answer.
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

    uint32_t CompositionRenderer::XamlElementCount() const
    {
        // The host panel and nothing else.
        return m_host ? 1u : 0u;
    }

    void CompositionRenderer::ApplyTheme(bool alternate)
    {
        // Six brushes. Nothing walks the controls.
        if (m_palette.IsCreated())
        {
            m_palette.Recolor(alternate);
        }
    }

    bool CompositionRenderer::VerifyTheme(bool alternate) const
    {
        return VerifyCompositionTheme(m_controls, alternate);
    }

    std::unique_ptr<ISurfaceRenderer> MakeCompositionRenderer()
    {
        return std::make_unique<CompositionRenderer>();
    }
}
