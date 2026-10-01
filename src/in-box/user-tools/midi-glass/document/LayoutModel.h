// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML. The whole document layer compiles into the unit test
// project unchanged, so it can be tested without a window and without a device.

#include <sal.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "EndpointMatch.h"
#include "ThemeModel.h"

namespace glass
{
    // A layout is a document. Nothing in this file touches XAML or a MIDI connection, so the
    // whole document layer can be tested without a window and without a device.

    constexpr uint32_t LayoutFileVersion = 1;

    constexpr int32_t AllGroups = -1;
    constexpr int32_t MaximumGroupCount = midiapp::MaximumGroupCount;

    // Untrusted input guards. A layout can arrive from a stranger, so everything read back is
    // bounded before it reaches the rest of the app.
    constexpr size_t MaximumLayoutFileBytes = 16 * 1024 * 1024;
    constexpr size_t MaximumStringLength = midiapp::MaximumStringLength;
    constexpr size_t MaximumPagesPerLayout = 64;
    constexpr size_t MaximumControlsPerPage = 1024;
    constexpr size_t MaximumMessagesPerControl = 32;
    constexpr size_t MaximumDevicesPerLayout = 32;
    constexpr size_t MaximumSequencesPerLayout = 256;
    constexpr size_t MaximumStepsPerSequence = 512;
    constexpr size_t MaximumSystemExclusiveBytes = 512 * 1024;

    // Six hue slots, and the literal escape hatch. A control stores a slot, so retheming is a
    // six color operation rather than a redesign.
    constexpr int32_t HueSlotCount = 6;
    constexpr int32_t LiteralHue = -1;

    // The theme's one un-hued color. A control set to this is deliberately NOT color coded, and
    // saying that with a slot rather than with a literal is what keeps it that way through a
    // theme swap. A theme with no neutral falls the control back to its first hue.
    constexpr int32_t NeutralSlot = HueSlotCount;

    constexpr bool IsSlotInRange(_In_ int32_t slot) noexcept
    {
        return slot == LiteralHue || slot == NeutralSlot || (slot >= 0 && slot < HueSlotCount);
    }

    // Keys this build did not understand, kept so an older build can open a newer file and write
    // it back without quietly throwing away the parts it could not edit.
    using UnknownFields = winrt::Windows::Data::Json::JsonObject;

    enum class ControlKind
    {
        Knob = 0,
        Fader = 1,
        Pad = 2,
        Button = 3,
        Toggle = 4,
        XYPad = 5,

        // 6 was an encoder, which drew and behaved exactly like a knob. A file that says encoder
        // opens as a knob.

        Meter = 7,
        Lamp = 8,
        Readout = 9,
        Label = 10,
        Image = 11,
        PageTab = 12,

        // A plate or an outline with nothing behind it, for putting a frame around the controls
        // that belong together. It sends nothing and takes no input; it is there so a page of
        // sixty controls reads as six groups of ten.
        Panel = 13,

        // Two axes on a round field, with the puck springing back to the middle when the finger
        // comes off if the customer asks for that. Same idea as the XY pad; the shape is the
        // difference, and a round field is what tells somebody it recenters.
        Joystick = 14,

        // A strip with nothing riding it. The value follows wherever the finger is and the light
        // follows the finger, so there is no cap to hunt for and no groove to aim at.
        Ribbon = 15,

        // Keys. One control rather than sixty, because a piano is a piano and nobody wants to
        // place and wire eighty-eight pads.
        PianoKeyboard = 16,

        // Generates MIDI clock at a tempo, and shows the beat while it does. The tempo can come
        // from another control on the same page, which is the whole reason it is a control and
        // not a layout setting.
        BeatClock = 17,

        // Elapsed time, counting up. Tapped, it starts again from zero. Sends nothing: it is
        // there so somebody on stage can see how long they have been playing.
        TimeDisplay = 18,

        // Sweeps a value on its own, in time with the layout's tempo, for as long as it is
        // running. A tremolo, a filter sweep or a slow drift is a thing a hand cannot do and
        // keep doing, which is the whole reason it is a control and not a gesture.
        Lfo = 19,

        // A platter. Pushed round with a finger, it reports how far it has been pushed from
        // where it sits, and it springs back the moment the finger comes off - which is what a
        // jog wheel on a DJ controller does and why a nudge is a nudge rather than a new
        // position.
        Turntable = 20,

        // A printed rule, across or down whichever way its rectangle is longer. It sends nothing
        // and takes no input: it divides a page the way the lines between groups of sections
        // on a hardware panel do.
        Line = 21,

        // A grid of square pads, each playing a note, colored by whether that note is in the
        // key. The pads keep their size and flow into rows as the control is resized. Every
        // finger is its own note, so a chord is a chord.
        NotePads = 22,

        // The same, with hexagons in rows that sit half a pad over from the row below. With
        // the right two intervals it is an isomorphic keyboard: a chord or a scale is the same
        // shape anywhere on it.
        HexPads = 23,

        // The wheel beside a keyboard. It turns under the thumb rather than jumping to it, and
        // springs back to the middle when the customer asks for that, which makes it a pitch
        // wheel; left where it is put, it is a modulation wheel.
        Wheel = 24,

        // A selector with two or more named positions, like the waveform switch on a synth.
        // Each position can send its own message, so it can pick a program, a mode or a value.
        Switch = 25,

        // A step sequencer: a row of notes and rests played in time with the layout's tempo
        // while it runs. Pressed, it starts or stops, the way an LFO does.
        Steps = 26,
    };

    // The shape an LFO sweeps. The first five repeat, so one cycle of them can be drawn with a
    // bead running along it; the noises do not, so there is no cycle to draw and the bead only
    // moves up and down.
    enum class LfoWave
    {
        Sine = 0,
        Triangle = 1,
        Square = 2,

        // A sawtooth. Up is the one that climbs and falls off a cliff; down is the reverse.
        RampUp = 3,
        RampDown = 4,

        // Flat spectrum. Every sample is as likely to be anything as any other.
        WhiteNoise = 5,

        // Falls away three decibels an octave. Sounds and reads as more natural than white.
        PinkNoise = 6,

        // Six decibels an octave: a random walk, so it wanders rather than jumps. Also called
        // red noise, which is the same thing under a different name rather than a sixth shape.
        BrownNoise = 7,

        // The mirror of pink. Rises three decibels an octave, so it is all jitter and no drift.
        BlueNoise = 8,
    };

    // When a control sends. A control has a list of messages, not one, so a single button can
    // send a note on press and a control change to a different device on release.
    enum class MessageTrigger
    {
        TurnsOn = 0,
        TurnsOff = 1,
        Changes = 2,
        Touched = 3,
        Released = 4,
    };

    enum class MessageKind
    {
        Note = 0,
        ControlChange = 1,
        ProgramChange = 2,
        PitchBend = 3,
        ChannelPressure = 4,
        PerNoteController = 5,
        RegisteredController = 6,
        AssignedController = 7,
        SystemExclusive = 8,
        RawUmp = 9,
        Sequence = 10,
        GoToPage = 11,
        HoldLayer = 12,

        // A named Mackie Control function, such as Play or Fader 3. Number holds which one.
        MackieControl = 13,
    };

