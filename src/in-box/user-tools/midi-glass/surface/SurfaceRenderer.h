// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LayoutModel.h"
#include "PadGrid.h"
#include "ThemeModel.h"
#include "ThemeStore.h"
#include "SurfaceColors.h"
#include "GlassControl.h"

#include <winrt/Windows.Media.Playback.h>

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

        // Where that glow sits when nothing is happening. Zero on every theme whose plate can
        // separate itself from its deck on its own; a floor rather than zero on the ones where
        // the spill IS the separation.
        float RestingGlow{ 0.0f };

        comp::ShapeVisual Shape{ nullptr };

        // Light that has to reach past the control's own edges: a knob's glowing arc, the line
        // of light round an outlined section, a lit lamp. Larger than the control and offset so
        // nothing is cut at its bounding box. Null on every control that has none.
        comp::ShapeVisual Halo{ nullptr };

        // The halo is a lamp's, so it is only shown while the switch is on.
        bool HaloFollowsLamp{ false };

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

        // Everything that rides the fader - the cap, its line and the shadow it casts - in one
        // visual, so a value change moves one offset rather than three geometries. A shadow has
        // to be cast by a visual of its own, which is why the cap is not simply more shapes.
        comp::ContainerVisual Cap{ nullptr };

        // A knob's face keeps its own shading under a finger, so touch is a wash laid over it
        // rather than a different brush for the face. Null on every other kind.
        comp::CompositionSpriteShape TouchOverlay{ nullptr };
        comp::CompositionBrush TouchOverlayBrush{ nullptr };

        // The gap cut into a grouping panel's frame for its name. Laid out with the label, which
        // is the only thing that knows how wide the name came out.
        comp::CompositionSpriteShape NotchShape{ nullptr };

        comp::CompositionSpriteShape PlateShape{ nullptr };
        comp::CompositionSpriteShape PipeShape{ nullptr };

        // The line on a knob that says which way it is pointing. Rotated rather than rebuilt.
        comp::CompositionSpriteShape PointerShape{ nullptr };

        // A switch changes what its plate and rim are painted with rather than being rebuilt,
        // so turning one on costs two property sets.
        comp::CompositionBrush PlateOffBrush{ nullptr };
        comp::CompositionBrush PlateOnBrush{ nullptr };

        // What the plate is painted with while a finger is on it. Null on a theme that lifts a
        // glow instead, which is every theme built out of glass.
        comp::CompositionBrush PlateTouchBrush{ nullptr };

        comp::CompositionBrush RimOffBrush{ nullptr };
        comp::CompositionBrush RimOnBrush{ nullptr };

        // The lamp a switch lights instead of filling its plate, on a theme that asks for one.
        // Null everywhere else, which is every theme that says "on" by filling.
        comp::CompositionSpriteShape LampShape{ nullptr };
        comp::CompositionBrush LampOffBrush{ nullptr };
        comp::CompositionBrush LampOnBrush{ nullptr };

        // The rim under a finger. Never null where the control has a rim at all: a touch state
        // carried by a glow alone is a touch state nobody sees on a light theme.
        comp::CompositionBrush RimTouchBrush{ nullptr };

        bool IsSwitch{ false };

        ControlKind Kind{ ControlKind::Knob };

        // "Stays lit for", from this control's listener. Zero means it listens for nothing and
        // the design's own decay is used.
        int32_t FeedbackHoldMilliseconds{ 0 };

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

        // ---- two axis: the XY pad and the joystick ----

        comp::CompositionEllipseGeometry PuckGeometry{ nullptr };
        comp::CompositionRoundedRectangleGeometry CrossAcross{ nullptr };
        comp::CompositionRoundedRectangleGeometry CrossDown{ nullptr };

        // The grid behind a two axis field. Its own visual because it has to be clipped to the
        // shape of the field, which a round one does not share with its bounding box. Kept here
        // so a resize replaces it instead of stacking another one behind it.
        comp::ShapeVisual Grid{ nullptr };

        // The rectangle the puck may sit in, already inset by its own radius so it never hangs
        // off the edge of the field.
        float FieldX{ 0.0f };
        float FieldY{ 0.0f };
        float FieldWidth{ 0.0f };
        float FieldHeight{ 0.0f };
        float PuckRadius{ 0.0f };

        // ---- ribbon: light under the finger rather than a bar that grows ----

        std::vector<comp::CompositionRoundedRectangleGeometry> RibbonGlow{};
        float RibbonSpan{ 0.0f };

        // ---- wheel: ridges and a painted line that roll past as it turns ----

        comp::CompositionContainerShape WheelDrum{ nullptr };
        float WheelTravel{ 0.0f };
        bool WheelVertical{ true };

        // ---- switch: one slice per position, the chosen one lit ----

        int32_t SwitchPositions{ 0 };
        std::vector<comp::CompositionSpriteShape> SwitchSegments{};
        std::vector<winrt::Windows::Foundation::Numerics::float4> SwitchCells{};
        comp::CompositionBrush SwitchLitFill{ nullptr };
        comp::CompositionBrush SwitchRestFill{ nullptr };
        ThemeColor SwitchLitInk{};
        ThemeColor SwitchRestInk{};

        // ---- step sequencer: a slot per step, a bar in it as tall as the step plays hard ----

        std::vector<comp::CompositionSpriteShape> StepSlots{};
        std::vector<comp::CompositionSpriteShape> StepBars{};
        std::vector<comp::CompositionBrush> StepSlotRestFills{};
        comp::CompositionBrush StepSlotLitFill{ nullptr };
        comp::CompositionBrush StepBarRestFill{ nullptr };
        comp::CompositionBrush StepBarLitFill{ nullptr };
        int32_t CurrentStep{ -1 };

        // ---- piano keyboard ----

        std::vector<comp::CompositionSpriteShape> KeyShapes{};
        std::vector<comp::CompositionBrush> KeyRestBrushes{};
        comp::CompositionBrush KeyPressedBrush{ nullptr };
        int32_t PressedKey{ -1 };

        // ---- note pads and hex pads ----

        // One shape per pad, in the order the pads flow, painted from the brushes beside it.
        std::vector<comp::CompositionSpriteShape> PadShapes{};
        std::vector<comp::CompositionBrush> PadRestFills{};
        std::vector<comp::CompositionBrush> PadRestRims{};
        std::vector<comp::CompositionBrush> PadLitFills{};
        comp::CompositionBrush PadLitRim{ nullptr };

        // How many fingers are on each pad. Two on one pad is one lit pad, and it stays lit
        // until both have gone.
        std::vector<uint8_t> PadHeld{};

        // Where the pads were drawn, which is where a finger has to land to play one.
        PadGridLayout PadLayout{};

        // One shadow for every pad, cast through a mask built from their shapes. The mask's
        // source has to stay alive for the mask to render.
        comp::SpriteVisual PadShadow{ nullptr };
        comp::ShapeVisual PadShadowSource{ nullptr };

        // ---- beat clock ----

        comp::CompositionEllipseGeometry SweepGeometry{ nullptr };
        comp::CompositionSpriteShape BeatShape{ nullptr };
        std::vector<comp::CompositionSpriteShape> PipShapes{};
        comp::CompositionBrush PipLitBrush{ nullptr };
        comp::CompositionBrush PipDimBrush{ nullptr };
        comp::CompositionBrush BeatOnBrush{ nullptr };
        comp::CompositionBrush BeatOffBrush{ nullptr };

        // The beat count inside the ring. A XAML text block, because composition has no text.
        controls::TextBlock BeatText{ nullptr };
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

        // Which way a finger drags this control up, and what keys it draws. Both live here
        // rather than being looked up in the document, because input runs on the hot path and
        // the document can be edited underneath a gesture.
        DragAxis DragAxisAt(_In_ size_t itemIndex) const noexcept;
        KeyboardSpec const& KeyboardAt(_In_ size_t itemIndex) const noexcept;

        // Whether this control takes its velocity from how hard it was hit.
        bool VelocityFromTouchAt(_In_ size_t itemIndex) const noexcept;

        // Whether a press latches or has to be held. Only an LFO answers anything but true.
        bool LatchesAt(_In_ size_t itemIndex) const noexcept;

        // How far a platter has to be pushed round to drive it from one end to the other.
        double TurnDegreesAt(_In_ size_t itemIndex) const noexcept;
        int32_t SwitchPositionsAt(_In_ size_t itemIndex) const noexcept;

        // Where this control sits when nothing is holding it, and whether it goes back there on
        // its own. A pitch wheel does; a volume fader had better not.
        bool ReturnsToRestAt(_In_ size_t itemIndex) const noexcept;
        double RestValueAt(_In_ size_t itemIndex) const noexcept;
        double RestValueYAt(_In_ size_t itemIndex) const noexcept;

        // Moves the drawing. Does not send anything and does not touch the XAML element's value,
        // which the caller owns.
        void SetValue(_In_ size_t itemIndex, _In_ double value) noexcept;

        // The other axis of an XY pad or a joystick. Bottom is zero, which is the way every
        // joystick and every plug-in reads and the opposite of the way the screen counts.
        void SetValueY(_In_ size_t itemIndex, _In_ double value) noexcept;

        double ValueYAt(_In_ size_t itemIndex) const noexcept;

        // Which key on a piano keyboard is down, counted from the leftmost. -1 is none.
        void SetPressedKey(_In_ size_t itemIndex, _In_ int32_t key) noexcept;

        // What a note pad or hex pad control plays, and where its pads were drawn. Kept here for
        // the same reason the keyboard is: input runs on the hot path, and a finger has to land
        // on the pad it can see rather than on one worked out again from a document that may
        // have been edited underneath it.
        PadGridSpec const& PadGridAt(_In_ size_t itemIndex) const noexcept;
        PadGridLayout const& PadLayoutAt(_In_ size_t itemIndex) const noexcept;

        // A finger went onto a pad or came off it. Counted, because two fingers can share one.
        void SetPadHeld(_In_ size_t itemIndex, _In_ int32_t cell, _In_ bool held) noexcept;

        // The beat a clock generator is on, and how far through it. Drawn by the compositor
        // from two numbers rather than animated, so the picture can never disagree with the
        // clock that is actually sending.
        void SetBeat(
            _In_ size_t itemIndex,
            _In_ int32_t beatInBar,
            _In_ double phase,
            _In_ bool running) noexcept;

        // What a clock generator is running at, under its ring. Zero means stopped.
        void SetClockTempo(_In_ size_t itemIndex, _In_ double beatsPerMinute) noexcept;

        // Where an LFO's bead sits on the cycle it drew. Running dims the bead when false, so a
        // stopped sweep still shows its shape without looking live.
        void SetSweepPosition(
            _In_ size_t itemIndex,
            _In_ double value,
            _In_ double phase,
            _In_ bool running) noexcept;

        // Which step a step sequencer is playing. -1 lights none, which is what a stopped
        // sequencer shows.
        void SetCurrentStep(_In_ size_t itemIndex, _In_ int32_t stepIndex) noexcept;

        // A time display was tapped, so it counts again from zero.
        void ResetElapsed(_In_ size_t itemIndex) noexcept;

        // Whether a stopwatch is counting. The designer holds it at zero: a clock running while
        // somebody is placing controls is a moving thing in the corner of the eye that has
        // nothing to do with the work, and the number it reaches is the time spent editing.
        void SetElapsedRunning(_In_ bool running) noexcept;

        // Whether videos play. The designer holds every video still on the first frame of the
        // part that plays, for the same reason it holds a stopwatch at zero, and plays one only
        // when the inspector's play button asks. Try mode and the running window play them.
        void SetVideosLive(_In_ bool live) noexcept;

        // Whether this item shows a video at all, and whether a click on it stops and starts it.
        bool HasVideo(_In_ size_t itemIndex) const noexcept;
        bool VideoTakesClicks(_In_ size_t itemIndex) const noexcept;
        bool VideoShowsScrubber(_In_ size_t itemIndex) const noexcept;

        // Plays or stops one video: the inspector's play button, and a click on a running video.
        bool IsVideoPlaying(_In_ size_t itemIndex) const noexcept;
        void SetVideoPlaying(_In_ size_t itemIndex, _In_ bool playing) noexcept;
        void ToggleVideo(_In_ size_t itemIndex) noexcept;

        // Stops a video on the frame at this time, while one end of the part that plays is
        // being dragged in the inspector.
        void ShowVideoFrame(_In_ size_t itemIndex, _In_ double seconds) noexcept;

        // Where a video is and how long its file is, for the inspector's timeline. False until
        // the file has opened.
        bool TryGetVideoPosition(
            _In_ size_t itemIndex,
            _Out_ double& seconds,
            _Out_ double& durationSeconds,
            _Out_ bool& playing) const noexcept;

        // A point in the control's own units, as a fraction along the video's bar. False off the
        // bar, unless `anywhere` is set: a drag that started on the bar keeps scrubbing when the
        // finger wanders off it.
        bool TryGetScrubFraction(
            _In_ size_t itemIndex,
            _In_ double x,
            _In_ double y,
            _In_ bool anywhere,
            _Out_ double& fraction) const noexcept;

        // A finger on the bar. The video holds still under it, and carries on when the finger
        // comes off if it was playing before.
        void ScrubVideo(_In_ size_t itemIndex, _In_ double fraction) noexcept;
        void EndScrub(_In_ size_t itemIndex) noexcept;

        // The device this control sends to is not here. It is struck through rather than hidden
        // or disabled: a layout with a missing device still has to be editable, and the person
        // looking at it has to be able to see which controls have gone quiet.
        void SetUnavailable(_In_ size_t itemIndex, _In_ bool unavailable) noexcept;

        // Activity, and the only thing that blooms. Decayed by the compositor rather than by a
        // timer on the UI thread, so a wall of blinking controls costs the app nothing.
        void Bloom(_In_ size_t itemIndex) noexcept;

        // The same, for something that arrived rather than something the customer did. This one
        // honors the control's own "stays lit for", which is the number the inspector shows.
        void BloomFeedback(_In_ size_t itemIndex) noexcept;

        // A blink: something arrived that carries no value of its own. A control whose state is
        // its plate - a lamp above all - lights up for "stays lit for" and then goes out, which
        // is the whole job of a lamp. Everything else just glows.
        void FlashFeedback(_In_ size_t itemIndex) noexcept;

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
        std::function<std::wstring(uint32_t controlIndex, ValueAxis axis, double value)> DescribeValue{};

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
        // A video on the page: its player, what it was asked to do, and the bar along its bottom.
        // Shared, so the player's events, which arrive on media threads and are handed to the UI
        // thread, can tell when the page they were meant for has gone.
        struct SurfaceVideo
        {
            winrt::Windows::Media::Playback::MediaPlayer Player{ nullptr };
            Picture Spec{};

            // The element the frames land in, so a closed player can be taken out of it.
            controls::MediaPlayerElement Element{ nullptr };

            // The control it belongs to, for its status, or null for the page background.
            GlassControlElement Owner{ nullptr };

            // A panel's fill and the page background play and loop, but take no clicks and
            // draw no bar: both sit behind the controls.
            bool TakesInput{ false };

            // The control's own size, which the bar is laid out inside.
            double Width{ 0.0 };
            double Height{ 0.0 };

            // Known once the file has opened. Until then the part that plays is whatever the
            // layout says.
            bool Opened{ false };
            double DurationSeconds{ 0.0 };

            // What it has been asked to do. The player is told the same whenever it can listen.
            bool Playing{ false };

            // A frame asked for before the file had opened, or below zero for none.
            double PendingFrameSeconds{ -1.0 };

            // Held still under a finger on the bar, and whether to carry on afterward.
            bool Scrubbing{ false };
            bool ResumeAfterScrub{ false };

            // The part of the control the video covers, which the bar spans.
            PictureRect Visible{};

            controls::Canvas Scrubber{ nullptr };
            xaml::Shapes::Rectangle ScrubTrack{ nullptr };
            xaml::Shapes::Rectangle ScrubFill{ nullptr };
            xaml::Shapes::Ellipse ScrubThumb{ nullptr };

            // What a screen reader was last told, so it is only told again when it changes.
            std::wstring Status{};
        };

        // A new video on the page, silent, paused and waiting for its file to open.
        std::shared_ptr<SurfaceVideo> CreateVideo(
            _In_ foundation::Uri const& uri,
            _In_ Picture const& picture,
            _In_ double width,
            _In_ double height,
            _In_ bool takesInput);

        // Shuts a video's player down. A player left open keeps a decoder and its threads alive
        // long after the page it was on has gone.
        static void CloseVideo(_Inout_ SurfaceVideo& video) noexcept;

        SurfaceVideo* VideoAt(_In_ size_t itemIndex) const noexcept;

        // Tells the player what the video was asked to do, once it can listen.
        void ApplyVideoState(_Inout_ SurfaceVideo& video) noexcept;

        // The file opened, or played through to its end.
        void OnVideoOpened(_Inout_ SurfaceVideo& video) noexcept;
        void OnVideoEnded(_Inout_ SurfaceVideo& video) noexcept;

        // The bar along the bottom, across the part of the control the video covers.
        void LayoutScrubber(_Inout_ SurfaceVideo& video) noexcept;
        void RefreshScrubber(_Inout_ SurfaceVideo& video, _In_ double seconds) noexcept;

        // Ticks while any video plays: holds each to the part it plays and moves the bars.
        void RefreshVideos() noexcept;
        void StartVideoTimerIfNeeded();
        void StopVideoTimer() noexcept;

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
            _In_ Theme const& theme,
            _In_ PrintSurface surface);

        // What is printed under the middle of this item: the deck, a section or an inset.
        PrintSurface SurfaceFor(_In_ size_t itemIndex, _In_ Control const& control) const noexcept;

        // Concentric strokes of a shape's own geometry, widest and faintest first, into the
        // item's halo visual. `baseThickness` is the stroke the shape itself is drawn with.
        void AppendHalo(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ comp::CompositionGeometry const& geometry,
            _In_ ThemeColor const& color,
            _In_ float baseThickness,
            _In_ float reach,
            _In_ double peak,
            _In_ bool roundCaps);

        // A printed rule, across or down.
        void LayoutLine(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ThemeColor const& ruleColor,
            _In_ bool fades,
            _In_ float width,
            _In_ float height);

        void LayoutLabel(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        // The picture or video a control shows, and the fill behind a grouping panel. A XAML
        // sibling rather than a composition brush, because that is the one way png, jpg, svg
        // and video are all the same amount of work.
        void LayoutPicture(
            _In_ size_t itemIndex,
            _In_ Control const& control);

        // The two things a picture can turn out to be. Both end up as an element already sized
        // and positioned for the crop, sitting inside the clipped container LayoutPicture made.
        // A video's element is the one in the SurfaceVideo that CreateVideo hands back.
        xaml::FrameworkElement BuildImageContent(
            _In_ foundation::Uri const& uri,
            _In_ Picture const& picture,
            _In_ double width,
            _In_ double height);

        // The wash of color over a picture, or nullptr when there is none.
        static xaml::FrameworkElement BuildPictureTint(
            _In_ Picture const& picture,
            _In_ double width,
            _In_ double height);

        // Shuts down the media player behind a video, if that is what this picture was.
        static void ClosePicture(_In_ xaml::FrameworkElement const& element) noexcept;

        // The beat count inside a clock's ring, and where it sits. A XAML sibling like the
        // label, because composition has no text.
        void LayoutBeatText(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        // The elapsed time a time display shows, and the timer that keeps it moving.
        void LayoutElapsedText(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        void RefreshElapsedTexts() noexcept;
        void StartElapsedTimerIfNeeded();

        // The number at each stop, for a control asked to show them. One canvas per control
        // holding one text block per stop, so moving the control moves them all at once.
        void LayoutDetentValues(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        // The field of a control that shows something, sunk into its plate. Nothing on a theme
        // that asks for no well.
        void AppendWell(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ ControlColors const& colors,
            _In_ float x,
            _In_ float y,
            _In_ float width,
            _In_ float height);

        // The two axis field, for the XY pad and the joystick.
        void LayoutTwoAxis(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height);

        void LayoutRibbon(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height);

        void LayoutKeyboard(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        // The pads of a note pad or hex pad control (SurfacePads.cpp).
        void LayoutPads(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ Theme const& theme,
            _In_ float width,
            _In_ float height);

        // The name printed on each pad. XAML text, like a label, because composition has none.
        void LayoutPadNames(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        // What a pad grid's plate comes out as over the deck, which is what every color on its
        // pads is measured against.
        ThemeColor PadBackdrop(_In_ Control const& control, _In_ Theme const& theme) const noexcept;

        void LayoutClock(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        void LayoutLfo(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        void LayoutTurntable(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        void LayoutWheel(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        void LayoutSwitch(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        void LayoutSteps(
            _In_ comp::Compositor const& compositor,
            _Inout_ SurfaceVisual& visual,
            _In_ Control const& control,
            _In_ ControlColors const& colors,
            _In_ float width,
            _In_ float height);

        // The name on each position of a switch. Kept in the same per-control slot as the note
        // names on a pad grid, because a control is one or the other.
        void LayoutSwitchLabels(_In_ size_t itemIndex, _In_ Control const& control);

        // Puts a two axis control's puck and crosshair where its two values say, and a ribbon's
        // light where its one value says.
        void MovePuck(_In_ size_t itemIndex) noexcept;
        void MoveRibbonLight(_In_ size_t itemIndex) noexcept;

        // The number inside the control, for the controls that show one.
        void LayoutValueText(
            _In_ size_t itemIndex,
            _In_ Control const& control,
            _In_ Theme const& theme);

        void RefreshValueText(_In_ size_t itemIndex) noexcept;

        void BloomFor(_In_ size_t itemIndex, _In_ int64_t milliseconds) noexcept;

        // Paints a control's plate for whichever state it is in: on wins over touched, and
        // touched wins over at rest. One place, because two call sites deciding it separately is
        // how a control ends up stuck looking held after a switch is turned off under a finger.
        void ApplyPlateBrush(_In_ size_t itemIndex) noexcept;

        // Turns off every control whose lit time has run out. One timer for the whole page
        // rather than one per blink, because a busy page blinks a lot.
        void SweepFlashes() noexcept;

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

        // The mirror of the sheen: the shadow color at the bottom, gone before the middle.
        comp::CompositionLinearGradientBrush ShadeBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& color);

        // Clear at both ends and the color in the middle, along a line.
        comp::CompositionLinearGradientBrush FadedLineBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& color,
            _In_ bool across);

        // The shadow inside something cut into the surface: strongest at its top edge and gone a
        // few pixels down. The fade is a fraction of the recess's own height.
        comp::CompositionLinearGradientBrush RecessBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& color,
            _In_ float fade);

        // A knob's face or cap, lit from above the middle and falling off to its edge.
        comp::CompositionRadialGradientBrush DomeBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ThemeColor const& lit,
            _In_ ThemeColor const& edge);

        // A meter's three zones, mapped to the TRACK rather than to the bar, so the boundaries
        // stay where the marks are as the bar grows past them.
        comp::CompositionLinearGradientBrush MeterBrush(
            _In_ comp::Compositor const& compositor,
            _In_ ControlColors const& colors,
            _In_ float trackOrigin,
            _In_ float trackLength,
            _In_ bool vertical);

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
        std::vector<double> m_restValuesY{};
        std::vector<bool> m_returnsToRest{};
        std::vector<DragAxis> m_dragAxes{};
        std::vector<KeyboardSpec> m_keyboards{};
        std::vector<PadGridSpec> m_padGrids{};
        std::vector<bool> m_velocityFromTouch{};
        std::vector<bool> m_latches{};
        std::vector<double> m_turnDegrees{};

        // The picture or video a control shows, and the fill behind a grouping panel. A XAML
        // child of the host like the label, so it has to be carried when the control moves.
        std::vector<xaml::FrameworkElement> m_pictures{};

        // Parallel to m_pictures: the video each one plays, null for a still picture.
        std::vector<std::shared_ptr<SurfaceVideo>> m_videos{};

        // A video behind the whole page, or null.
        std::shared_ptr<SurfaceVideo> m_backgroundVideo{};

        // Ticks only while a video is playing.
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_videoTimer{ nullptr };

        // False while a layout is being designed rather than run.
        bool m_videosLive{ true };

        // The numbers beside a stepped control's marks, one canvas per control.
        std::vector<controls::Canvas> m_detentTexts{};

        // The note printed on each pad of a pad grid: one canvas per control holding a text block
        // per pad, and the ink each wears at rest and lit, so a pad lighting up keeps its name.
        struct PadNameTexts
        {
            controls::Canvas Host{ nullptr };
            std::vector<controls::TextBlock> Texts{};
            std::vector<media::Brush> RestInks{};
            std::vector<media::Brush> LitInks{};
        };

        std::vector<PadNameTexts> m_padNames{};

        // The beat count a clock generator shows. One text block per clock, null everywhere
        // else, so a page with no clock on it pays nothing.
        std::vector<controls::TextBlock> m_beatTexts{};
        std::vector<double> m_beatTextOffsets{};

        // The tempo under a clock's ring, and the elapsed time a time display shows. Both are
        // text blocks for the same reason: composition has none.
        std::vector<controls::TextBlock> m_tempoTexts{};
        std::vector<double> m_tempoTextOffsets{};
        std::vector<controls::TextBlock> m_elapsedTexts{};
        std::vector<double> m_elapsedTextOffsets{};

        // When each time display was last started, as a tick count. Zero means it is not one.
        std::vector<uint64_t> m_elapsedOrigins{};

        // Ticks only while the page holds at least one time display.
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_elapsedTimer{ nullptr };

        // False while a layout is being designed rather than run.
        bool m_elapsedRunning{ true };

        // When a blinking control goes out again, as a tick count. Zero means it is not lit.
        std::vector<uint64_t> m_litUntil{};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_flashTimer{ nullptr };

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

        // A name inside a switch, in the ink that reads on the plate at rest and on the plate
        // lit. Null for every label that does not sit on a switch, which keeps its one ink.
        std::vector<media::Brush> m_labelRestInks{};
        std::vector<media::Brush> m_labelOnInks{};

        // The page's background picture, behind everything, hit test invisible.
        xaml::FrameworkElement m_background{ nullptr };

        // What each control is showing, so a re-layout can put the pipe back where it was.
        std::vector<double> m_values{};
        std::vector<double> m_valuesY{};

        // One brush per distinct color for the whole page. A control never owns a brush, which is
        // what keeps a theme swap a handful of objects rather than a walk of two hundred.
        std::unordered_map<uint32_t, comp::CompositionColorBrush> m_brushes{};
        std::unordered_map<uint64_t, comp::CompositionLinearGradientBrush> m_gradients{};
        std::unordered_map<uint64_t, comp::CompositionRadialGradientBrush> m_domes{};

        // Shadow masks, and the offscreen visuals they are rendered from, which have to stay
        // alive for as long as the brush does.
        std::unordered_map<uint64_t, comp::CompositionBrush> m_shadowMasks{};
        std::vector<comp::Visual> m_maskSources{};

        ThemeColor m_deck{};

        // Every filled grouping panel on the page, so a control or a label can be inked for the
        // surface it is printed on.
        std::vector<PanelFootprint> m_panels{};

        // How tall the page is, so a notch in a panel's frame can be filled with the deck color
        // at that height rather than the color at the top of the page.
        double m_pageHeight{ 0.0 };

        bool m_reducedMotion{ false };

        // How long a control stays lit when nothing else has said. The theme's own number where
        // it named one, because persistence belongs to the phosphor rather than to a binding.
        int64_t m_decayMilliseconds{ 220 };

        // Where the layout file is, so a control's picture resolves against its own folder.
        std::wstring m_layoutFilePath{};
    };
}
