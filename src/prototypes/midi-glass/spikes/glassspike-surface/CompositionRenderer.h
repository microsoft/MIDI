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
    // Approach B. One XAML element hosts the whole page; every control below it is composition
    // only. Nothing XAML knows about exists per control, which is the upside and the problem.
    class CompositionRenderer final : public ISurfaceRenderer
    {
    public:
        std::wstring_view Name() const noexcept override { return L"composition"; }

        void Build(winrt::xaml::Controls::Panel const& host, PageModel const& page) override;
        void Teardown() override;
        void SetValue(uint32_t index, float value, float bloom) noexcept override;
        int32_t HitTest(float x, float y) const noexcept override;
        uint32_t XamlElementCount() const override;
        bool HasAutomationPeers() const noexcept override { return false; }
        void ApplyTheme(bool alternate) override;
        bool VerifyTheme(bool alternate) const override;

    private:
        winrt::xaml::Controls::Panel m_host{ nullptr };
        winrt::comp::ContainerVisual m_root{ nullptr };
        CompositionPalette m_palette{};
        std::vector<CompositionControl> m_controls;

        // The page geometry is kept in plain memory so a hit test never crosses the ABI.
        struct Bounds { float X, Y, Right, Bottom; };
        std::vector<Bounds> m_bounds;
    };
}
