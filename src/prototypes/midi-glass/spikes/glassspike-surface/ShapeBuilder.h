// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "SurfaceModel.h"

namespace gspike
{
    // The composition half of the spike, shared by the pure composition renderer and the hybrid
    // one so that the two are measuring the same paint, not two different paints.
    struct CompositionControl
    {
        winrt::comp::ContainerVisual Root{ nullptr };
        winrt::comp::SpriteVisual Bloom{ nullptr };
        winrt::comp::ShapeVisual Shape{ nullptr };

        // fader and pad
        winrt::comp::CompositionRoundedRectangleGeometry PipeGeometry{ nullptr };
        // knob
        winrt::comp::CompositionEllipseGeometry ArcGeometry{ nullptr };
        // plate, so a theme swap has something to change
        winrt::comp::CompositionRoundedRectangleGeometry PlateGeometry{ nullptr };
        winrt::comp::CompositionSpriteShape PlateShape{ nullptr };
        winrt::comp::CompositionSpriteShape PipeShape{ nullptr };

        ControlKind Kind{ ControlKind::Fader };
        float TrackOrigin{};
        float TrackLength{};
        float PipeThickness{};
        float PipeCrossOffset{};
    };

    // One set of brushes for the whole page. A control never owns a brush, which is what makes a
    // theme swap a six-color operation rather than a walk of two hundred objects.
    class CompositionPalette
    {
    public:
        void Create(winrt::comp::Compositor const& compositor, bool alternate);
        void Recolor(bool alternate);

        winrt::comp::CompositionColorBrush Hue(uint8_t slot) const { return m_hue[slot % HueSlotCount]; }
        winrt::comp::CompositionColorBrush Bloom(uint8_t slot) const { return m_bloom[slot % HueSlotCount]; }
        winrt::comp::CompositionColorBrush Plate() const { return m_plate; }
        winrt::comp::CompositionColorBrush Track() const { return m_track; }

        bool IsCreated() const noexcept { return m_plate != nullptr; }

    private:
        std::array<winrt::comp::CompositionColorBrush, HueSlotCount> m_hue{
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        std::array<winrt::comp::CompositionColorBrush, HueSlotCount> m_bloom{
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
        winrt::comp::CompositionColorBrush m_plate{ nullptr };
        winrt::comp::CompositionColorBrush m_track{ nullptr };
    };

    CompositionControl BuildCompositionControl(
        winrt::comp::Compositor const& compositor,
        CompositionPalette const& palette,
        ControlDescriptor const& descriptor);

    void SetCompositionValue(CompositionControl const& control, float value, float bloom) noexcept;

    // Reads the color each control's rim is actually stroked with, so a theme swap is proven to
    // have landed on all of them.
    bool VerifyCompositionTheme(std::vector<CompositionControl> const& controls, bool alternate);

    // Alternate hue table, used only to prove a theme swap reaches everything.
    winrt::Windows::UI::Color AlternateHueColor(uint8_t slot) noexcept;
    winrt::Windows::UI::Color AlternatePlateColor() noexcept;
}