    // How the layout talks to one device. It decides what a row sent there can say.
    enum class DeviceProtocol
    {
        // Values are percentages at MIDI 2.0 resolution. Windows converts for a MIDI 1.0 device.
        Midi2 = 0,

        // Values are the exact 7 and 14 bit numbers MIDI 1.0 manuals print.
        Midi1 = 1,

        // A DAW set up for a Mackie Control surface. Rows name functions rather than messages.
        MackieControl = 2,
    };

    // A page of faders that fights the DAW is worse than no page at all.
    enum class PickupMode
    {
        Jump = 0,
        Catch = 1,
        Relative = 2,
    };

    // How one control is drawn, where it disagrees with its theme.
    //
    // A theme decides how a surface looks, and almost every control should leave this alone -
    // that is the whole point of having themes. But one control sometimes has to stand apart: a
    // panic button that must not be a plate like everything around it, or a label that should
    // carry no chrome at all. UseTheme is the default and stays out of the file.
    enum class ControlStyleOverride
    {
        UseTheme = 0,

        // Smoked glass plate, hairline rim. What the themes draw.
        Plate = 1,

        // The rim only, no plate behind it.
        Outline = 2,

        // Filled in the control's own hue.
        Solid = 3,

        // No plate, no rim. Only the value and the label.
        Bare = 4,
    };

    // Where a control's label sits. The theme picks a default and one control can disagree.
    //
    // The two vertical placements exist for a wall of narrow faders, where the only room left is
    // beside the control. They read the way the words on a mixer's channel strip do: up the left
    // side, down the right.
    enum class LabelPlacementOverride
    {
        UseTheme = 0,
        Inside = 1,
        Below = 2,
        None = 3,
        Above = 4,
        InsideTop = 5,
        InsideCenter = 6,
        InsideBottom = 7,
        VerticalLeft = 8,
        VerticalRight = 9,

        // Wherever the customer dragged the label's own handles to. Set by the canvas rather
        // than chosen from the list, and paired with the box in LabelStyle.
        Custom = 10,

        // Inside, at the top left, the way a keyboard prints the legend on a key.
        InsideTopLeft = 11,
    };

    enum class ShowValueOverride
    {
        UseTheme = 0,
        Always = 1,
        WhileTouched = 2,
        Never = 3,
    };

    // How the numbers on a message row are meant to be read.
    //
    // Most of the time a value is a position and a percentage is the right way to hold it, because
    // it then lands correctly on any device whatever the wire can carry. But device documentation
    // does not talk in percentages. The APC40 Mk2 manual says a clip LED is a note on where
    // velocity 5 is red and 21 is green, on channel 0 for a solid color or channel 9 to pulse.
    // A customer copying that table has to be able to type 5, and to see 5 afterwards.
    enum class ValueScaling
    {
        // 0 to 1 of full scale, folded to whatever the destination field can carry.
        Fraction = 0,

        // The exact number the documentation gave, written into the field as it stands. Clamped
        // to the field, never scaled, so it is as true of a 16 bit MIDI 2.0 velocity as of a
        // 7 bit MIDI 1.0 one.
        Absolute = 1,
    };

    // One end of a message's range. Either end can be a percentage or an exact number, and they
    // do not have to agree.
    struct MessageValue
    {
        double Value{ 0.0 };
        ValueScaling Scaling{ ValueScaling::Fraction };
    };

    // Whether a continuous control has stops, and where they are.
    enum class DetentMode
    {
        // Smooth all the way. The default, and what a volume fader wants.
        Continuous = 0,

        // Evenly spaced, by a step in the same units as the ends: 0 to 100 % in steps of 10 %,
        // or 27 to 127 in steps of 5.
        EvenSteps = 1,

        // An arbitrary list: stops at 10, 17, 38, 39, 40 and 57.
        ExplicitValues = 2,
    };

    constexpr size_t MaximumDetentStops = 512;

    struct MessageDetents
    {
        DetentMode Mode{ DetentMode::Continuous };

        // Units for Step and for every entry in Stops.
        ValueScaling Scaling{ ValueScaling::Fraction };

        double Step{ 0.0 };

        // Each stop gets an equal share of the travel, rather than sitting where its value falls
        // between the ends. That is the only way a list like 10, 17, 38, 39, 40, 57 is usable:
        // spaced by value, the three in the middle would be a fortieth of the travel apart and
        // nobody could pick one on purpose.
        std::vector<double> Stops{};
    };

    // Which way a two axis control's message is driven. Most controls have one value and leave
    // this alone; an XY pad and a joystick have two, so each message has to say which one it
    // follows.
    enum class ValueAxis
    {
        // Left to right on a two axis control, and the only value everything else has.
        X = 0,

        // Bottom to top. Screen coordinates run the other way, and every hardware joystick and
        // every plug-in treats up as more, so the surface flips it rather than the customer.
        Y = 1,
    };

    // The marks across a control's travel, so a fader has somewhere to be other than the two
    // ends. A pad and a joystick draw the same numbers as a grid.
    struct TickMarks
    {
        bool Show{ true };

        // Marks, not gaps. Five means both ends and three between them.
        int32_t Count{ 5 };

        UnknownFields Unknown{ nullptr };
    };

    constexpr int32_t MinimumTickCount = 2;
    constexpr int32_t MaximumTickCount = 64;

    // Which way a finger moves to turn a knob up. A circle is hard to trace on a small knob, so
    // most plug-ins drag in a straight line; which line, or a circle after all, is a preference.
    enum class DragAxis
    {
        // Up is more. The default, and what a plug-in does.
        Vertical = 0,

        // Right is more, for a row of knobs under a narrow strip of screen.
        Horizontal = 1,

        // Round and round, like the real thing: clockwise is more, and the knob turns from where
        // it is rather than jumping to the finger.
        Circular = 2,
    };

    // How a picture fills the rectangle it is drawn into, whether that is the whole page or one
    // control.
    enum class BackgroundFit
    {
        // The picture at its own size, in the middle.
        Centered = 0,

        // As large as fits without changing its shape. Nothing is cut off, and there may be
        // empty space either side of it.
        Uniform = 1,

        // Filled corner to corner, changing the picture's shape to do it.
        Stretch = 2,

        // Repeated at its own size from the top left.
        Tiled = 3,

        // Filled corner to corner, keeping its shape, so whatever does not fit is cut off.
        // This is the one to use with Zoom and Center below.
        Fill = 4,
    };

    // A picture or a video shown by an image control, or filling a grouping panel. Stored as a
    // bare file name resolved against the layout's own folder, so the two travel together and a
    // layout from a stranger cannot point this app at a file somewhere else on the PC.
    struct Picture
    {
        std::wstring FileName{};
        BackgroundFit Fit{ BackgroundFit::Uniform };
        double Opacity{ 1.0 };

        // A video plays on a loop unless it is asked not to. A one shot clip on a control
        // surface is a control that looks broken four seconds in.
        bool Loops{ true };

        // How much larger than the fit size to draw it. Anything past the edges of the control
        // is cut off, which is how a tall slice is taken out of a wide clip.
        double Zoom{ 1.0 };

