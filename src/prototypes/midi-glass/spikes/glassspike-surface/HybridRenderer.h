// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "SurfaceRenderer.h"
#include "ShapeBuilder.h"

namespace gspike
{
    // Approach C. A XAML element per control for identity, hit testing, focus and automation,
    // with the content drawn by the same composition code the pure composition renderer uses.
    class HybridRenderer final : public ISurfaceRenderer
    {
    public:
        std::wstring_view Name() const noexcept override { return L"hybrid"; }

        void Build(winrt::xaml::Controls::Panel const& host, PageModel const& page) override;
        void Teardown() override;
        void SetValue(uint32_t index, float value, float bloom) noexcept override;
        int32_t HitTest(float x, float y) const noexcept override;
        uint32_t XamlElementCount() const override;
        bool HasAutomationPeers() const noexcept override { return true; }
        void ApplyTheme(bool alternate) override;
        bool VerifyTheme(bool alternate) const override;

    private:
        winrt::xaml::Controls::Panel m_host{ nullptr };
        CompositionPalette m_palette{};

        std::vector<CompositionControl> m_visuals;
        std::vector<winrt::glassspike::implementation::HybridControl*> m_controls;
        std::vector<winrt::glassspike::HybridControl> m_keepAlive;

        struct Bounds { float X, Y, Right, Bottom; };
        std::vector<Bounds> m_bounds;
    };
}
