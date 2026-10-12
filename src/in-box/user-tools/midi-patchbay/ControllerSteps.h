// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The game controller steps: a button, and a stick or trigger. Pure: no XAML, no resources and no
// precompiled header, so the unit tests compile this exactly as it ships. Where the readings come
// from is ControllerHub.h; names and summaries for people are in MessageText.h.

#include "MessageTransform.h"
#include "HidControllers.h"

// Shared with MIDI Glass, in midi-app-shared. Pure.
#include "ChannelVoiceWords.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace midipatchbay
{
    // The controls of an Xbox controller as XInput reads them. The jacks on an Xbox Adaptive
    // Controller are these same controls.
    enum class ControllerControl : int32_t
    {
        A = 0,
        B = 1,
        X = 2,
        Y = 3,
        LeftBumper = 4,
        RightBumper = 5,
        View = 6,
        Menu = 7,
        LeftStickButton = 8,
        RightStickButton = 9,
        DPadUp = 10,
        DPadDown = 11,
        DPadLeft = 12,
        DPadRight = 13,
        LeftTrigger = 14,
        RightTrigger = 15,
        LeftStickX = 16,
        LeftStickY = 17,
        RightStickX = 18,
        RightStickY = 19,

        // A stick pushed past half way, as a button. A switch in an Xbox Adaptive Controller's X1
        // jack is left stick up, and one in X2 is left stick down, unless the Xbox Accessories app
        // says otherwise.
        LeftStickUp = 20,
        LeftStickDown = 21,
        LeftStickLeft = 22,
        LeftStickRight = 23,
        RightStickUp = 24,
        RightStickDown = 25,
        RightStickLeft = 26,
        RightStickRight = 27,
    };

    constexpr size_t ControllerControlCount = 28;

    // Windows numbers Xbox controllers 1 to 4 in the order they were plugged in. Counted from 0 in
    // the file and from 1 on screen.
    constexpr uint8_t ControllerPlayerCount = 4;

    // A button or a direction on the D-pad: down or up, nothing between.
    bool IsControllerButton(_In_ ControllerControl control) noexcept;

    // A trigger: rests at 0. It can be a button too, pressed past half way.
    bool IsControllerTrigger(_In_ ControllerControl control) noexcept;

    // One direction of a stick: rests in the middle.
    bool IsControllerStick(_In_ ControllerControl control) noexcept;

    // A stick pushed one way, read as a button.
    bool IsControllerStickDirection(_In_ ControllerControl control) noexcept;

    // What a button step can use: buttons, triggers and stick directions.
    bool CanBePressed(_In_ ControllerControl control) noexcept;

    // What a stick or trigger step can use: sticks and triggers.
    bool CanSlide(_In_ ControllerControl control) noexcept;

    // The name a patch file uses for the control, for example "dpadUp".
    std::wstring_view ControllerControlKey(_In_ ControllerControl control) noexcept;
    std::optional<ControllerControl> ControllerControlFromKey(_In_ std::wstring_view key) noexcept;

    // One reading of an Xbox controller, every value from 0 to 1. A stick at rest reads about half.
    struct ControllerReading
    {
        bool Connected{ false };

        // XInput's button bits.
        uint16_t Buttons{ 0 };

        double LeftTrigger{ 0.0 };
        double RightTrigger{ 0.0 };
        double LeftStickX{ 0.5 };
        double LeftStickY{ 0.5 };
        double RightStickX{ 0.5 };
        double RightStickY{ 0.5 };
    };

    // XInput's own numbers, as one reading. Up and right are toward 1.
    ControllerReading ControllerReadingFromXInput(
        _In_ uint16_t buttons,
        _In_ uint8_t leftTrigger,
        _In_ uint8_t rightTrigger,
        _In_ int16_t leftStickX,
        _In_ int16_t leftStickY,
        _In_ int16_t rightStickX,
        _In_ int16_t rightStickY) noexcept;

    // 0 to 1. A button or a stick direction is 0 or 1.
    double ControlValue(_In_ ControllerReading const& reading, _In_ ControllerControl control) noexcept;

    // A button that is down, or a trigger or a stick direction pushed past half way.
    bool ControlIsDown(_In_ ControllerReading const& reading, _In_ ControllerControl control) noexcept;

    // Where a control rests: the middle for a stick, 0 for anything else.
    double ControlRestValue(_In_ ControllerControl control) noexcept;

    // For Learn: the first control that clearly moved since before was read. A button step looks
    // at buttons, triggers and stick directions, a stick or trigger step at sticks and triggers.
    std::optional<ControllerControl> FindMovedControl(
        _In_ ControllerReading const& before,
        _In_ ControllerReading const& now,
        _In_ bool forButton) noexcept;

    // What a button sends.
    enum class ControllerMessageKind : int32_t
    {
        Nothing = 0,

        // A note on, and its note off when it lets go.
        Note = 1,

        ControlChange = 2,
        ProgramChange = 3,
        PitchBend = 4,
        ChannelPressure = 5,

        // Pressure on one note. Number is the note.
        PolyPressure = 6,

        // RPN and NRPN, always MIDI 2.0. Number is the bank times 128 plus the index.
        RegisteredController = 7,
        AssignableController = 8,

        Start = 9,
        Continue = 10,
        Stop = 11,

        // One whole message, typed in as words.
        Words = 12,
    };

    constexpr size_t ControllerMessageKindCount = 13;

    std::wstring_view ControllerMessageKindKey(_In_ ControllerMessageKind kind) noexcept;
    std::optional<ControllerMessageKind> ControllerMessageKindFromKey(_In_ std::wstring_view key) noexcept;

    // Has a group and a channel, and can go as MIDI 1.0.
    bool IsChannelMessageKind(_In_ ControllerMessageKind kind) noexcept;

    // Carries a value, so it has a value to send when the button lets go.
    bool IsValueMessageKind(_In_ ControllerMessageKind kind) noexcept;

    // The largest number the kind takes: 127, 16383 for an RPN or an NRPN, or 0 for none. A
    // program change's number is only for "this program".
    uint32_t ControllerMessageNumberMaximum(_In_ ControllerMessageKind kind) noexcept;

    // Which program a program change sends.
    enum class ProgramStep : int32_t
    {
        This = 0,
        Next = 1,
        Previous = 2,
    };

    constexpr int32_t DefaultControllerNote = 60;
    constexpr int32_t DefaultControllerVelocityHundredths = 7874;
    constexpr uint8_t MaximumControllerWords = 4;

    struct ControllerMessage
    {
        ControllerMessageKind Kind{ ControllerMessageKind::Note };

        // Both counted from 0.
        uint8_t Group{ 0 };
        uint8_t Channel{ 0 };

        // The note, the controller, the program for "this program", or bank * 128 + index.
        uint32_t Number{ DefaultControllerNote };

        // The note on velocity, or the value sent, in hundredths of a percent.
        int32_t ValueHundredths{ DefaultControllerVelocityHundredths };

        // What letting go sends: the note off for a note, or ReleaseHundredths for a value.
        bool SendsRelease{ true };
        int32_t ReleaseHundredths{ 0 };

        // MIDI 1.0 protocol words instead of MIDI 2.0. An RPN or an NRPN always goes as MIDI 2.0.
        bool Midi1Protocol{ false };

        // Program change only. Next and previous share where they are with every other step in
        // the patch that steps through programs on the same group and channel.
        ProgramStep Step{ ProgramStep::This };
        uint8_t LowestProgram{ 0 };
        uint8_t HighestProgram{ 127 };
        bool Wraps{ true };

        // Words only. Group and all, as typed.
        uint8_t WordCount{ 1 };
        std::array<uint32_t, MaximumControllerWords> Words{};
    };

    // What a message of this kind starts as: middle C, the sustain pedal, program 0, a pitch bend
    // that springs back to the middle, and so on. Group, channel and protocol are left at 0 and
    // MIDI 2.0 for the caller to keep.
    ControllerMessage ControllerMessageFor(_In_ ControllerMessageKind kind) noexcept;

    // How long a button is held before the held message goes, and the repeat rate after it.
    constexpr int32_t MinimumHoldMilliseconds = 100;
    constexpr int32_t MaximumHoldMilliseconds = 10000;
    constexpr int32_t DefaultHoldMilliseconds = 500;
    constexpr int32_t MinimumRepeatMilliseconds = 20;
    constexpr int32_t MaximumRepeatMilliseconds = 5000;
    constexpr int32_t DefaultRepeatMilliseconds = 150;

    // With "tap or hold", a tap's message only goes when the button lets go, and a note or a
    // value it starts ends this long after, so a synth hears it.
    constexpr uint64_t TapMessageMilliseconds = 100;

    struct ControllerButtonSettings
    {
        uint8_t Player{ 0 };
        ControllerControl Control{ ControllerControl::A };

        // Read instead of Xbox controller Player when it isn't empty.
        HidDeviceId Device{};
        HidControl DeviceControl{ HidButtonControl(1) };

        ControllerMessage Pressed{};

        // Each press switches it on, and the next switches it off. Held messages don't apply.
        bool Toggles{ false };

        // Nothing unless Held has a kind. The held message goes once the button has been down
        // for HoldMilliseconds, and again every RepeatMilliseconds when Repeats.
        ControllerMessage Held{ ControllerMessageFor(ControllerMessageKind::Nothing) };
        int32_t HoldMilliseconds{ DefaultHoldMilliseconds };
        bool Repeats{ false };
        int32_t RepeatMilliseconds{ DefaultRepeatMilliseconds };

        // A short press sends the pressed message and a long one sends only the held message, so
        // the pressed message waits until the button lets go.
        bool TapOrHold{ false };

        // Display only.
        ValueScale Scale{ ValueScale::Percent };

        bool SendsWhenHeld() const noexcept;
        bool ReadsDevice() const noexcept { return !Device.IsEmpty(); }
    };

    // A control on an Xbox controller, or on Device when it isn't empty.
    struct LearnedControl
    {
        uint8_t Player{ 0 };
        ControllerControl Control{ ControllerControl::A };
        HidDeviceId Device{};
        HidControl DeviceControl{};
    };

    constexpr int32_t MaximumDeadZoneHundredths = 5000;
    constexpr int32_t DefaultDeadZoneHundredths = 1000;
    constexpr int32_t MaximumAxisIntervalMilliseconds = 1000;
    constexpr int32_t DefaultAxisIntervalMilliseconds = 10;

    struct ControllerAxisSettings
    {
        uint8_t Player{ 0 };
        ControllerControl Control{ ControllerControl::LeftStickX };

        // Read instead of Xbox controller Player when it isn't empty.
        HidDeviceId Device{};
        HidControl DeviceControl{ HidAxisControl(HidAxis::X) };

        // Pitch bend on channel 1, group 1, unless the step says otherwise. Pitch bend has no number.
        midiapp::ValueMessageTarget Target{ midiapp::ValueMessageKind::PitchBend, 0, 0, 0 };

        // How far from where the control rests counts as resting, in hundredths of a percent of
        // its travel. A stick never comes back to exactly the middle.
        int32_t DeadZoneHundredths{ DefaultDeadZoneHundredths };

        ValueShape Shape{};

        // At most one message this often. 0 sends every change.
        int32_t IntervalMilliseconds{ DefaultAxisIntervalMilliseconds };

        // When the patch stops routing or the controller goes away, sends where the control
        // rests, so a pitch bend is not left bent.
        bool ReturnsToRest{ true };

        ValueScale Scale{ ValueScale::Percent };

        bool ReadsDevice() const noexcept { return !Device.IsEmpty(); }
    };

    // A control with no rest, such as a slider, has no dead zone and never goes back to rest.
    ControlRest AxisRest(_In_ ControllerAxisSettings const& settings) noexcept;

    // Where next and previous program left off, for one group and channel in one patch. -1 is
    // before the first one.
    using ProgramPosition = std::shared_ptr<std::atomic<int32_t>>;
    using ProgramPositionFor = std::function<ProgramPosition(uint8_t group, uint8_t channel)>;

    // The program a step lands on from current. -1, or a program outside the range, starts at
    // the lowest going up and the highest going down; -1 starts at the lowest either way.
    uint8_t SteppedProgram(
        _In_ int32_t current,
        _In_ ProgramStep step,
        _In_ uint8_t lowest,
        _In_ uint8_t highest,
        _In_ bool wraps) noexcept;

    // A message as words: what it sends when it starts, and what ends it when the button lets go.
    struct ControllerWords
    {
        std::array<uint32_t, MaximumControllerWords> On{};
        uint8_t OnCount{ 0 };

        std::array<uint32_t, MaximumControllerWords> Off{};
        uint8_t OffCount{ 0 };
    };

    // program is the one a program change sends; nothing else reads it.
    ControllerWords BuildControllerWords(_In_ ControllerMessage const& message, _In_ uint8_t program) noexcept;

    // Where the messages a step makes go. Called on the thread that reads the controller.
    using ControllerSink = std::function<void(uint32_t const* words, uint8_t count)>;

    // Press, hold, repeat, toggle and tap-or-hold for one button.
    class ControllerButtonMachine
    {
    public:
        explicit ControllerButtonMachine(_In_ ProgramPositionFor programs);

        // For every reading, down or not, so a held message keeps time. A controller that goes
        // away counts as letting go.
        void Update(
            _In_ ControllerButtonSettings const& settings,
            _In_ uint64_t nowMilliseconds,
            _In_ bool down,
            _In_ ControllerSink const& sink);

        // Ends whatever is still on: a note still sounding, a value still held, a toggle left on.
        void Release(_In_ ControllerSink const& sink);

    private:
        // The words a message sends now, and what ends it, kept as words so a change of settings
        // while it is on still ends the right note.
        ControllerWords Start(_In_ ControllerMessage const& message);

        struct Pending
        {
            std::array<uint32_t, MaximumControllerWords> Words{};
            uint8_t Count{ 0 };
        };

        static void Send(_In_ ControllerSink const& sink, _Inout_ Pending& pending);

        ProgramPositionFor m_programs{};

        bool m_down{ false };
        uint64_t m_downSince{ 0 };
        bool m_heldSent{ false };
        uint64_t m_nextRepeat{ 0 };
        bool m_tapWaiting{ false };
        bool m_toggledOn{ false };

        Pending m_pressedOff{};
        Pending m_heldOff{};

        Pending m_tapOff{};
        uint64_t m_tapOffAt{ 0 };
    };

    // The value an axis step sends for a reading, 0 to 1: rest inside the dead zone, then the
    // shape.
    double ShapedAxisValue(_In_ ControllerAxisSettings const& settings, _In_ double value) noexcept;

    // A stick or trigger. Sends only when the value moves a step the message can carry, and no
    // more often than the interval.
    class ControllerAxisMachine
    {
    public:
        void Update(
            _In_ ControllerAxisSettings const& settings,
            _In_ uint64_t nowMilliseconds,
            _In_ bool connected,
            _In_ double value,
            _In_ ControllerSink const& sink);

        // Sends where the control rests, when it said to and something else went before.
        void Release(_In_ ControllerAxisSettings const& settings, _In_ ControllerSink const& sink);

    private:
        void SendValue(_In_ ControllerAxisSettings const& settings, _In_ double shaped, _In_ int64_t step, _In_ ControllerSink const& sink);

        bool m_started{ false };
        bool m_sentAny{ false };
        int64_t m_lastStep{ 0 };
        double m_lastSentValue{ 0.0 };
        uint64_t m_lastSentAt{ 0 };
    };

    // How finely an axis step's value has to move before it is sent again: the message's own
    // steps for MIDI 1.0, and 1024 steps for a MIDI 2.0 value, which is well below what a
    // stick's own jitter would fill a cable with.
    int64_t AxisStepCount(_In_ midiapp::ValueMessageTarget const& target) noexcept;

    // Only the parts the message uses are written, so a step in a patch file reads like what it
    // does. Anything missing or out of range takes the default when read.
    void ControllerButtonToJson(_In_ ControllerButtonSettings const& settings, _In_ json::JsonObject const& object);
    ControllerButtonSettings ControllerButtonFromJson(_In_ json::JsonObject const& object) noexcept;
    void ControllerAxisToJson(_In_ ControllerAxisSettings const& settings, _In_ json::JsonObject const& object);
    ControllerAxisSettings ControllerAxisFromJson(_In_ json::JsonObject const& object) noexcept;

    std::wstring ControllerButtonSignature(_In_ ControllerButtonSettings const& settings);
    std::wstring ControllerAxisSignature(_In_ ControllerAxisSettings const& settings);

    // A different controller or control means a running step starts again, which lets go of
    // anything the old control left on. Everything else changes on the fly.
    std::wstring ControllerRestartSignature(
        _In_ uint8_t player,
        _In_ ControllerControl control,
        _In_ HidDeviceId const& device,
        _In_ HidControl deviceControl);
}