        // Which point of the picture lands in the middle of the control, from 0 at the left or
        // top to 1 at the right or bottom. Half and half is the middle of the picture. Where the
        // picture is smaller than the control, the same number is where it sits: 0 against the
        // left or top edge, 1 against the right or bottom.
        double CenterX{ 0.5 };
        double CenterY{ 0.5 };

        // A wash of color laid over the top. Empty for none. Black dims, which is usually what
        // a picture behind a page of controls needs before the controls can be read over it;
        // anything else tints.
        std::wstring TintColor{};
        double TintStrength{ 0.0 };

        // The part of a video that plays, in seconds from the start of the file. A stop of zero
        // means the end of the file. A loop runs from the start to the stop and round again, so
        // a few seconds can be taken out of a longer clip without cutting the file.
        double VideoStartSeconds{ 0.0 };
        double VideoEndSeconds{ 0.0 };

        // A video plays as soon as its page is running. Off, it waits on the first frame of the
        // part that plays until somebody clicks it or drags its bar.
        bool AutoPlays{ true };

        // A click on a running video stops it, and the next click starts it again. Image controls
        // only: a panel's fill sits behind the controls on the panel.
        bool ClickToPlay{ false };

        // A bar along the bottom of the video to drag through the part that plays. It spans only
        // the part of the control the video covers, not the empty space around a fitted clip.
        // Image controls only, for the same reason.
        bool ShowsScrubber{ false };

        // Nothing at all to draw.
        bool IsEmpty() const noexcept { return FileName.empty(); }

        UnknownFields Unknown{ nullptr };
    };

    constexpr double MinimumPictureZoom = 1.0;
    constexpr double MaximumPictureZoom = 8.0;

    // The longest time a start or stop point can name. A day, which no clip on a control surface
    // gets near.
    constexpr double MaximumVideoSeconds = 86400.0;

    // The shortest part of a video that plays. Anything shorter is a stutter, not a loop.
    constexpr double MinimumVideoPlaySeconds = 0.1;

    // The part of a video that plays, in seconds from the start of the file.
    struct VideoRange
    {
        double StartSeconds{ 0.0 };

        // Zero while the length of the file is not known and no stop point was set.
        double EndSeconds{ 0.0 };

        double Length() const noexcept { return EndSeconds > StartSeconds ? EndSeconds - StartSeconds : 0.0; }
    };

    // Works out the part that plays from the start and stop points and the length of the file.
    // A length of zero means the file has not been opened yet. A stop of zero, past the end of the
    // file or before the start is the end of the file, and a start past the end of the file is
    // the beginning, because the file must have been swapped for a shorter one.
    VideoRange VideoPlayRange(_In_ Picture const& picture, _In_ double durationSeconds) noexcept;

    // Where a time sits along that part, from 0 at the start to 1 at the stop, and back again.
    double VideoRangeFraction(_In_ VideoRange const& range, _In_ double seconds) noexcept;
    double VideoRangeSeconds(_In_ VideoRange const& range, _In_ double fraction) noexcept;

    // A time in a video the way a player shows one, to the tenth: 0:03.2, 1:05.0, 1:02:03.4.
    std::wstring FormatVideoTime(_In_ double seconds);

    // Where a picture ends up inside the control that shows it, in the control's own pixels.
    // Anything outside the control is cut off by the caller.
    struct PictureRect
    {
        double X{ 0.0 };
        double Y{ 0.0 };
        double Width{ 0.0 };
        double Height{ 0.0 };
    };

    // Works out that rectangle from the fit, the zoom and the point of the picture the customer
    // wants in the middle. A natural width or height of zero means the file has not been
    // decoded yet, and the answer is simply the whole control.
    PictureRect PictureCropRect(
        _In_ Picture const& picture,
        _In_ double controlWidth,
        _In_ double controlHeight,
        _In_ double naturalWidth,
        _In_ double naturalHeight) noexcept;

    // The part of the control a picture actually covers: its crop rectangle cut to the control's
    // edges. A video's bar is drawn across this, so it sits on the video and not on the empty
    // space beside a fitted clip. Empty when the two do not overlap.
    PictureRect VisiblePictureRect(
        _In_ PictureRect const& content,
        _In_ double controlWidth,
        _In_ double controlHeight) noexcept;

    // The keys on a piano keyboard control. Width and height come from the control's own
    // rectangle; this is only what is drawn inside it.
    struct KeyboardSpec
    {
        // White and black together, counted the way a keyboard is sold: 25, 49, 61, 88.
        int32_t KeyCount{ 25 };

        // The note the leftmost key plays. 48 is C3 in the naming this app uses everywhere else.
        int32_t LowestNote{ 48 };

        // Empty means the theme decides. A keyboard is the one control where the two colors are
        // the whole point, so they are named here rather than derived from a hue.
        std::wstring WhiteKeyColor{};
        std::wstring BlackKeyColor{};

        // The key under the finger. Empty means the control's own hue.
        std::wstring PressedKeyColor{};

        // The C keys carry their octave number, so a wide keyboard can be read at a glance.
        bool ShowNoteNames{ false };

        // Harder on a touch screen than on a keyboard, and not every layout wants it.
        bool VelocityFromKeyPosition{ false };

        UnknownFields Unknown{ nullptr };
    };

    constexpr int32_t MinimumKeyboardKeys = 5;
    constexpr int32_t MaximumKeyboardKeys = 128;

    // The clock generator. Tempo is either a number typed here or whatever another control on
    // the layout is sitting at, which is what makes a tempo knob a tempo knob.
    struct ClockSpec
    {
        double BeatsPerMinute{ 120.0 };

        // The id of a control on this layout whose value sets the tempo. Empty means the number
        // above is used as it stands.
        std::wstring TempoControlId{};

        // What that control's ends mean, since a fader's value is a position rather than a
        // tempo.
        double LowestBeatsPerMinute{ 40.0 };
        double HighestBeatsPerMinute{ 240.0 };

        // Running the moment the layout opens, rather than waiting to be pressed.
        bool StartsRunning{ false };

        // Sends start and stop around the clock, so a drum machine follows rather than only
        // keeping time.
        bool SendsTransport{ true };

        UnknownFields Unknown{ nullptr };
    };

    constexpr double MinimumBeatsPerMinute = 20.0;
    constexpr double MaximumBeatsPerMinute = 300.0;

    // The sweep an LFO control runs. The shape and how long one pass of it takes; where it
    // sends and what the two ends mean is the control's ordinary message list, exactly as it is
    // for a fader, so an LFO can drive anything a hand could.
    struct LfoSpec
    {
        LfoWave Wave{ LfoWave::Sine };

        // How long one pass takes, in quarter notes of the layout's tempo. Four is one bar.
        // Musical rather than in seconds because everything else on stage is.
        double BeatsPerCycle{ 4.0 };

        // The two ends of the sweep, along the control's own travel. The middle of the wave
        // lands halfway between them, so a sweep set 0.25 to 0.75 rests where a fader at half
        // would. Lowest above highest turns the wave upside down, which is a real thing to want.
        double Lowest{ 0.0 };
        double Highest{ 1.0 };

