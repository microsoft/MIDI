// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "SurfaceRenderer.h"

namespace gspike
{
    // Approach A. One templated XAML control per surface control, placed on a Canvas.
    class XamlElementRenderer final : public ISurfaceRenderer
    {
    public:
        std::wstring_view Name() const noexcept override { return L"xaml"; }

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

        // Direct pointers to the implementation types. A value change on the hot path must not
        // pay for a projection call, and a real implementation would do exactly this.
        std::vector<winrt::glassspike::implementation::GlassControl*> m_controls;
        std::vector<winrt::glassspike::GlassControl> m_keepAlive;
        std::vector<uint8_t> m_slots;

        std::array<winrt::xaml::Media::SolidColorBrush, HueSlotCount> m_hueBrushes{
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        std::array<winrt::xaml::Media::SolidColorBrush, HueSlotCount> m_bloomBrushes{
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        winrt::xaml::Media::SolidColorBrush m_plateBrush{ nullptr };
        winrt::xaml::Media::SolidColorBrush m_trackBrush{ nullptr };
    };
}
