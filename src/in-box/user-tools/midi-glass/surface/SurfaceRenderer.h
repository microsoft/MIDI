// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LayoutModel.h"
#include "ThemeModel.h"
#include "ThemeStore.h"
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
    //
    // Bottom to top: the elevation shadow, the glow, the plate and its track, the glow under the
    // value, then the value itself. Five visuals, because a shadow has to be cast by a visual
    // and a glow has to sit between two things a single ShapeVisual would draw in one pass.
    struct SurfaceVisual
    {
        comp::ContainerVisual Root{ nullptr };

        // Casts the plate's drop shadow. Paints nothing itself: the shadow is shaped by a mask.
        comp::SpriteVisual Elevation{ nullptr };

        // The soft light around a lit or active control. This is a real blurred shadow in the
        // control's own hue, not a rectangle of color.
        comp::SpriteVisual Bloom{ nullptr };
        comp::DropShadow BloomShadow{ nullptr };

        comp::ShapeVisual Shape{ nullptr };

        // Struck through when the device this control sends to is not here. Built once and
        // hidden, because a device coming and going must not rebuild a page.
        comp::ShapeVisual Unavailable{ nullptr };

        comp::ShapeVisual ValueShape{ nullptr };

        // Fader, pad and meter: a rectangle that grows.
        comp::CompositionRoundedRectangleGeometry PipeGeometry{ nullptr };

        // Knob and encoder: an arc that sweeps.
        comp::CompositionEllipseGeometry ArcGeometry{ nullptr };

        // The cap on a fader, and the hairline of hue through it.
        comp::CompositionRoundedRectangleGeometry ThumbGeometry{ nullptr };
        comp::CompositionRoundedRectangleGeometry ThumbLineGeometry{ nullptr };

        comp::CompositionSpriteShape PlateShape{ nullptr };
        comp::CompositionSpriteShape PipeShape{ nullptr };

        // The line on a knob that says which way it is pointing. Rotated rather than rebuilt.
        comp::CompositionSpriteShape PointerShape{ nullptr };

        // A switch changes what its plate and rim are painted with rather than being rebuilt,
        // so turning one on costs two property sets.
        comp::CompositionBrush PlateOffBrush{ nullptr };
        comp::CompositionBrush PlateOnBrush{ nullptr };
        comp::CompositionBrush RimOffBrush{ nullptr };
        comp::CompositionBrush RimOnBrush{ nullptr };

        bool IsSwitch{ false };

        ControlKind Kind{ ControlKind::Knob };

        float TrackOrigin{ 0.0f };
        float TrackLength{ 0.0f };
        float PipeThickness{ 0.0f };
        float PipeCrossOffset{ 0.0f };

        float Width{ 0.0f };
        float Height{ 0.0f };

        // Set when the control carries a cap, so a value change knows to move it.
        bool HasThumb{ false };
        float ThumbLength{ 0.0f };
        float ThumbSpan{ 0.0f };
        float ThumbInset{ 0.0f };
        float ThumbLineLength{ 0.0f };
        float ThumbLineThickness{ 0.0f };

        bool Vertical{ false };
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

        // The rectangle this item's label paints into, relative to its control's top-left, in
        // page units. False when the control has no label drawn. The editor's label handles are
        // drawn from this, so that dragging them starts from where the text actually is.
        bool TryGetLabelBox(
            _In_ size_t itemIndex,
            _Out_ double& x,
            _Out_ double& y,
            _Out_ double& width,
            _Out_ double& height) const noexcept;

        GlassControlElement ElementAt(_In_ size_t itemIndex) const noexcept;

        ControlKind KindAt(_In_ size_t itemIndex) const noexcept;

        // Where this control sits when nothing is holding it, and whether it goes back there on
        // its own. A pitch wheel does; a volume fader had better not.
        bool ReturnsToRestAt(_In_ size_t itemIndex) const noexcept;
        double RestValueAt(_In_ size_t itemIndex) const noexcept;

        // Moves the drawing. Does not send anything and does not touch the XAML element's value,
        // which the caller owns.
        void SetValue(_In_ size_t itemIndex, _In_ double value) noexcept;

        // The device this control sends to is not here. It is struck through rather than hidden
        // or disabled: a layout with a missing device still has to be editable, and the person
        // looking at it has to be able to see which controls have gone quiet.
        void SetUnavailable(_In_ size_t itemIndex, _In_ bool unavailable) noexcept;

        // Activity, and the only thing that blooms. Decayed by the compositor rather than by a
        // timer on the UI thread, so a wall of blinking controls costs the app nothing.
        void Bloom(_In_ size_t itemIndex) noexcept;
        void ClearBloom(_In_ size_t itemIndex) noexcept;

        // Windows says reduce motion, so the bloom switches instead of fading. The surface is a
        // wall of animation by design, which is exactly why honoring this is not optional.
        void SetReducedMotion(_In_ bool reduced) noexcept { m_reducedMotion = reduced; }

        // What a control shows for its value, when it shows one. Set by the window, because only
        // the window has the binding engine and only the engine knows whether this control's
        // numbers are a percentage or a figure out of a device manual.
        //
        // Called on the hot path, but only for a control that is actually showing a value, which
        // is a handful on a page rather than all of them.
        std::function<std::wstring(uint32_t controlIndex, double value)> DescribeValue{};

        // A finger went down or came up. A control set to show its value only while touched
        // needs to be told; everything else ignores it.
        void SetTouched(_In_ size_t itemIndex, _In_ bool touched) noexcept;

        // Moves a control without rebuilding anything. The editor drags with this, because
        // rebuilding a page of two hundred on every pointer move costs about five milliseconds
        // and a drag has one frame to spend. The label beside it moves too.
        void MoveItem(_In_ size_t itemIndex, _In_ double x, _In_ double y) noexcept;

        // The same for a size change. The track length and the pipe thickness are baked into the
        // geometry, so this re-lays the one control rather than moving it, but it still touches
        // nothing else on the page.
        void ResizeItem(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme) noexcept;

        ThemeColor DeckColor() const noexcept { return m_deck; }

    private:
        void BuildBackground(_In_ LayoutDocument const& document);

        void BuildControl(
            _In_ comp::Compositor const& compositor,
            _In_ Control const& control,
            _In_ Theme const& theme,
            _In_ uint32_t controlIndex);

        // Everything about a control's drawing that depends on its size. Called once when the
        // page is built and again on every frame of a resize drag.
        void LayoutVisual(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ Theme const& theme);

        void LayoutLabel(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        // The number inside the control, for the controls that show one.
        void LayoutValueText(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        void RefreshValueText(_In_ size_t itemIndex) noexcept;

        comp::CompositionColorBrush BrushFor(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& color);

        // Top to bottom, for a plate sheen or a value bar. Cached the same way solid colors are.
        comp::CompositionLinearGradientBrush VerticalBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& top,
            _In_ ThemeColor const& bottom,
            _In_ bool horizontal);

        // White at the top, gone before the middle. Painted with the plate's OWN geometry, so a
        // round control's sheen follows its edge instead of being a rectangle laid over it.
        comp::CompositionLinearGradientBrush SheenBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& color);

        // A brush whose alpha is the shape of a control, for a drop shadow to be cast through.
        // Without one, a shadow is the visual's rectangle, which is how a knob ended up with a
        // square of light behind it. Shared across every control of the same corner radius, so
        // a page of two hundred builds one of these rather than two hundred.
        comp::CompositionBrush ShadowMaskFor(
            _In_ comp::Compositor const& compositor,
            _In_ float width,
            _In_ float height,
            _In_ float cornerRadius,
            _In_ bool round);


        controls::Canvas m_host{ nullptr };

        std::vector<SurfaceVisual> m_visuals{};
        std::vector<GlassControlElement> m_elements{};
        std::vector<uint32_t> m_controlIndexes{};
        std::vector<ControlKind> m_kinds{};

        // Parallel to m_kinds: where a spring-return control goes when it is let go, and which
        // controls do that at all.
        std::vector<double> m_restValues{};
        std::vector<bool> m_returnsToRest{};

        // The number drawn inside a control, for the few that show one. Null everywhere else,
        // so a page of two hundred pays nothing for a feature four of them use.
        std::vector<controls::TextBlock> m_valueTexts{};
        std::vector<double> m_valueOffsets{};
        std::vector<ShowValueOverride> m_showValues{};
        std::vector<bool> m_touched{};

        // The label is a XAML child of the host beside the control, not inside it, so it has to
        // be carried along by hand when the control moves.
        std::vector<controls::TextBlock> m_labels{};
        std::vector<double> m_labelOffsets{};

        // Across, the way m_labelOffsets is down. A label wider than its control, or rotated
        // down one side of it, does not start at the control's own left edge.
        std::vector<double> m_labelInsets{};

        // The rectangle the label actually paints into, relative to its control. Not the same as
        // the offsets above for a rotated label, whose painted strip lands somewhere its text
        // block was never placed. This is what the editor draws handles around.
        std::vector<double> m_labelBoxInsets{};
        std::vector<double> m_labelBoxOffsets{};
        std::vector<double> m_labelBoxWidths{};
        std::vector<double> m_labelBoxHeights{};

        // The page's background picture, behind everything, hit test invisible.
        xaml::FrameworkElement m_background{ nullptr };

        // What each control is showing, so a re-layout can put the pipe back where it was.
        std::vector<double> m_values{};

        // One brush per distinct color for the whole page. A control never owns a brush, which is
        // what keeps a theme swap a handful of objects rather than a walk of two hundred.
        std::unordered_map<uint32_t, comp::CompositionColorBrush> m_brushes{};
        std::unordered_map<uint64_t, comp::CompositionLinearGradientBrush> m_gradients{};

        // Shadow masks, and the offscreen visuals they are rendered from, which have to stay
        // alive for as long as the brush does.
        std::unordered_map<uint64_t, comp::CompositionBrush> m_shadowMasks{};
        std::vector<comp::Visual> m_maskSources{};

        ThemeColor m_deck{};
        bool m_reducedMotion{ false };
    };
}