        // How often a value is taken off the wave and sent. A sweep is not audio: past about
        // forty a second nobody can hear the difference and a DIN cable certainly cannot carry
        // it, so the default leaves room on the wire for everything else the layout is doing.
        int32_t UpdateIntervalMilliseconds{ 25 };

        // Press to start and press again to stop, rather than running only while held.
        bool Latching{ true };

        // Running the moment the layout opens.
        bool StartsRunning{ false };

        // Where the value goes when it stops. A tremolo left parked at the bottom of its sweep
        // is a muted channel, so stopping puts the control back where its own default says.
        bool ReturnsToRestWhenStopped{ true };

        UnknownFields Unknown{ nullptr };
    };

    // One pass may not be so slow that nobody can tell it is running, nor so fast that the
    // update rate is doing all the shaping.
    constexpr double MinimumBeatsPerCycle = 0.0625;
    constexpr double MaximumBeatsPerCycle = 64.0;

    constexpr int32_t MinimumLfoIntervalMilliseconds = 5;
    constexpr int32_t MaximumLfoIntervalMilliseconds = 1000;

    // The platter. There is only one thing to set on it, because everything else a jog wheel
    // does already falls out of the ordinary message rows: the control's value is how far it
    // has been pushed, with the middle meaning "not moving", so a pitch bend row nudges the
    // pitch and a controller row set 0 to 127 sits at 64 at rest, which is the convention DJ
    // software reads a jog wheel with.
    struct TurntableSpec
    {
        // How far the platter has to be turned to drive the control from one end to the other.
        // Smaller is twitchier. Half a turn is about right for a nudge and for a scratch.
        double DegreesForFullRange{ 180.0 };

        // Draw the ridges around the edge, the way a slipmat or a jog wheel has them, so it is
        // obvious the thing turns.
        bool ShowsGrip{ true };

        UnknownFields Unknown{ nullptr };
    };

    constexpr double MinimumTurntableDegrees = 15.0;
    constexpr double MaximumTurntableDegrees = 1440.0;

    // How a line's two ends finish.
    enum class LineEnds
    {
        // Whatever the theme's rules do.
        UseTheme = 0,
        Square = 1,
        Faded = 2,
    };

    // A line runs the long way across its own rectangle. The rectangle is what a mouse or a
    // finger grabs in the editor, so the line can be one pixel while its handle is eight.
    struct LineSpec
    {
        double Thickness{ 1.0 };

        // Empty means the theme's rule color.
        std::wstring Color{};

        LineEnds Ends{ LineEnds::UseTheme };

        UnknownFields Unknown{ nullptr };
    };

    constexpr double MinimumLineThickness = 1.0;
    constexpr double MaximumLineThickness = 64.0;

    // The positions of a switch, in order, by the name printed on each. The first position is
    // the value 0 and the last is 1, so a message row that follows the value still works.
    struct SwitchSpec
    {
        std::vector<std::wstring> Positions{};

        UnknownFields Unknown{ nullptr };
    };

    constexpr int32_t MinimumSwitchPositions = 2;
    constexpr int32_t MaximumSwitchPositions = 16;

    // Which way a step sequencer walks its steps.
    enum class StepDirection
    {
        Forward = 0,
        Backward = 1,

        // To the last step and back again, without playing either end twice in a row.
        PingPong = 2,

        // Any step, every time. The same step can come up twice running.
        Random = 3,
    };

    // One step of a step sequencer: a note, or a rest.
    struct SequencerStep
    {
        // A step that is off is a rest. It still takes its turn; it just plays nothing.
        bool On{ true };

        int32_t Note{ 60 };

        // How hard, from 0 to 1 of the note row's own range.
        double Velocity{ 0.8 };

        UnknownFields Unknown{ nullptr };
    };

    // A step sequencer: a row of steps, each a note or a rest, played in time with the layout's
    // tempo while it runs. Where the notes go is the control's ordinary note row, so it plays on
    // whatever device and channel a keyboard pointed at the same row would.
    struct StepsSpec
    {
        std::vector<SequencerStep> Pattern{};

        // How many steps fit in a beat. Four is sixteenth notes; three is eighth note triplets.
        double StepsPerBeat{ 4.0 };

        // How much of its step each note sounds for. A little is staccato; all of it is legato.
        double Gate{ 0.5 };

        // How late every second step lands, the way drum machines count it: 0.5 is straight,
        // two thirds is a triplet shuffle, and 0.75 is as far as it goes.
        double Swing{ 0.5 };

        StepDirection Direction{ StepDirection::Forward };

        // Press to start and press again to stop, rather than running only while held.
        bool Latching{ true };

        // Running the moment the layout opens.
        bool StartsRunning{ false };

        UnknownFields Unknown{ nullptr };
    };

    constexpr int32_t MinimumSequencerSteps = 1;
    constexpr int32_t MaximumSequencerSteps = 64;
    constexpr int32_t DefaultSequencerSteps = 8;

    constexpr double MinimumStepsPerBeat = 0.25;
    constexpr double MaximumStepsPerBeat = 8.0;

    constexpr double MinimumStepGate = 0.05;
    constexpr double MaximumStepGate = 1.0;

    constexpr double MinimumStepSwing = 0.5;
    constexpr double MaximumStepSwing = 0.75;

    // Which notes a key holds, counted up from its root.
    enum class MusicalScale
    {
        Major = 0,
        Minor = 1,
        HarmonicMinor = 2,
        MelodicMinor = 3,
        Dorian = 4,
        Phrygian = 5,
        Lydian = 6,
        Mixolydian = 7,
        Locrian = 8,
        MajorPentatonic = 9,
        MinorPentatonic = 10,
        Blues = 11,
        WholeTone = 12,
    };

    // Where the note's name is printed on each pad.
    enum class PadNoteNames
    {
        Hidden = 0,
        Center = 1,
        Top = 2,
        Bottom = 3,
        TopLeft = 4,
        TopRight = 5,
        BottomLeft = 6,
        BottomRight = 7,
    };

    // What a finger sliding from one pad onto the next does.
    enum class PadGlide
    {
        // The first note ends and the next one starts, the way a finger dragged along a
        // keyboard plays each key it crosses.
        Off = 0,

        // The next note starts before the last one ends, with a Portamento Control message
        // (control change 84) naming the note it came from. A synth that follows it glides
        // between the two at its own portamento time, and a mono synth set to legato glides
        // without it.
        Portamento = 1,

        // The note that was struck keeps sounding and bends to follow the finger, using MIDI
        // 2.0 per-note pitch bend, so each finger in a chord bends on its own. A MIDI 1.0
        // instrument only hears the note that was struck.
        PerNoteBend = 2,
    };

    // No key: every pad is the same color.
    constexpr int32_t NoKey = -1;

    // The pads on a note pad or hex pad control. Width and height come from the control's own
    // rectangle; this is what is drawn inside it and what each pad plays.
    struct PadGridSpec
    {
        // How many pads there are. They flow into as many rows as the control's width needs.
        int32_t PadCount{ 24 };

        // How wide one pad is, in page units. The pads shrink to fit only when the control is
        // too small to hold them all at this size.
        double PadSize{ 48.0 };

        // The note the bottom left pad plays. 48 is C3 in the naming this app uses everywhere
        // else.
        int32_t StartNote{ 48 };

