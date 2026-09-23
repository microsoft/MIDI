// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "GlassControl.g.h"

namespace winrt::glassspike::implementation
{
    // Approach A. A real templated control: the template comes from markup, every part of it is a
    // XAML element, and the framework supplies layout, hit testing, focus and theming.
    struct GlassControl : GlassControlT<GlassControl>
    {
        GlassControl() = default;

        int32_t Kind() const noexcept { return m_kind; }
        void Kind(int32_t value) noexcept { m_kind = value; }

        int32_t HueSlot() const noexcept { return m_hueSlot; }
        void HueSlot(int32_t value) noexcept { m_hueSlot = value; }

        double Bloom() const noexcept { return m_bloom; }
        void Bloom(double value);

        double SurfaceValue() const noexcept { return m_value; }
        void SurfaceValue(double value);

        hstring SurfaceName() const { return m_name; }
        void SurfaceName(hstring const& value) { m_name = value; }

        void OnApplyTemplate();
        Microsoft::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        // Called from the renderer through get_self, so the hot path never crosses the ABI.
        void SetValueDirect(float value, float bloom) noexcept;

        void ApplyBrushes(
            Microsoft::UI::Xaml::Media::Brush const& plate,
            Microsoft::UI::Xaml::Media::Brush const& rim,
            Microsoft::UI::Xaml::Media::Brush const& track,
            Microsoft::UI::Xaml::Media::Brush const& pipe,
            Microsoft::UI::Xaml::Media::Brush const& bloom);

        // Read back what the element is actually painting with, not what it was told to paint with.
        Windows::UI::Color RimColorInUse() const;

    private:
        void UpdatePipe(float value) noexcept;

        int32_t m_kind{ 0 };
        int32_t m_hueSlot{ 0 };
        double m_bloom{ 0.0 };
        double m_value{ 0.0 };
        hstring m_name;

        Microsoft::UI::Xaml::Controls::Border m_plate{ nullptr };
        Microsoft::UI::Xaml::Controls::Border m_bloomLayer{ nullptr };
        Microsoft::UI::Xaml::Shapes::Rectangle m_track{ nullptr };
        Microsoft::UI::Xaml::Shapes::Rectangle m_pipe{ nullptr };
        Microsoft::UI::Xaml::Shapes::Ellipse m_ring{ nullptr };
        Microsoft::UI::Xaml::Shapes::Path m_arc{ nullptr };
        Microsoft::UI::Xaml::Media::ArcSegment m_arcSegment{ nullptr };

        float m_trackLength{ 0.0f };
        float m_arcRadius{ 0.0f };
        float m_arcCenterX{ 0.0f };
        float m_arcCenterY{ 0.0f };
        bool m_templateApplied{ false };
    };
}

namespace winrt::glassspike::factory_implementation
{
    struct GlassControl : GlassControlT<GlassControl, implementation::GlassControl>
    {
    };
}
