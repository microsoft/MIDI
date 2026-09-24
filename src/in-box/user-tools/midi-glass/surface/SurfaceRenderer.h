// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LayoutModel.h"
#include "ThemeModel.h"
#include "SurfaceColors.h"
#include "GlassControl.h"

namespace glass
{
    namespace comp = ::winrt::Microsoft::UI::Composition;

    // The app has a plain C++ namespace called midiglass as well as the projected one, so the
    // control type is always written out in full.
    namespace projected = ::winrt::midiglass;

    using GlassControlElement = ::winrt::midiglass::GlassControl;

    // Everything drawn for one control. The XAML element beside it owns identity, hit testing,
    // focus and automation; nothing in here is a XAML element, because changing a value must not
    // invalidate a layout pass.
    struct SurfaceVisual
    {
        comp::ContainerVisual Root{ nullptr };
        comp::SpriteVisual Bloom{ nullptr };
        comp::ShapeVisual Shape{ nullptr };

        // Fader, pad and meter: a rectangle that grows.
        comp::CompositionRoundedRectangleGeometry PipeGeometry{ nullptr };

        // Knob and encoder: an arc that sweeps.
        comp::CompositionEllipseGeometry ArcGeometry{ nullptr };

        comp::CompositionSpriteShape PlateShape{ nullptr };
        comp::CompositionSpriteShape PipeShape{ nullptr };

        ControlKind Kind{ ControlKind::Knob };

        float TrackOrigin{ 0.0f };
        float TrackLength{ 0.0f };
        float PipeThickness{ 0.0f };
        float PipeCrossOffset{ 0.0f };

        float Width{ 0.0f };
        float Height{ 0.0f };
    };

    // Draws one page of a layout, the way phase 0 decided: a light XAML element per control for
    // identity and hit testing, with the content drawn as composition visuals inside it.
    //
    // Measured in the spike: the XAML element costs nothing per frame, because setting a value
    // touches only the composition visual. What it buys is a surface a screen reader can find,
    // pointer routing and per-pointer capture, which is what gives multi-touch for free.
    class SurfaceRenderer
    {
    public:
        // Builds one page. The host is in page coordinates; scaling is the window's business.
        void Build(
            _In_ controls::Canvas const& host,
            _In_ LayoutDocument const& document,
            _In_ Theme const& theme,
            _In_ size_t pageIndex);

        void Teardown();

        size_t ItemCount() const noexcept { return m_visuals.size(); }

        // Page item to the index the binding engine uses, which counts every page in order.
        uint32_t ControlIndexOf(_In_ size_t itemIndex) const noexcept;

        // The other direction, for feedback arriving from a device. Returns false when the
        // control is not on the page being shown.
        bool TryFindItem(_In_ uint32_t controlIndex, _Out_ size_t& itemIndex) const noexcept;

        GlassControlElement ElementAt(_In_ size_t itemIndex) const noexcept;

        ControlKind KindAt(_In_ size_t itemIndex) const noexcept;

        // Moves the drawing. Does not send anything and does not touch the XAML element's value,
        // which the caller owns.
        void SetValue(_In_ size_t itemIndex, _In_ double value) noexcept;

        // Activity, and the only thing that blooms. Decayed by the compositor rather than by a
        // timer on the UI thread, so a wall of blinking controls costs the app nothing.
        void Bloom(_In_ size_t itemIndex) noexcept;
        void ClearBloom(_In_ size_t itemIndex) noexcept;

        // Windows says reduce motion, so the bloom switches instead of fading. The surface is a
        // wall of animation by design, which is exactly why honoring this is not optional.
        void SetReducedMotion(_In_ bool reduced) noexcept { m_reducedMotion = reduced; }

        ThemeColor DeckColor() const noexcept { return m_deck; }

    private:
        void BuildControl(
            _In_ comp::Compositor const& compositor,
            _In_ Control const& control,
            _In_ Theme const& theme,
            _In_ uint32_t controlIndex);

        comp::CompositionColorBrush BrushFor(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& color);

        controls::Canvas m_host{ nullptr };

        std::vector<SurfaceVisual> m_visuals{};
        std::vector<GlassControlElement> m_elements{};
        std::vector<uint32_t> m_controlIndexes{};
        std::vector<ControlKind> m_kinds{};

        // One brush per distinct color for the whole page. A control never owns a brush, which is
        // what keeps a theme swap a handful of objects rather than a walk of two hundred.
        std::unordered_map<uint32_t, comp::CompositionColorBrush> m_brushes{};

        ThemeColor m_deck{};
        bool m_reducedMotion{ false };
    };
}