        // Semitones from one pad to the next one on its right.
        int32_t RightInterval{ 1 };

        // Semitones from one pad to the one above it on square pads, or to the one above and to
        // the right on hexagons. Zero on square pads means each row carries on from the end of
        // the row below, so no note appears twice.
        int32_t RowInterval{ 5 };

        // The key's root, 0 for C up to 11 for B, or NoKey.
        int32_t KeyRoot{ 0 };
        MusicalScale Scale{ MusicalScale::Major };

        PadNoteNames NoteNames{ PadNoteNames::Center };

        // Zero means the name is sized to the pad.
        double NoteNameSize{ 0.0 };

        // Empty means the theme decides. The root is the one pad that has to be found without
        // looking for it, so it gets a color of its own.
        std::wstring RootColor{};
        std::wstring InKeyColor{};
        std::wstring OutOfKeyColor{};
        std::wstring PressedColor{};

        PadGlide Glide{ PadGlide::Off };

        // How far a per-note pitch bend reaches either way, in semitones. Sent to the
        // instrument ahead of every note, so the bend lands on the pitch under the finger.
        int32_t BendRangeSemitones{ 48 };

        UnknownFields Unknown{ nullptr };
    };

    constexpr int32_t MinimumPadCount = 1;
    constexpr int32_t MaximumPadCount = 128;
    constexpr double MinimumPadSize = 12.0;
    constexpr double MaximumPadSize = 240.0;
    constexpr int32_t MinimumRightInterval = 1;
    constexpr int32_t MinimumRowInterval = 0;
    constexpr int32_t MaximumPadInterval = 24;
    constexpr double MaximumPadNoteNameSize = 64.0;
    constexpr int32_t MinimumBendRangeSemitones = 1;
    constexpr int32_t MaximumBendRangeSemitones = 96;

    // What lights a lamp or moves a meter. A meter following one controller is the ordinary
    // case; a lamp is more often "is anything coming from this device at all".
    enum class FeedbackMode
    {
        // One message: this controller, on this channel, from this device. The only mode that
        // carries a value, so it is the only one that can move a fader or a meter.
        Message = 0,

        // Any message at all from the device, optionally narrowed to a group and a channel.
        // This is what somebody means by an activity light.
        AnyActivity = 1,

        // The beat. Either incoming MIDI clock, counted twenty four to the quarter note, or a
        // clock generator control on this layout.
        Tempo = 2,

        // Any note on, whatever the note is. What a keyboard activity light wants.
        Notes = 3,

        // Any control change, whatever the controller is.
        ControlChanges = 4,

        // Lit from start until stop, rather than blinking. The one mode that latches, because
        // "is the sequencer running" is a state and not an event.
        Transport = 5,
    };

    // How many clock messages make a quarter note. Fixed by MIDI since 1983.
    constexpr int32_t ClockTicksPerQuarterNote = 24;

    enum class ScaleMode
    {
        ActualSize = 0,
        FitToScreen = 1,
        Custom = 2,
    };

    enum class ScreenCorner
    {
        TopLeft = 0,
        TopRight = 1,
        BottomLeft = 2,
        BottomRight = 3,
    };

    enum class TempoSourceKind
    {
        Internal = 0,
        FollowIncomingClock = 1,
    };

    enum class SequenceStepKind
    {
        // Not SendMessage: windows.h defines that as a macro, and every file that uses this enum
        // also includes windows.h.
        SendMidiMessage = 0,
        SendSystemExclusive = 1,
        Wait = 2,
        SetControlValue = 3,
        GoToPage = 4,
        HoldLayer = 5,
        RepeatBlockStart = 6,
        RepeatBlockEnd = 7,
    };

    // Everything about how a label is drawn that is not where it sits. Every field defaults to
    // "the theme decides", so a layout that has not been fiddled with carries none of it and a
    // theme change still reaches every control.
    struct LabelStyle
    {
        // Empty means the theme's font.
        std::wstring FontFamily{};

        // 0 means the theme's size.
        double FontSize{ 0.0 };

        // 0 means the theme's weight. Otherwise a normal OpenType weight: 400, 600, 700.
        int32_t FontWeight{ 0 };

        bool Italic{ false };
        bool Underline{ false };

        // Empty means the theme's label color.
        std::wstring Color{};

        // Long words break to the next line instead of being cut off. On by default: a fader is
        // narrower than most of the words people put under one.
        bool Wrap{ true };

        // How wide the label is allowed to be, as a percentage of the control. Above 100 the
        // text is centered on the control and spills either side, which is what makes a readable
        // caption possible under a 40 px fader.
        double WidthPercent{ 100.0 };

        // An explicit rectangle for the label, in page units, measured from the control's own
        // top-left corner. Dragged with handles on the canvas, the same way a control is sized.
        //
        // Zero width or height means there is no explicit box and the placement rule decides,
        // which is where every control starts. Once a box is set it wins over the placement and
        // over WidthPercent: the customer has said where the text goes, so anything that does
        // not fit is trimmed with an ellipsis rather than moved.
        double BoxX{ 0.0 };
        double BoxY{ 0.0 };
        double BoxWidth{ 0.0 };
        double BoxHeight{ 0.0 };

        bool HasBox() const noexcept { return BoxWidth > 0.0 && BoxHeight > 0.0; }

        UnknownFields Unknown{ nullptr };
    };

    // The smallest a dragged label box may get. Smaller than a control's minimum, because a
    // label reading "1" beside a knob is a reasonable thing to want.
    constexpr double MinimumLabelBoxSize = 8.0;

    // How far a label box may reach from its control. A box from a stranger's file must not be
    // able to ask for a text block the size of a wall.
    constexpr double MaximumLabelBoxExtent = 8192.0;

    // One row of "what this control sends". Destination is a name from the layout's own device
    // table, never a device id, which is what makes a layout portable.
    struct ControlMessage
    {
        MessageTrigger Trigger{ MessageTrigger::Changes };
        MessageKind Kind{ MessageKind::ControlChange };

        std::wstring DeviceName{};
        int32_t GroupIndex{ 0 };
        int32_t ChannelIndex{ 0 };

        // Controller, note or bank number, depending on Kind. Ignored where it has no meaning.
        uint32_t Number{ 0 };

        // The two ends of what this message sends. A control at rest sends the minimum and a
        // control at full travel sends the maximum; anything between is interpolated.
        //
        // This is one idea rather than two, which is why there is no separate on and off pair. A
        // button only ever sits at one end or the other, so "off is 0 and on is 127" and "off is
        // 17 and on is 13005" and "0 % to 100 %" are all the same setting. A fader limited to
        // 0 to 127 falls out of the same arithmetic, quantized because the ends are whole numbers.
        //
        // A minimum above a maximum is allowed and inverts the control, which is what somebody
        // wants for a fader that reads top to bottom.
        MessageValue Minimum{ 0.0, ValueScaling::Fraction };
        MessageValue Maximum{ 1.0, ValueScaling::Fraction };

        MessageDetents Detents{};

        // Which of a two axis control's values drives this message. Ignored everywhere else.
        ValueAxis Axis{ ValueAxis::X };

        std::vector<uint8_t> SystemExclusive{};
        std::vector<uint32_t> RawWords{};

