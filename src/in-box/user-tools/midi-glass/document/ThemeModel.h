// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glass
{
    constexpr int32_t ThemeHueSlotCount = 6;

    // Straight 8 bits per channel. The surface never needs more, and a theme file a person can
    // read and hand-edit is worth more here than a wide gamut nobody asked for.
    struct ThemeColor
    {
        uint8_t R{ 0 };
        uint8_t G{ 0 };
        uint8_t B{ 0 };
        uint8_t A{ 255 };

        bool operator==(ThemeColor const& other) const noexcept
        {
            return R == other.R && G == other.G && B == other.B && A == other.A;
        }
    };

    enum class DeckKind
    {
        SolidColor = 0,
        Gradient = 1,
        Image = 2,
    };

    enum class LabelPlacement
    {
        Inside = 0,
        Below = 1,
        None = 2,

        // Above the control, which is where a hardware panel silkscreens it.
        Above = 3,
    };

    // How a grouping panel says what it is. A caption on the deck is what a group box does
    // everywhere; a filled bar is what a synthesizer front panel does, and on a dark panel that
    // bar is the only thing marking where one section ends and the next starts.
    enum class SectionHeaderStyle
    {
        Caption = 0,
        FilledBar = 1,

        // The name sits in a gap cut into the top of the frame, the way a group box is printed
        // on a hardware panel. It reads right on a panel drawn as an outline.
        Notched = 2,

        // The name centered across the top of the section, inside it.
        Centered = 3,
    };

    // The lamp a switch lights when the theme says "on" with a lamp rather than a fill.
    enum class LampStyle
    {
        // A short bar across the top of the cap.
        Bar = 0,

        // A round lens, which is what a panel lamp of the seventies and eighties is.
        Dot = 1,
    };

    // What fills a grouping panel.
    enum class PanelFillStyle
    {
        // The same plate a control gets, which is what every theme did before this.
        Plate = 0,

        // A color of its own, one step between the page and the controls on it.
        Color = 1,

        // Nothing at all: the panel is an outline on the deck.
        None = 2,
    };

    // What a fader's slot is cut into.
    enum class FaderPlateStyle
    {
        // A plate the size of the control, which is what every theme did before this.
        Full = 0,

        // A narrow strip of molding around the slot, with the cap wider than it.
        Strip = 1,

        // No plate: the slot is cut straight into the panel.
        None = 2,

        // The strip's shape, but cut into the panel rather than standing on it: the rim and the
        // resting glow go around the slot and the marks are printed outside them.
        Frame = 3,
    };

    // Where the rim color comes from. Bigwig needs a neutral edge so that orange only ever means
    // "this is the value", which is what keeps a dense page readable.
    enum class RimSource
    {
        ControlHue = 0,
        NeutralEdge = 1,
        None = 2,
    };

    enum class ValueStripPlacement
    {
        Bottom = 0,
        Top = 1,
        None = 2,
    };

    enum class ValueIndicatorStyle
    {
        SolidArc = 0,
        SegmentedLamps = 1,
    };

    // What color activity lights up in. Bone lights up white, because on a bone deck a control
    // is already near-white and the only room left to say "this one just did something" is to
    // take it the rest of the way.
    //
    // Superseded by BloomColor, which can say any color rather than these two. Still read from
    // theme files written before that existed, and still what an untouched theme means.
    enum class LightSource
    {
        ControlHue = 0,
        White = 1,
    };

    // The cap on a fader. Studio Dark's is a neutral machined bar with a hairline of the
    // control's hue through it; the tonal themes and Bigwig make the whole cap the hue.
    enum class ThumbStyle
    {
        None = 0,
        Neutral = 1,
        Hue = 2,
    };

    struct ThemeDeck
    {
        DeckKind Kind{ DeckKind::SolidColor };
        ThemeColor Color{};
        ThemeColor GradientEndColor{};

        // A bare file name inside the shared assets folder, never a path out of it.
        std::wstring ImageFileName{};
    };

    // The scan lines, the corner fall-off and the faceplate reflection: everything a theme lays
    // over the whole deck once, rather than over each control.
    //
    // One overlay for the page, never one per control, so eighty controls cost what four cost.
    // Drawn after the page scale as well, because a three pixel pitch in page units at 87 per
    // cent zoom is a beat pattern across the screen rather than a row of scan lines.
    struct DeckOverlay
    {
        // Distance between two scan lines, in screen pixels. Zero is off, which is every theme
        // that is not a cathode ray tube.
        int32_t ScanLinePitch{ 0 };

        // How dark each of those lines is, 0 to 100.
        int32_t ScanLineStrength{ 0 };

        ThemeColor ScanLineColor{ 0, 0, 0, 255 };

        // How hard the deck falls off at the corners, 0 to 100. A tube is brighter in the
        // middle because the raster scatters inside the glass.
        int32_t VignettePercent{ 0 };

        // What it falls off to. Alpha 0 means the deck's own floor, so a theme only has to name
        // this when it wants the corners to go somewhere its deck does not.
        ThemeColor VignetteColor{ 0, 0, 0, 0 };

        // The room reflected in the glass: a soft diagonal highlight across the upper left. It
        // is in every photograph of a real terminal, and it is what makes glass read as glass
        // rather than as paint.
        int32_t FaceplateSheenPercent{ 0 };

        // Alpha 0 means a cool white, which is what a lit room reflects.
        ThemeColor FaceplateSheenColor{ 0, 0, 0, 0 };

        // How coarse the deck's texture is, 0 to 100. A panel that has been bead blasted or
        // wrinkle painted is not a flat color, and the grain is most of why it reads as a
        // manufactured object rather than a fill.
        //
        // It is a TILE rather than a picture. An image deck is stretched to the page, so a
        // 120 pixel noise tile stretched across 1280 turns into blur; this is one brush over the
        // whole deck, drawn once, nothing per control. At full it is texture and at a quarter it
        // is only a warmth, which is why it is a number rather than a switch.
        int32_t GrainPercent{ 0 };

        // Alpha 0 means light and dark speckle worked out from the deck itself.
        ThemeColor GrainColor{ 0, 0, 0, 0 };

        // How long one speck is along the page, in grain cells. One is a speck, which is
        // sandpaper; forty or more is a streak, which is brushed metal.
        int32_t GrainStreak{ 1 };

        bool IsEmpty() const noexcept
        {
            return (ScanLinePitch <= 0 || ScanLineStrength <= 0) &&
                VignettePercent <= 0 &&
                GrainPercent <= 0 &&
                FaceplateSheenPercent <= 0;
        }
    };

    // Which hue slot each of a meter's three zones is drawn in. A meter is the one control where
    // a heat ramp does real work, and the trio is not the same on every theme: Bigwig follows
    // its desk, and a tube theme has no green, amber and red to have, so it spends the widest
    // three brightnesses it owns instead.
    constexpr int32_t MeterZoneCount = 3;

    // A theme is six hue slots, a deck, and a handful of control defaults. A control stores a
    // slot rather than a color, so switching theme is a six color operation instead of a
    // redesign.
    struct Theme
    {
        std::wstring Name{};

        // Set on the ones that ship, so the UI can offer "reset" and refuse to overwrite them.
        bool IsBuiltIn{ false };

        std::array<ThemeColor, ThemeHueSlotCount> HueSlots{};
        ThemeDeck Deck{};

        int32_t CornerRadius{ 7 };

        // 0 is opaque, 100 is the original glass. The one skeuomorphic thing in the app, and it
        // is doing real work: it lets a themed deck show through so a surface reads as one object
        // rather than a scatter of stickers.
        int32_t GlassTintPercent{ 86 };

        // What the glass is made of. Smoked slate, not black: a plate of black at 86 percent
        // over a near-black deck comes out at almost zero, and the control stops reading as a
        // piece of glass sitting on the surface and starts reading as a hole cut out of it.
        ThemeColor GlassColor{ 0x11, 0x14, 0x1A, 255 };

        int32_t GlowStrength{ 60 };

        // The sheen down the top of a plate, as a percentage of its own color. It is what makes
        // a plate read as a raised piece of glass rather than a filled rectangle. Per theme
        // rather than a constant, because a white sheen on a near-white plate is just haze.
        int32_t PlateSheenPercent{ 6 };

        // What that sheen is made of. Alpha 0 means white, which is where it started and what
        // every dark theme wants. A warm plate does not: measured on Terminal Amber, white at
        // 7 per cent over #3A2413 overshoots blue by eight counts and the warm lift goes gray.
        //
        // This is the third time a percentage of pure white has needed a color of its own, after
        // the shadow and the bloom. If a fourth is ever added to this model, give it one at
        // birth rather than waiting for the theme that proves it.
        ThemeColor PlateSheenColor{ 0, 0, 0, 0 };

        // How hard a plate sits above the deck, 0 to 100. The tonal themes deliberately carry
        // elevation as a tint of the surface's own color instead, and set this to zero.
        int32_t PlateElevation{ 55 };

        // How far that shadow reaches, as a blur radius in pixels. Elevation is how DARK the
        // shadow is; this is how FAR it goes, and they are not the same question. On a dark deck
        // a shadow is nearly invisible whatever it does, so 3 was fine for every theme until one
        // arrived that has nothing else to separate a control from its deck. Measured on screen:
        // at 3 a white plate on a bone deck reads as a flat rectangle with an outline.
        int32_t ShadowSpread{ 3 };

        // What that shadow is made of. Black everywhere it has always been black, so nothing
        // shipped moves. Bone needs it warm: a black shadow on a bone deck comes out a dirty
        // gray, and on that theme the shadow is not decoration - a warm white plate measures
        // 1.23 : 1 against a bone deck, so the shadow is the only thing separating a control
        // from the space behind it.
        ThemeColor ShadowColor{ 0, 0, 0, 255 };

        LightSource Light{ LightSource::ControlHue };

        // What a control's activity lights up in, when it is not simply the control's own hue.
        // Alpha 0 means work it out from Light, so nothing written before this existed moves.
        //
        // Three themes need a color here and none of them could be expressed by an enum. A
        // cathode ray tube's phosphor is blue-white, not white. An amber tube's halo is redder
        // than the thing casting it, because P3 decays through red, and that single fact is what
        // people recognize as "the amber screen". A green terminal's is yellower than its
        // emission, which is the same shift at the other end.
        ThemeColor BloomColor{ 0, 0, 0, 0 };

        // How much of that light a lit control spills when nothing is happening to it, 0 to 100.
        // GlowStrength is activity; this is the floor underneath it, and it costs no new layer.
        //
        // On the three tube themes this is structure rather than decoration: a raster box
        // measures 1.11 : 1 to 1.22 : 1 against the middle of its own deck, so the spill is the
        // only thing separating a control from the glass. A theme editor has to refuse to let it
        // reach zero on those, exactly as it has to refuse to flatten Bone's shadow.
        int32_t RestingGlowPercent{ 0 };

        // How long a control stays lit after something arrives for it, in milliseconds. Zero
        // means the design's own decay. A phosphor's persistence belongs to the tube rather
        // than to one binding, and P3 holds on far longer than a screen does.
        int32_t PersistenceMilliseconds{ 0 };

        // The far end of the value bar, as a fraction of the hue's own alpha. The bar is
        // brightest where the value is and falls away behind it, which is what stops a long
        // fader reading as a flat stripe. 1.0 is a solid bar.
        double PipeFalloff{ 0.35 };

        // The far end of that bar is the light's color rather than the hue's. Long persistence
        // drawn rather than animated: on a P3 tube the top of the fill is where the beam just
        // was and everything under it has had time to decay toward red. It is what makes an
        // amber fader look like fire.
        bool ValueFadesToLight{ false };

        ThumbStyle Thumb{ ThumbStyle::Neutral };

        // The two ends of a neutral cap. Ignored when Thumb is not Neutral.
        ThemeColor ThumbColor{ 0x31, 0x39, 0x45, 255 };
        ThemeColor ThumbEndColor{ 0x16, 0x1A, 0x21, 255 };

        LabelPlacement Labels{ LabelPlacement::Below };

        // Tonal. A wash of the control's own hue at rest, instead of an outline. This is what
        // makes the tonal themes read as a different family rather than a recolor.
        double FillAtRest{ 0.0 };

        // More of that wash while a finger is on it, 0 to 100. On a flat theme this IS the touch
        // state: there is no glow to lift, so the plate has to say it. A glass theme leaves it at
        // zero and lets the bloom do the work.
        int32_t TouchFillPercent{ 0 };

        // The same wash, for a switch rather than for a knob or a fader. Below zero means follow
        // FillAtRest, which is what every theme did before this.
        //
        // Earned by a panel whose tabs are solid color at rest while its knobs on the same page
        // are bare black. One number for the whole theme cannot say that: turning the tabs on
        // fills the knobs too.
        double SwitchFillAtRest{ -1.0 };

        // How much of the control's own hue the plate carries while it is ON, 0 to 100. A third
        // is what every theme was drawn with before this existed.
        //
        // It is an amount of hue rather than a multiplier on one, because the two ends of the
        // range are both real themes. At zero a switch lights only its lamp and the plate stays
        // where it was - a rack of sixteen identical black switches with one lamp lit is far
        // easier to read at arm's length than sixteen colored blocks, because the eye only has
        // to find the bright thing. At a hundred the plate becomes the hue outright, which is
        // what a panel of colored tabs needs: its switches are already most of the way there at
        // rest, so a THIRD of the remaining distance is a change nobody can see.
        int32_t FillWhenOnPercent{ 34 };

        // The line on a knob that says which way it is pointing. Alpha 0 means work it out from
        // the hue and the rim source, which is what every theme did before this.
        //
        // This is the mirror of the white-and-black rule below: anything derived from the
        // CONTROL'S HUE eventually meets a theme where it is not the hue. A panel whose every
        // knob points in the section color is that theme.
        ThemeColor PointerColor{ 0, 0, 0, 0 };

        // The wide line across a fader cap. Alpha 0 means the control's hue. Two hardware panels
        // in a row draw it white on every fader whatever the fader is for.
        ThemeColor CapLineColor{ 0, 0, 0, 0 };

        // The one un-hued color, reached by naming the neutral slot on a control. Alpha 0 means
        // the theme has none and that slot falls back to the first hue.
        //
        // Earned by a panel with seven button colors on it: red, orange, yellow, green, aqua and
        // blue is exactly six, and the cream tabs beside them are the ABSENCE of a color rather
        // than a seventh one. Without somewhere to put that, a customer reaches for a literal
        // color and loses it at the next theme swap.
        ThemeColor NeutralColor{ 0, 0, 0, 0 };

        // How a grouping panel says what it is.
        SectionHeaderStyle SectionHeader{ SectionHeaderStyle::Caption };

        // The panel's name in its own color rather than in the ink. A section that says its
        // name in its color is how a dense page tells you where one group ends.
        bool SectionNameInHue{ false };

        // What fills a grouping panel, and with what. The two colors are only read when the
        // fill is Color; alpha 0 on the second one means a flat fill.
        PanelFillStyle PanelFill{ PanelFillStyle::Plate };
        ThemeColor PanelColor{ 0, 0, 0, 0 };
        ThemeColor PanelEndColor{ 0, 0, 0, 0 };

        // The line around a grouping panel. Alpha 0 means the rim, which is what every theme did
        // before this. A panel drawn as an outline wants a faint light line rather than a hue.
        ThemeColor PanelOutlineColor{ 0, 0, 0, 0 };

        // A panel that sits inside another panel. Alpha 0 means it is drawn like any other panel.
        // Named, it is printed flat: a second layer of ink on the first, so it casts no shadow.
        ThemeColor InsetPanelColor{ 0, 0, 0, 0 };
        ThemeColor InsetPanelEndColor{ 0, 0, 0, 0 };

        // How hard a section sits above the deck, 0 to 100. Below zero means the plate's own
        // elevation, which is what every theme did before this. A printed section has none.
        int32_t PanelElevation{ -1 };

        // The ink for anything printed on a section, rather than on an inset or on the deck.
        // Alpha 0 means the theme's ink wherever it is printed.
        //
        // Earned by a panel printed on two surfaces a long way apart in value: white print
        // measures 1.5 : 1 on its tan sections and black print 3.0 : 1 on its green insets, so
        // neither ink can do both.
        ThemeColor SectionInkColor{ 0, 0, 0, 0 };

        // ---- knobs ----

        // The top face of a knob, lit from above and falling off to its edge. Alpha 0 means the
        // plate, which is what every theme did before this. A turned knob cap is brighter where
        // the light catches it, and a flat disc of plate color reads as a sticker.
        ThemeColor KnobFaceColor{ 0, 0, 0, 0 };
        ThemeColor KnobFaceEndColor{ 0, 0, 0, 0 };

        // A small cap in the middle of that face, as a share of it. Alpha 0 means none, which
        // leaves the small center dot every theme had before this.
        ThemeColor KnobCapColor{ 0, 0, 0, 0 };
        ThemeColor KnobCapEndColor{ 0, 0, 0, 0 };
        int32_t KnobCapSizePercent{ 28 };

        // A ring of printed marks around every knob, outside its arc. Zero means none. A control
        // that asks for its own marks still gets those instead.
        int32_t KnobTickCount{ 0 };

        // The pointer is printed on the cap, the part that turns, from the cap's edge to near its
        // middle. Off, the cap covers the pointer's inner end, which is what every theme did.
        bool PointerOnCap{ false };

        // A knob's arc carries the same halo a fader's fill does. Off on every theme drawn before
        // this, so nothing shipped moves.
        bool ArcGlow{ false };

        // The unlit part of a knob's arc in the control's own hue at this strength, 0 to 100,
        // rather than one track color for every knob. Zero means the arc track color.
        int32_t ArcTrackHuePercent{ 0 };

        // ---- switches ----

        // A switch shows its name in the middle of itself, whatever the theme does with every
        // other label. Knobs and faders can have their names above or below while buttons carry
        // theirs, which is what a real panel does.
        bool NamesInsideSwitches{ false };

        // How far a lit switch moves toward white, or toward black when this is below zero,
        // -100 to 100. Zero is what every theme did before this. A colored tab lit from behind
        // gets paler; a colored button on paper gets deeper so its white name still reads.
        int32_t OnLiftPercent{ 0 };

        // The lamp a switch lights when the theme says "on" with a lamp rather than a fill.
        // Alpha 0 means the control's own hue.
        ThemeColor LampColor{ 0, 0, 0, 0 };

        LampStyle LampShape{ LampStyle::Bar };

        // The rim and the resting glow of anything that is pressed or read rather than turned or
        // slid, 0 to 100. Below zero means the same as every other control, which is what every
        // theme did before this.
        //
        // Earned by a panel whose knobs and faders are lit all the time while its buttons are
        // black until they are on. One number for the whole theme gives either every button a
        // glow at rest, so turning one on is a smaller change than it should be, or no fader a
        // lit frame at all.
        int32_t SwitchRimStrengthPercent{ -1 };
        int32_t SwitchRestingGlowPercent{ -1 };

        // A pad is its own family: colored plastic with a lamp behind it, on the same page as
        // black buttons. Below zero means the same as every other switch.
        double PadFillAtRest{ -1.0 };
        int32_t PadFillWhenOnPercent{ -1 };

        // A fader or a switch on the neutral slot wears the neutral as its cap, the way one row
        // of cream caps on a panel of black ones says "these belong together".
        bool NeutralCaps{ false };

        // ---- plates ----

        // A shade up from the bottom of a plate, 0 to 100, in the shadow color. The mirror of
        // the sheen: a glossy button is lighter at the top AND darker at the bottom.
        int32_t PlateShadePercent{ 0 };

        // A one pixel light line just inside the top edge of a plate, 0 to 100, in the sheen
        // color. It is what makes molded plastic read as molded.
        int32_t PlateHighlightPercent{ 0 };

        // ---- faders and displays ----

        // What a fader's slot is cut into.
        FaderPlateStyle FaderPlate{ FaderPlateStyle::Full };

        // How strong a fader's fill is, 0 to 100. A panel where the cap position is the whole
        // value wants the slot below it only faintly lit.
        int32_t FaderFillPercent{ 100 };

        // The color every value is drawn in: a knob's arc, a fader's fill, a puck. Alpha 0 means
        // the control's own hue, which is what every theme did before this. A panel whose
        // switches are all different colors can still draw every value in one.
        ThemeColor ValueColor{ 0, 0, 0, 0 };

        // The shadow inside anything cut into the surface - a slot, a display - 0 to 100, in the
        // shadow color. What holds a value is sunk, and this is what sells it.
        int32_t RecessShadePercent{ 0 };

        // The field of a control that shows something - an XY pad, an LFO, a ribbon - as a
        // well cut into its plate. Alpha 0 means no well, which is what every theme did before.
        ThemeColor WellColor{ 0, 0, 0, 0 };

        // A drop shadow under a fader's cap, 0 to 100. A cap standing off its slot is most of
        // why a fader reads as something to grab.
        int32_t ThumbShadowPercent{ 0 };

        // The line across a fader cap runs nearly its full width and three pixels thick, the way
        // a hardware panel paints it, rather than a hairline.
        bool CapLineWide{ false };

        // How strongly a fader's scale is printed beside its slot, 0 to 100, in the ink. Zero is
        // the faint marks inside a plate that every theme drew before this. A printed scale
        // reaches the control's edges, the way it does beside a slider on a hardware panel.
        int32_t FaderScalePercent{ 0 };

        // ---- lines ----

        // A line control, and the rules a hardware panel prints between groups of sections.
        // Alpha 0 means the ink at a sixth of its strength.
        ThemeColor RuleColor{ 0, 0, 0, 0 };

        // The line fades out at both ends rather than stopping square.
        bool RuleFades{ true };

        // ---- the piano keyboard ----

        // The keys, where a theme names them. Alpha 0 means a plain white and black. The light
        // one is the natural keys and the dark one is the sharps and flats, and both come from
        // the theme's own two ends rather than from a piano.
        ThemeColor KeyWhiteColor{ 0, 0, 0, 0 };
        ThemeColor KeyBlackColor{ 0, 0, 0, 0 };

        // How strong a control's rim is when nothing is happening to it. A quarter strength is
        // what keeps a busy page readable - nothing is saturated at rest, and the value and the
        // activity are the only things allowed to be bright.
        //
        // It is a property rather than a constant because a hairline behaves differently on a
        // light plate. Measured against each theme's own plate, 28 lands at 1.8 : 1 to 2.2 : 1
        // on the six dark themes and only 1.4 : 1 on the light ones, where the rim simply
        // vanishes and takes the control's identity with it - six faders on six different slots
        // all look the same. Bone runs 85.
        int32_t RimStrengthPercent{ 28 };

        // Tonal. The empty part of a fader slot or a knob arc. The dark themes get away with
        // hardcoding this black, which is a gap in the model rather than a cost of those themes:
        // the first customer to build a light theme of their own would have hit it.
        ThemeColor TrackColor{ 0, 0, 0, 255 };

        // What a label, a readout and a tick mark are drawn in. Alpha 0 means measure it against
        // whatever the plate turned out to be, which is what every theme did before this and is
        // still the right answer for most of them.
        //
        // A tube is not one of them. Its ink is the phosphor: blue-white on a monochrome set,
        // warm on an amber one, green on a terminal. A neutral white label on amber glass reads
        // as a fault rather than as a picture, which is the same thing the plate sheen proved.
        ThemeColor InkColor{ 0, 0, 0, 0 };

        // The empty part of a knob's arc, which is a different question from the empty part of a
        // fader's slot however much they look alike. A slot is a recess cut INTO the plate, so
        // it has to be darker than the plate. An arc sits OUTSIDE the plate on bare deck, and on
        // a dark theme there is nothing left out there to be darker than, so it has to be a
        // faint light instead. Take it out and a knob shows where it is without ever showing how
        // far it can go.
        //
        // Alpha 0 means use TrackColor, so a theme only names this when the two differ.
        ThemeColor ArcTrackColor{ 0, 0, 0, 0 };

        // The control plate itself. Alpha 0 means work it out from the two properties above it:
        // black at the glass tint, washed with the control's own hue where the theme is tonal.
        // Every theme but one wants that. Bigwig does not - its plate is a neutral raised gray,
        // because its whole idea is that orange only ever means "this is the value".
        ThemeColor PlateColor{ 0, 0, 0, 0 };

        // The bottom of the plate, where a theme wants one lifted at the top. Alpha 0 means a
        // flat plate, which is what every theme written before this asked for.
        ThemeColor PlateEndColor{ 0, 0, 0, 0 };

        // Bigwig.
        RimSource Rim{ RimSource::ControlHue };
        ThemeColor NeutralRimColor{ 90, 90, 90, 255 };
        ValueStripPlacement ValueStrip{ ValueStripPlacement::Bottom };
        ValueIndicatorStyle ValueIndicator{ ValueIndicatorStyle::SolidArc };

        int32_t LampCount{ 24 };

        // Below this, the lamps stop separating and the ring reads as a fine comb. A knob that
        // small falls back to the solid arc on its own rather than making somebody notice.
        int32_t MinimumLampRingSize{ 48 };

        // The scan lines, the corner fall-off and the faceplate reflection. All three are off
        // on every theme that is not a tube.
        DeckOverlay Overlay{};

        // Which slot each of a meter's three zones is drawn in. Green, amber and red by default,
        // which is what the design sheet draws.
        std::array<int32_t, MeterZoneCount> MeterSlots{ 1, 2, 5 };

        // What this theme costs, as a resource key rather than a sentence, so it is translated
        // like everything else. Empty where a theme costs nothing worth saying.
        //
        // Not every theme has to be equally accessible. Some ask for ordinary color vision, and
        // the honest thing is to measure that, say it in the picker, and leave high contrast one
        // press away - not to wash a palette out until every number clears 4.5 and the theme no
        // longer looks like the thing it was named after.
        std::wstring CautionResourceKey{};
    };

    // The ones that ship. Studio Dark first, because it is the default and the one that stays
    // readable on the densest page; the rest in alphabetical order.
    std::vector<Theme> const& BuiltInThemes() noexcept;

    // A shipped theme that has since been renamed answers to its old name as well, so a layout
    // saved before the rename still opens in the theme it was made with.
    std::wstring CurrentThemeName(_In_ std::wstring const& name);

    Theme const* FindBuiltInTheme(_In_ std::wstring const& name) noexcept;

    // ---- the colors a theme leaves for the renderer to work out ----
    //
    // Each of these is "the theme named it, or here is what it means when it did not". They live
    // here rather than in the renderer so the theme editor shows exactly what will be drawn
    // instead of its own guess at it.

    // The empty part of a knob's arc.
    ThemeColor EffectiveArcTrackColor(_In_ Theme const& theme) noexcept;

    // What the plate sheen is made of, before the theme's own percentage is applied to it.
    ThemeColor EffectivePlateSheenColor(_In_ Theme const& theme) noexcept;

    // Where the deck falls off to at the corners.
    ThemeColor EffectiveVignetteColor(_In_ Theme const& theme) noexcept;

    // The room reflected in the faceplate.
    ThemeColor EffectiveFaceplateColor(_In_ Theme const& theme) noexcept;

    // The speckle laid over the deck.
    ThemeColor EffectiveGrainColor(_In_ Theme const& theme) noexcept;

    // The natural keys and the sharps and flats of a piano keyboard.
    ThemeColor EffectiveKeyWhiteColor(_In_ Theme const& theme) noexcept;
    ThemeColor EffectiveKeyBlackColor(_In_ Theme const& theme) noexcept;

    // The deck's own color at a height down the page, 0 at the top and 1 at the bottom. A
    // notch cut into a panel's frame is filled with this, so the frame looks cut rather than
    // painted over.
    ThemeColor DeckColorAt(_In_ Theme const& theme, _In_ double fraction) noexcept;

    // How much of a control's own hue sits under it at rest. Not the same question for a switch
    // as for a knob: a panel can have solid tabs and bare knobs on the same page.
    double FillAtRestFor(_In_ Theme const& theme, _In_ bool isSwitch) noexcept;

    // Whether this theme has one.
    bool HasNeutralColor(_In_ Theme const& theme) noexcept;

    // How hard a section sits above the deck, once "the same as the plate" is worked out.
    int32_t EffectivePanelElevation(_In_ Theme const& theme) noexcept;

    // Whether this theme lights up in a color of its own rather than in each control's hue.
    bool HasNamedBloomColor(_In_ Theme const& theme) noexcept;

    // The named bloom color, or the one Light asks for. The control's own hue is not a color
    // this can return, so ResolveControlColors asks HasNamedBloomColor first.
    ThemeColor NamedBloomColor(_In_ Theme const& theme) noexcept;

    // ---- contrast, measured rather than guessed ----

    // Relative luminance, per WCAG. Exposed because the contrast figure is meaningless without it
    // and a test that only checks the ratio cannot tell which side is wrong.
    double RelativeLuminance(_In_ ThemeColor const& color) noexcept;

    // WCAG contrast ratio, 1.0 to 21.0.
    double ContrastRatio(_In_ ThemeColor const& first, _In_ ThemeColor const& second) noexcept;

    // The bar a hue slot has to clear against the deck to be legible on a stage. A control rim is
    // a thin line and a value pipe is a thin bar, so this is the large-text threshold rather than
    // the body-text one; below it, a slot is called out with what to do about it.
    constexpr double MinimumSlotContrast = 3.0;

    struct SlotContrast
    {
        int32_t SlotIndex{ 0 };
        double Ratio{ 0.0 };
        bool MeetsMinimum{ false };
    };

    // Every slot measured against the deck. An image deck cannot be measured, so it reports
    // against the deck color, which is what the image is laid over.
    std::vector<SlotContrast> MeasureContrast(_In_ Theme const& theme) noexcept;
}
