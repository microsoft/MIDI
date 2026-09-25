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
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "EndpointMatch.h"

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

    enum class ControlKind
    {
        Knob = 0,
        Fader = 1,
        Pad = 2,
        Button = 3,
        Toggle = 4,
        XYPad = 5,
        Encoder = 6,
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

    enum class LabelPlacementOverride
    {
        UseTheme = 0,
        Inside = 1,
        Below = 2,
        None = 3,
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

    // Keys this build did not understand, kept so an older build can open a newer file and write
    // it back without quietly throwing away the parts it could not edit.
    using UnknownFields = winrt::Windows::Data::Json::JsonObject;

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

        UnknownFields Unknown{ nullptr };
    };

    // What a control listens for, so a fader can follow the DAW rather than only lead it.
    struct FeedbackBinding
    {
        bool Enabled{ false };
        MessageKind Kind{ MessageKind::ControlChange };
        std::wstring DeviceName{};
        int32_t GroupIndex{ 0 };
        int32_t ChannelIndex{ 0 };
        uint32_t Number{ 0 };

        UnknownFields Unknown{ nullptr };
    };

    struct Control
    {
        std::wstring Id{};
        ControlKind Kind{ ControlKind::Knob };
        std::wstring Label{};

        double X{ 0 };
        double Y{ 0 };
        double Width{ 56 };
        double Height{ 56 };

        // A slot, not a color, unless LiteralColor is set and HueSlot is LiteralHue.
        int32_t HueSlot{ 0 };
        std::wstring LiteralColor{};

        bool AspectLocked{ false };

        // Where this control disagrees with its theme. UseTheme is the default and almost every
        // control stays there, which is what makes switching theme a six color operation.
        ControlStyleOverride Style{ ControlStyleOverride::UseTheme };
        LabelPlacementOverride LabelPlaced{ LabelPlacementOverride::UseTheme };
        ShowValueOverride ShowValue{ ShowValueOverride::UseTheme };

        // The order a screen reader walks, and the order a bank learn fills. Visible in the
        // editor as a badge, because a hidden ordering is one nobody can fix.
        int32_t KeyboardOrder{ 0 };

        PickupMode Pickup{ PickupMode::Jump };

        // A layout always starts from its own defaults; this is the value it starts at.
        double DefaultValue{ 0.0 };

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

    struct Page
    {
        std::wstring Id{};
        std::wstring Name{};
        int32_t HueSlot{ 0 };

        // Pinned to every page, for transport and panic, so they are not rebuilt four times.
        bool IsSharedBand{ false };

        std::vector<Control> Controls{};

        UnknownFields Unknown{ nullptr };
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

        ScaleMode Scale{ ScaleMode::ActualSize };
        double CustomScalePercent{ 100.0 };
        ScreenCorner FullScreenButtonCorner{ ScreenCorner::TopRight };

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

        DeviceEntry const* FindDevice(_In_ std::wstring const& name) const noexcept;
        Sequence const* FindSequence(_In_ std::wstring const& name) const noexcept;

        size_t ControlCount() const noexcept;

        // Controls that fall outside the page rectangle. The editor ghosts these and counts them
        // rather than moving them, so shrinking a page can always be undone.
        std::vector<Control const*> ControlsOutsidePage() const noexcept;

        static std::wstring NewId() noexcept;
    };

    // Trims, bounds and strips control characters. Used on everything read from a layout file.
    std::wstring SanitizeStoredString(_In_ std::wstring value) noexcept;

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
}