        // Send this one as MIDI 1.0 protocol rather than MIDI 2.0, so the seven bit value on the
        // wire is exactly the one the customer typed.
        //
        // The default is MIDI 2.0 and the service downscales, which is right for anything that is
        // a position. It is wrong for anything that is a code: a pad that sets its color from the
        // velocity of a note on has 127 discrete colors, and "nearly 37" is a different color.
        bool UseMidi1Protocol{ false };

        std::wstring SequenceName{};
        std::wstring TargetPageId{};
        std::wstring TargetLayerId{};

        // On a switch, the one position that sends this row, counted from 0, and it sends its
        // maximum. -1 is every change, the way a row on any other control works.
        int32_t Position{ -1 };

        UnknownFields Unknown{ nullptr };
    };

    // An RPN or NRPN is named by a bank and an index, 0 to 127 each: in MIDI 1.0, the values of
    // CC 101 and 100 or of CC 99 and 98. ControlMessage::Number holds both as bank * 128 + index.
    constexpr uint32_t MaximumControllerNumber = 16383;

    bool HasBankAndIndex(_In_ MessageKind kind) noexcept;
    uint32_t ControllerBank(_In_ uint32_t number) noexcept;
    uint32_t ControllerIndex(_In_ uint32_t number) noexcept;
    uint32_t ControllerNumber(_In_ uint32_t bank, _In_ uint32_t index) noexcept;

    // The number the way a device manual prints it: "3:17" for an RPN or NRPN, "74" otherwise.
    std::wstring FormatMessageNumber(_In_ MessageKind kind, _In_ uint32_t number);

    // Whether a row goes out to a device, and so has a device and a group. A page change and a
    // sequence stay inside the app.
    bool SendsToADevice(_In_ MessageKind kind) noexcept;

    // Whether a row carries a channel. Only the channel voice messages do.
    bool CarriesAChannel(_In_ MessageKind kind) noexcept;

    // Whether a row goes out as MIDI 1.0 protocol. An RPN, an NRPN and a per-note controller
    // have no single MIDI 1.0 message, so they go as MIDI 2.0 and Windows converts them.
    bool SendsAsMidi1(_In_ ControlMessage const& message, _In_ DeviceProtocol protocol) noexcept;

    // The highest number a row's values are typed as: 127, or 16383 for a MIDI 1.0 pitch bend,
    // RPN or NRPN. 0 means they are typed as a percentage, the way MIDI 2.0 values are.
    int32_t RawValueMaximum(_In_ ControlMessage const& message, _In_ DeviceProtocol protocol) noexcept;

    // One end of a row's range the way it is typed: a whole number, or a percentage.
    double ShownValue(
        _In_ MessageValue const& end,
        _In_ ControlMessage const& message,
        _In_ DeviceProtocol protocol) noexcept;

    // Kept as a share of the range, so it survives the device changing protocol.
    MessageValue ValueFromShown(
        _In_ double shown,
        _In_ ControlMessage const& message,
        _In_ DeviceProtocol protocol) noexcept;

    // Turns exact numbers into shares of the range, as the row sends them under this protocol.
    // Done before a device changes protocol, because an exact number is only exact in one.
    void ShareExactValues(_Inout_ ControlMessage& message, _In_ DeviceProtocol protocol) noexcept;

    // What a control listens for, so a fader can follow the DAW rather than only lead it.
    struct FeedbackBinding
    {
        bool Enabled{ false };
        FeedbackMode Mode{ FeedbackMode::Message };
        MessageKind Kind{ MessageKind::ControlChange };
        std::wstring DeviceName{};
        int32_t GroupIndex{ 0 };
        int32_t ChannelIndex{ 0 };
        uint32_t Number{ 0 };

        // Activity mode only. Off means any channel counts, which is what an activity light on
        // a whole device wants; on narrows it to the channel above.
        bool MatchesChannel{ false };

        // Tempo mode only. The id of a clock generator control on this layout to follow. Empty
        // means whatever clock is arriving from the device named above.
        std::wstring TempoControlId{};

        // How long the lamp stays lit after something arrives. Short enough to read as a blink,
        // long enough to see across a room.
        int32_t HoldMilliseconds{ 120 };

        UnknownFields Unknown{ nullptr };
    };

    struct Control
    {
        std::wstring Id{};
        ControlKind Kind{ ControlKind::Knob };
        std::wstring Label{};

        // Controls that share this select and move as one. Empty for a control on its own.
        std::wstring GroupId{};

        double X{ 0 };
        double Y{ 0 };
        double Width{ 56 };
        double Height{ 56 };

        // A slot, not a color, unless LiteralColor is set and HueSlot is LiteralHue.
        int32_t HueSlot{ 0 };
        std::wstring LiteralColor{};

        bool AspectLocked{ false };

        // Locked on the page: it cannot be picked there, moved or resized until it is unlocked.
        // Clicks go through it, which is what lets somebody work over a big panel behind
        // everything. The outline still selects it.
        bool Locked{ false };

        // Where this control disagrees with its theme. UseTheme is the default and almost every
        // control stays there, which is what makes switching theme a six color operation.
        ControlStyleOverride Style{ ControlStyleOverride::UseTheme };
        LabelPlacementOverride LabelPlaced{ LabelPlacementOverride::UseTheme };
        LabelStyle LabelLook{};
        ShowValueOverride ShowValue{ ShowValueOverride::UseTheme };

        // The order a screen reader walks, and the order a bank learn fills. Visible in the
        // editor as a badge, because a hidden ordering is one nobody can fix.
        int32_t KeyboardOrder{ 0 };

        PickupMode Pickup{ PickupMode::Jump };

        // Which way a finger drags to turn this control up. Knobs only.
        DragAxis Drag{ DragAxis::Vertical };
        // A pad hit softly sends a softer note. Pads only, and only worth turning on where the
        // hardware reports it: a mouse says the same thing every time, and a finger on a
        // screen without pressure says the same thing every time too.
        bool VelocityFromTouch{ false };
        // The marks across the travel. A grid on a two axis control, notches beside a slot on
        // a fader, and ticks around the arc on a knob.
        TickMarks Ticks{};

        // Print the number at each stop beside the marks. Off by default: it is what somebody
        // building a six position mode switch wants and what nobody building a volume fader
        // wants.
        bool ShowDetentValues{ false };

        // A picture or a video, for an image control and for a grouping panel's fill.
        Picture Image{};

        // Only read when the kind is PianoKeyboard.
        KeyboardSpec Keyboard{};

        // Only read when the kind is BeatClock.
        ClockSpec Clock{};

        // Only read when the kind is Lfo.
        LfoSpec Lfo{};

        // Only read when the kind is Turntable.
        TurntableSpec Turntable{};

        // Only read when the kind is Line.
        LineSpec Line{};

        // Only read when the kind is Switch.
        SwitchSpec Switch{};

        // Only read when the kind is Steps.
        StepsSpec Steps{};

        // Only read when the kind is NotePads or HexPads.
        PadGridSpec Pads{};

        // A layout always starts from its own defaults; this is the value it starts at.
        double DefaultValue{ 0.0 };

        // The same, for the second axis of an XY pad or a joystick. Ignored everywhere else.
        double DefaultValueY{ 0.0 };

