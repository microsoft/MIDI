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

        // Sent when the control turns on, and when it turns off, as a fraction of full scale.
        // Stored once at full resolution and folded down on the way out to a MIDI 1.0 device, so
        // the same file is correct on both.
        double OnValue{ 1.0 };
        double OffValue{ 0.0 };

        std::vector<uint8_t> SystemExclusive{};
        std::vector<uint32_t> RawWords{};

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

        // The order a screen reader walks, and the order a bank learn fills. Visible in the
        // editor as a badge, because a hidden ordering is one nobody can fix.
        int32_t KeyboardOrder{ 0 };

        PickupMode Pickup{ PickupMode::Jump };

        // A layout always starts from its own defaults; this is the value it starts at.
        double DefaultValue{ 0.0 };

        // Sends DefaultValue when the layout opens, so a synth can be put into a known state.
        // The global override in the layout can suppress every one of these at once.
        bool SendsValueOnStart{ false };

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

        std::wstring TargetControlId{};
        double TargetValue{ 0.0 };

        UnknownFields Unknown{ nullptr };
    };

    struct Sequence
    {
        std::wstring Name{};
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
}