        // Rests in the middle and lights only the part it has been moved away from the middle,
        // the way a pan knob does. Knobs and faders only.
        bool LightsFromCenter{ false };

        // Springs back to DefaultValue the moment the finger comes off. A pitch wheel does; a
        // volume fader had better not. It is a property of the control rather than of its kind
        // because a mod wheel and a pitch wheel are the same control with different answers.
        bool ReturnsToDefault{ false };

        // Sends DefaultValue when the layout opens, so a synth can be put into a known state.
        // The global override in the layout can suppress every one of these at once.
        bool SendsValueOnStart{ false };

        // The least time between two sends while this control is being moved. Zero is no limit.
        //
        // A DIN cable carries about 350 three byte messages a second, shared with everything else
        // on that wire, and a fader dragged across a high rate digitizer will out-run it. Only a
        // continuous control is ever limited; rate limiting a note on would be a defect.
        int32_t SendIntervalMilliseconds{ 0 };

        std::vector<ControlMessage> Messages{};
        FeedbackBinding Feedback{};

        UnknownFields Unknown{ nullptr };
    };

    // How many stops a control snaps to, or zero when it is smooth all the way.
    //
    // Stops live on a message rather than on the control, because one control can send two
    // things and only one of them steps. A finger has one position, so where the messages
    // disagree the widest set wins.
    int32_t DetentStopCount(_In_ Control const& control) noexcept;

    // How many positions a switch has, never fewer than two, and which one a value lands on.
    int32_t SwitchPositionCount(_In_ Control const& control) noexcept;
    int32_t SwitchPositionAt(_In_ double value, _In_ int32_t positions) noexcept;
    double SwitchValueOf(_In_ int32_t position, _In_ int32_t positions) noexcept;

    // A list of stops as somebody types it, and back again. Separators are forgiving because a
    // customer copying "10, 17, 38" out of a manual should not have to think about commas.
    // Anything that is not a number is dropped rather than turning the whole list into nothing:
    // a stray character at the end of a long list should not empty it.
    std::vector<double> ParseStopList(_In_ std::wstring const& text) noexcept;
    std::wstring FormatStopList(_In_ std::vector<double> const& stops) noexcept;

    // What to print beside each mark when a control is asked to show its stop values, bottom
    // end first. Empty when the control is smooth or has too many stops to label.
    //
    // A stop typed as an exact number is printed as itself, because somebody working from a
    // device manual wants to see the number they typed. A stop typed as a fraction is printed
    // as a percentage.
    std::vector<std::wstring> DetentStopLabels(_In_ Control const& control) noexcept;

    // More than this and the numbers run into each other whatever size the control is.
    constexpr int32_t MaximumLabeledStops = 16;

    // The page a page tab goes to: the page named by its first go-to-page row. Empty when it has
    // none.
    std::wstring PageTabTarget(_In_ Control const& control);

    // Whether a label may name this font. A family name never holds a path, a link or a list of
    // families, and a layout can come from a stranger, so a name that does is read as no font.
    bool IsSafeFontFamilyName(_In_ std::wstring_view name) noexcept;

    // The smallest side a page can have. Small enough for a strip of buttons used as a toolbar.
    constexpr int32_t MinimumPageSide = 32;
    constexpr int32_t MaximumPageSide = 8192;

    // What a group is called. Which controls are in it is on the controls themselves; this is only
    // the name, so a page of channel strips reads as Drums and Bass rather than Group 1 and 2. A
    // group nobody named has no entry.
    struct ControlGroup
    {
        std::wstring Id{};
        std::wstring Name{};

        UnknownFields Unknown{ nullptr };
    };

    struct Page
    {
        std::wstring Id{};
        std::wstring Name{};
        int32_t HueSlot{ 0 };

        // Pinned to every page, for transport and panic, so they are not rebuilt four times.
        bool IsSharedBand{ false };

        std::vector<Control> Controls{};

        // Names for the groups on this page, by the group id the controls carry.
        std::vector<ControlGroup> Groups{};

        UnknownFields Unknown{ nullptr };

        ControlGroup* FindGroup(_In_ std::wstring const& id) noexcept;
        ControlGroup const* FindGroup(_In_ std::wstring const& id) const noexcept;
    };

    // An entry in the layout's own device table. The criteria are the same ones the service
    // configuration and MIDI Patchbay use, so a layout built on one PC has a real chance of
    // finding the right hardware on another.
    struct DeviceEntry
    {
        std::wstring Name{};
        midiapp::EndpointMatch Match{};
        midiapp::EndpointMatchMode MatchMode{ midiapp::EndpointMatchMode::EndpointDeviceId };

        // Clock to a device that does not want it is noise, so this is per destination.
        bool SendsBeatClock{ false };

        DeviceProtocol Protocol{ DeviceProtocol::Midi2 };

        // A protocol a newer build named. Treated as MIDI 2.0 and written back as it was.
        std::wstring UnrecognizedProtocol{};

        UnknownFields Unknown{ nullptr };
    };

    struct SequenceStep
    {
        SequenceStepKind Kind{ SequenceStepKind::SendMidiMessage };

        ControlMessage Message{};
        uint32_t WaitMilliseconds{ 0 };
        uint32_t RepeatCount{ 1 };

        // How long a note in a sequence is held before its note off. A step list that could
        // only turn notes on would be a step list that leaves a synthesizer droning, so this is
        // part of the step rather than something to remember to add afterwards.
        uint32_t DurationMilliseconds{ 200 };

        std::wstring TargetControlId{};
        double TargetValue{ 0.0 };

        // Set when the file named a step kind this build does not know. The step does nothing
        // and keeps its own name, because the alternative is a build from last year sending a
        // message a newer build meant as something else entirely.
        std::wstring UnrecognizedKind{};

        UnknownFields Unknown{ nullptr };
    };

    // What pressing the button does. A one shot patch recall and an arpeggio that runs while a
    // finger is down are the same list of steps with a different answer to this question.
    enum class SequenceRunMode
    {
        Once = 0,
        WhileHeld = 1,

        // Starts on one press and stops on the next.
        Toggle = 2,
    };

    struct Sequence
    {
        std::wstring Name{};
        SequenceRunMode Mode{ SequenceRunMode::Once };
        std::vector<SequenceStep> Steps{};

        UnknownFields Unknown{ nullptr };
    };

    struct TempoSource
    {
        TempoSourceKind Kind{ TempoSourceKind::Internal };
        double BeatsPerMinute{ 120.0 };
        std::wstring DeviceName{};

        UnknownFields Unknown{ nullptr };
    };

    struct LayoutDocument
    {
        // What wrote this file. An older build keeps a higher number as it found it.
        uint32_t FileVersion{ LayoutFileVersion };

        std::wstring Name{};
        std::wstring Description{};

        // Empty while the layout has never been written.
        std::wstring FilePath{};

        // A page has a fixed pixel size and scales letterboxed, never stretched, because a
        // control surface is muscle memory.
        int32_t PageWidth{ 1280 };
        int32_t PageHeight{ 800 };

        // The editor works on a canvas larger than the page. Controls outside the page are
        // ghosted rather than clamped, so a resize is always reversible.
        int32_t CanvasWidth{ 1280 };
        int32_t CanvasHeight{ 800 };

        std::wstring ThemeName{};

        // The theme itself, where the customer has edited it away from the one it is named
        // after. A theme travels INSIDE the layout file, so a layout sent to somebody looks the
        // way it was built even though they have never seen the theme.
        //
        // Only written when it has been edited. A layout that simply picked a shipped theme
        // keeps the name alone, so an improvement to that theme reaches it.
        bool HasOwnTheme{ false };
        Theme OwnTheme{};

        // A picture behind the controls. Stored as a bare file name, resolved against the folder
        // the layout file is in, so a layout and its artwork move together. Empty means none.
        std::wstring BackgroundImage{};
        BackgroundFit BackgroundFitMode{ BackgroundFit::Uniform };

        // How strongly it shows through. The deck is still what the controls are read against,
        // so a picture at full strength is usually the wrong answer.
        double BackgroundOpacity{ 1.0 };

        ScaleMode Scale{ ScaleMode::ActualSize };
        double CustomScalePercent{ 100.0 };
        ScreenCorner FullScreenButtonCorner{ ScreenCorner::TopRight };

        // A layout used as a toolbar. Its window is the page and nothing else: no title bar, no
        // row of buttons and no border, with a handle at one end to move it and open its menu.
        bool ToolbarWindow{ false };

        // Stays in front of other windows while it runs, the way a toolbar has to.
        bool AlwaysOnTop{ false };

        // Nothing is drawn behind the controls while it runs, neither the theme's deck nor the
        // window, so whatever is behind the window shows through.
        bool SeeThrough{ false };

        // Which display this layout was last opened on. Gone display means primary, not
        // off screen.
        std::wstring PreferredDisplayId{};

        // The one switch that stops a layout whose faders sit at zero from muting a live desk.
        bool SuppressAllStartupValues{ false };

        // Set when the file came from outside the layouts folder. The first run asks before
        // sending system exclusive, because arbitrary SysEx can damage a device.
        bool IsImported{ false };

        // Pinned to the top of the library. It travels with the file rather than living in this
        // PC's settings, because the customer who made the layout is the one who cares about it
        // and a layout carried to another machine should arrive where they left it.
        bool IsFavorite{ false };

        // Appears to other apps while the layout runs, and goes away with it. Off unless asked
        // for, and it is a virtual device rather than a loopback so nothing is left behind.
        bool PublishesVirtualDevice{ false };

        int64_t CreatedTimestamp{ 0 };
        int64_t ModifiedTimestamp{ 0 };

        TempoSource Tempo{};

        std::vector<Page> Pages{};
        std::vector<DeviceEntry> Devices{};
        std::vector<Sequence> Sequences{};

        UnknownFields Unknown{ nullptr };

        Page* FindPage(_In_ std::wstring const& id) noexcept;
        Page const* FindPage(_In_ std::wstring const& id) const noexcept;

        Control* FindControl(_In_ std::wstring const& id) noexcept;
        Control const* FindControl(_In_ std::wstring const& id) const noexcept;

        // By the index the binding engine counts with: page order, then control order within
        // the page. Null when the index is past the end.
        Control const* ControlAtIndex(_In_ size_t controlIndex) const noexcept;

        DeviceEntry const* FindDevice(_In_ std::wstring const& name) const noexcept;

        // MIDI 2.0 for a name that is not in the table.
        DeviceProtocol ProtocolOf(_In_ std::wstring const& deviceName) const noexcept;
        Sequence const* FindSequence(_In_ std::wstring const& name) const noexcept;

        size_t ControlCount() const noexcept;

        // Controls that fall outside the page rectangle. The editor ghosts these and counts them
        // rather than moving them, so shrinking a page can always be undone.
        std::vector<Control const*> ControlsOutsidePage() const noexcept;

        static std::wstring NewId() noexcept;
    };

    // Trims, bounds and strips control characters. Used on everything read from a layout file.
    std::wstring SanitizeStoredString(_In_ std::wstring value) noexcept;

    // Copies of grouped controls get groups of their own, so a copy never joins the group it
    // was copied from. Copies that were together are still together. Hands back each group that
    // is still a group among the copies, as the id it was copied from and the id it has now.
    std::vector<std::pair<std::wstring, std::wstring>> RegroupCopies(_Inout_ std::vector<Control>& copies);

    // A name for the copy of a group called this. The name itself while no group on the page has
    // it, so a cut and paste changes nothing. Otherwise the number at its end moves on to one that
    // is free, or a 2 goes on the end, so copies of Strip 1 read Strip 2 and Strip 3.
    std::wstring NameForCopiedGroup(_In_ std::wstring const& name, _In_ Page const& page);

    // Names the groups RegroupCopies made after the ones they were copied from, in order, so a
    // bank of copies is numbered along the bank. The originals are a copy because they are often
    // the page's own list, which this adds to.
    void NameCopiedGroups(
        _Inout_ Page& page,
        _In_ std::vector<ControlGroup> originals,
        _In_ std::vector<std::pair<std::wstring, std::wstring>> const& regrouped);

    // Drops the names of groups with no control left on the page, and entries that carry
    // nothing. Run after any edit that can empty a group.
    void PruneControlGroups(_Inout_ Page& page) noexcept;

    // What is wrong with a document, in the order a person would want to fix it. An empty result
    // means the document is safe to run; it never means the document is beautiful.
    struct ValidationIssue
    {
        std::wstring ObjectId{};
        std::wstring Detail{};
    };

    std::vector<ValidationIssue> Validate(_In_ LayoutDocument const& document) noexcept;

    // Which groups this layout sends on, one bit per group, per entry in the device table and in
    // the same order. Panic uses it: a panic that covers the wrong groups is silent at exactly
    // the moment it matters, so which groups a layout drives is worth deriving rather than
    // guessing, and worth a test.
    std::vector<uint16_t> CollectGroupMasks(_In_ LayoutDocument const& document) noexcept;

    // What a control or a label is printed on. A theme can ink each one differently, because a
    // panel printed in two layers is two surfaces a long way apart in value.
    enum class PrintSurface
    {
        Deck = 0,
        Section = 1,

        // A section inside another section.
        Inset = 2,
    };

    // A grouping panel that is a surface: one that fills its frame. An outline is not one,
    // because whatever is printed inside it is printed on whatever is under the frame.
    struct PanelFootprint
    {
        double X{ 0 };
        double Y{ 0 };
        double Width{ 0 };
        double Height{ 0 };

        // Its place in the page's drawing order. A panel only lies under what comes after it.
        size_t Order{ 0 };

        bool IsInset{ false };
    };

    // Whether a panel fills its frame under this theme, once its own style has had its say.
    bool PanelIsFilled(_In_ Control const& panel, _In_ Theme const& theme) noexcept;

    std::vector<PanelFootprint> PanelFootprints(_In_ Page const& page, _In_ Theme const& theme) noexcept;

    // The topmost filled panel drawn before `order` that contains the point, or the deck.
    PrintSurface SurfaceAt(
        _In_ std::vector<PanelFootprint> const& panels,
        _In_ double x,
        _In_ double y,
        _In_ size_t order) noexcept;
}
