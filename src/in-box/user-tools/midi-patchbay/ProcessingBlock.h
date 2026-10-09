// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The processing steps a patch is built from. Pure: no XAML, no resources and no precompiled
// header, so the unit tests compile this exactly as it ships. Names and summaries for people are
// in MessageText.h.

#include "MessageTransform.h"

// Shared with MIDI Glass and MIDI Clock, in midi-app-shared. All three are pure.
#include "ChannelVoiceWords.h"
#include "LfoWave.h"
#include "MidiTimeCode.h"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace midipatchbay
{
    // Every kind of block. The numbers are never stored: the patch file uses BlockKindKey.
    enum class BlockKind : int32_t
    {
        MessageTypeFilter = 0,
        GroupFilter = 1,
        ChannelFilter = 2,
        NoteFilter = 3,
        ControlChangeFilter = 4,
        VelocityFilter = 5,
        MessageMaskFilter = 6,

        ChannelMap = 7,
        GroupMap = 8,
        NoteMap = 9,
        Transpose = 10,
        Velocity = 11,
        Aftertouch = 12,
        ControlChangeMap = 13,
        ControlChangeValue = 14,
        ProgramMap = 15,

        Throttle = 16,

        ClockGenerator = 17,
        TimeCodeGenerator = 18,
        LfoGenerator = 19,

        ClockDivider = 20,

        Annotation = 21,

        ParameterFilter = 22,
        ParameterTransform = 23,

        NoteDistributor = 24,
        Gate = 25,

        CiResponder = 26,
        CiFilter = 27,

        Branch = 28,
        Switch = 29,
        SetTag = 30,
        SetMemory = 31,
        PutValue = 32,
    };

    constexpr size_t BlockKindCount = 33;

    // The order the palette shows them in.
    constexpr BlockKind AllBlockKinds[BlockKindCount] =
    {
        BlockKind::MessageTypeFilter, BlockKind::GroupFilter, BlockKind::ChannelFilter,
        BlockKind::NoteFilter, BlockKind::ControlChangeFilter, BlockKind::VelocityFilter,
        BlockKind::ParameterFilter, BlockKind::MessageMaskFilter,
        BlockKind::ChannelMap, BlockKind::GroupMap, BlockKind::NoteMap, BlockKind::Transpose,
        BlockKind::Velocity, BlockKind::Aftertouch, BlockKind::ControlChangeMap,
        BlockKind::ControlChangeValue, BlockKind::ProgramMap, BlockKind::ParameterTransform,
        BlockKind::ClockDivider,
        BlockKind::Throttle,
        BlockKind::NoteDistributor,
        BlockKind::Branch, BlockKind::Switch, BlockKind::SetMemory, BlockKind::SetTag, BlockKind::PutValue,
        BlockKind::Gate,
        BlockKind::CiResponder, BlockKind::CiFilter,
        BlockKind::ClockGenerator, BlockKind::TimeCodeGenerator, BlockKind::LfoGenerator,
        BlockKind::Annotation,
    };

    enum class BlockCategory : int32_t
    {
        Filter = 0,
        Transform = 1,
        Sending = 2,

        // Makes messages of its own, so it has a way out and no way in.
        Generator = 3,

        // Text on the canvas. Nothing goes in or comes out.
        Annotation = 4,

        // Decides which of its connections a message goes out on, or whether it goes at all.
        Distribution = 5,

        // Answers MIDI-CI for a device that can't, or keeps MIDI-CI away from one.
        CapabilityInquiry = 6,

        // Decides by a value, remembers one, or puts one into the message.
        Logic = 7,
    };

    BlockCategory CategoryOf(_In_ BlockKind kind) noexcept;

    bool IsGenerator(_In_ BlockKind kind) noexcept;

    bool IsAnnotation(_In_ BlockKind kind) noexcept;

    // Every kind but MIDI clock and MIDI Time Code has an In. An LFO's takes the clock it follows.
    bool HasInput(_In_ BlockKind kind) noexcept;

    // Every kind but an annotation has an Out.
    bool HasOutput(_In_ BlockKind kind) noexcept;

    // Whether a step of this kind can be put into the middle of a connection: it takes what the
    // connection carries and passes it on.
    bool CanGoIntoConnection(_In_ BlockKind kind) noexcept;

    // The name a patch file uses for the kind, for example "noteFilter".
    std::wstring_view BlockKindKey(_In_ BlockKind kind) noexcept;
    std::optional<BlockKind> BlockKindFromKey(_In_ std::wstring_view key) noexcept;

    // Which values a note or control change filter looks at.
    enum class ValueSetMode : int32_t
    {
        Range = 0,
        One = 1,
        List = 2,
    };

    // What happens to the messages a filter picks out.
    enum class FilterAction : int32_t
    {
        LetThrough = 0,
        KeepOut = 1,
    };

    constexpr size_t SevenBitValueCount = 128;

    // A set of 0 to 127 values: notes for the note filter, controller numbers for the control
    // change filter.
    struct ValueSetFilter
    {
        ValueSetMode Mode{ ValueSetMode::Range };
        FilterAction Action{ FilterAction::LetThrough };

        uint8_t Lowest{ 0 };
        uint8_t Highest{ 127 };
        uint8_t One{ 60 };
        std::array<bool, SevenBitValueCount> List{};

        bool Contains(_In_ uint8_t value) const noexcept;
        bool Passes(_In_ uint8_t value) const noexcept;
        bool PassesEverything() const noexcept;
    };

    // Note on velocities, in hundredths of a percent like every other value in this app.
    struct VelocityRange
    {
        FilterAction Action{ FilterAction::LetThrough };
        ValueScale Scale{ ValueScale::Percent };

        int32_t LowestHundredths{ 0 };
        int32_t HighestHundredths{ FullScaleHundredths };

        bool Contains(_In_ int32_t hundredths) const noexcept;
        bool PassesEverything() const noexcept;
    };

    enum class MaskMatch : int32_t
    {
        Exactly = 0,
        AnyOf = 1,
        Between = 2,
    };

    constexpr size_t MaximumMaskConditions = 4;
    constexpr size_t MaximumMaskValues = 128;
    constexpr uint8_t MaximumUmpWords = 4;

    // One place in a message, and the values that count as a match there.
    struct MaskCondition
    {
        // Counted from 0 in the file and from 1 on screen.
        uint8_t Word{ 0 };

        // Both ends included. Bit 31 is the top bit of the word.
        uint8_t HighBit{ 31 };
        uint8_t LowBit{ 0 };

        MaskMatch Match{ MaskMatch::Exactly };

        uint32_t Value{ 0 };
        std::vector<uint32_t> Values{};
        uint32_t Lowest{ 0 };
        uint32_t Highest{ 0 };

        uint8_t BitCount() const noexcept;
        uint32_t FieldMaximum() const noexcept;
        uint32_t FieldOf(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
        bool Matches(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
    };

    // The way out for any message the other filters do not cover: look at bits in messages of one
    // size, and let the ones that match through or keep them out.
    struct MessageMask
    {
        uint8_t WordCount{ 2 };
        FilterAction Action{ FilterAction::KeepOut };

        // Display only.
        bool ShowHex{ false };

        // All of them have to match. With none, the block does nothing.
        std::vector<MaskCondition> Conditions{};

        bool Passes(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
    };

    // Unchanged where -1.
    using GroupMapTable = std::array<int8_t, 16>;

    constexpr uint32_t DefaultThrottleSpeed = 1;

    // The tempo a clock or an LFO runs at. The same range as MIDI Clock.
    constexpr double MinimumGeneratorBeatsPerMinute = 20.0;
    constexpr double MaximumGeneratorBeatsPerMinute = 300.0;
    constexpr double DefaultGeneratorBeatsPerMinute = 120.0;

    // MIDI clock, 24 pulses a quarter note, from the moment the patch starts routing.
    struct ClockGeneratorSettings
    {
        double BeatsPerMinute{ DefaultGeneratorBeatsPerMinute };

        // Start as routing starts and stop as it stops, so a sequencer follows the patch.
        bool SendStartStop{ true };

        // 50 is straight. Up to 75, on eighth notes (2) or sixteenth notes (4).
        double SwingPercent{ 50.0 };
        int32_t SwingSubdivision{ 2 };

        // Counted from 0.
        uint8_t Group{ 0 };
    };

    struct TimeCodeGeneratorSettings
    {
        midiapp::MidiTimeCodeFrameRate FrameRate{ midiapp::MidiTimeCodeFrameRate::Frames30 };
        midiapp::MidiTimeCodePosition Start{};

        // A full frame message as it starts and as it stops, so a receiver finds its place at
        // once instead of reading it from the next eight quarter frames.
        bool SendFullFrame{ true };

        uint8_t Group{ 0 };
    };

    struct LfoGeneratorSettings
    {
        midiapp::LfoWave Wave{ midiapp::LfoWave::Sine };

        // How long one pass takes, in quarter notes at BeatsPerMinute. Four is one bar.
        double BeatsPerCycle{ 4.0 };
        double BeatsPerMinute{ DefaultGeneratorBeatsPerMinute };

        // The two ends of the sweep, in hundredths of a percent of the message's whole range.
        // Lowest above highest turns the wave upside down.
        int32_t LowestHundredths{ 0 };
        int32_t HighestHundredths{ FullScaleHundredths };

        int32_t IntervalMilliseconds{ midiapp::DefaultLfoIntervalMilliseconds };

        // The mod wheel on channel 1, group 1, unless the step says otherwise.
        midiapp::ValueMessageTarget Target{};

        // Sends the middle of the range when the patch stops routing, so a pitch bend is not
        // left bent.
        bool ReturnsToMiddle{ true };

        // Following a clock: waits for Start or Continue, and holds on Stop.
        bool KeepsToStartAndStop{ false };
    };

    // What an LFO step's number starts as for each kind of message: the mod wheel, middle C, or
    // the first RPN or NRPN.
    uint32_t DefaultLfoNumber(_In_ midiapp::ValueMessageKind kind) noexcept;

    // Clock divider: one timing clock in this many goes through. 1 lets every one through.
    constexpr uint32_t DefaultClockDivision = 2;
    constexpr uint32_t MaximumClockDivision = 96;

    // RPN is registered, NRPN is assignable. Either is only for matching.
    enum class ParameterKind : int32_t
    {
        Registered = 0,
        Assignable = 1,
        Either = 2,
    };

    // Bank and index are 0 to 127, or -1 for any.
    struct ParameterMatch
    {
        ParameterKind Kind{ ParameterKind::Registered };
        int16_t Bank{ -1 };
        int16_t Index{ -1 };

        bool Matches(_In_ bool assignable, _In_ uint8_t bank, _In_ uint8_t index) const noexcept;
    };

    constexpr size_t MaximumParameterRows = 32;

    // With no parameters listed, the step does nothing.
    struct ParameterFilterSettings
    {
        FilterAction Action{ FilterAction::KeepOut };
        std::vector<ParameterMatch> Parameters{};
    };

    // One parameter moved to another, its value reshaped on the way. To fields of -1, or a kind
    // of Either, keep what came in.
    struct ParameterMapRow
    {
        ParameterMatch From{};

        ParameterKind ToKind{ ParameterKind::Either };
        int16_t ToBank{ -1 };
        int16_t ToIndex{ -1 };

        ValueShape Shape{};
    };

    // The first row that matches is the one used.
    struct ParameterTransformSettings
    {
        std::vector<ParameterMapRow> Rows{};
    };

    // Which voice a new note goes to. A voice is one connection out of the step.
    enum class DistributionMode : int32_t
    {
        // Each note goes to the next voice along, skipping voices that are still playing.
        TakeTurns = 0,

        // The first voice that isn't playing. With none free, the oldest note is cut short.
        FirstFree = 1,

        // With every voice playing, a new note replaces the lowest one if it's higher.
        HighestNotes = 2,

        // With every voice playing, a new note replaces the highest one if it's lower.
        LowestNotes = 3,
    };

    constexpr size_t MaximumVoices = 64;

    struct NoteDistributorSettings
    {
        DistributionMode Mode{ DistributionMode::TakeTurns };

        // Otherwise each goes only to the voice that played the latest note.
        bool ControlChangesToEveryVoice{ true };
        bool ChannelPressureToEveryVoice{ true };
        bool PitchBendToEveryVoice{ true };
    };

    enum class GateTriggerKind : int32_t
    {
        NoteOn = 0,
        NoteOff = 1,
        ControlChange = 2,
        ProgramChange = 3,
        Start = 4,
        Continue = 5,
        Stop = 6,

        // The first words of a message, exactly.
        Words = 7,
    };

    enum class GateValueTest : int32_t
    {
        Any = 0,
        AtLeast = 1,
        Below = 2,
    };

    // A message that opens or closes a gate. Group, channel and number are -1 for any, and count
    // from 0 like everywhere else in the file.
    struct GateTrigger
    {
        GateTriggerKind Kind{ GateTriggerKind::Start };

        int8_t Group{ -1 };
        int8_t Channel{ -1 };

        // The note, controller or program.
        int16_t Number{ -1 };

        // Control change only. Compared on the MIDI 1.0 scale, so a MIDI 2.0 value is compared
        // at its top seven bits.
        GateValueTest Test{ GateValueTest::Any };
        uint8_t Value{ 64 };

        // Words only.
        uint8_t WordCount{ 1 };
        std::array<uint32_t, MaximumUmpWords> Words{};

        bool Matches(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) const noexcept;
    };

    // Lets messages through between one trigger and the other. The same trigger for both turns
    // it on and off. Note offs always go through, so nothing is left sounding.
    struct GateSettings
    {
        GateTrigger Open{ GateTriggerKind::Start };
        GateTrigger Close{ GateTriggerKind::Stop };

        bool StartsOpen{ true };

        // Off keeps the trigger messages out. On sends them on whether the gate is open or not.
        bool PassesTriggers{ true };
    };

    // ------------------------------------------------------------------ logic

    // The part of a message a logic step reads or writes.
    enum class MessagePart : int32_t
    {
        Group = 0,
        Channel = 1,
        Note = 2,
        Velocity = 3,
        ControllerNumber = 4,
        ControllerValue = 5,
        Program = 6,
        BankMsb = 7,
        BankLsb = 8,
        Pressure = 9,
        PitchBend = 10,

        // Any bits of any word, the same way the message mask filter finds them.
        Bits = 11,
    };

    constexpr size_t MessagePartCount = 12;

    // What a number counts, which decides how it is shown and how it is compared. Channels and
    // groups count from 0 in the file and from 1 on screen. A value is a share of its whole
    // range, so a MIDI 1.0 and a MIDI 2.0 message compare the same: hundredths of a percent here,
    // a percent in the file.
    enum class LogicUnit : int32_t
    {
        Number = 0,
        Channel = 1,
        Group = 2,
        Note = 3,
        Value = 4,
    };

    LogicUnit UnitOfPart(_In_ MessagePart part) noexcept;

    // Where a part is. The word and the bits only mean something for MessagePart::Bits.
    struct PartPlace
    {
        MessagePart Part{ MessagePart::Channel };
        uint8_t Word{ 0 };
        uint8_t HighBit{ 31 };
        uint8_t LowBit{ 0 };
    };

    enum class LogicSourceKind : int32_t
    {
        Number = 0,
        Part = 1,
        Tag = 2,
        Memory = 3,
    };

    // Names are compared without regard to case, and only this many tags and memories in one
    // patch get a place. The rest read as empty.
    constexpr size_t MaximumLogicNameLength = 32;
    constexpr size_t MaximumTagsPerPatch = 64;
    constexpr size_t MaximumMemoriesPerPatch = 64;
    constexpr uint32_t NoLogicIndex = 0xFFFFFFFF;

    // Where a value comes from: a number typed in, a part of the message, a tag or a memory.
    struct LogicSource
    {
        LogicSourceKind Kind{ LogicSourceKind::Part };

        // A number typed in. Channels and groups from 0, a value in hundredths of a percent.
        LogicUnit Unit{ LogicUnit::Number };
        uint32_t Number{ 0 };

        PartPlace Place{};

        // The tag or the memory.
        std::wstring Name{};

        // The tag's slot or the memory's place, filled in when the patch routes. Never in the file.
        uint32_t Index{ NoLogicIndex };
    };

    enum class LogicTest : int32_t
    {
        // Not a test: everything goes out the first way. What a new Branch starts as.
        Anything = 0,

        Is = 1,
        IsNot = 2,
        AtLeast = 3,
        Below = 4,

        // Both ends included.
        Between = 5,
        OneOf = 6,

        HasValue = 7,
        IsEmpty = 8,
    };

    constexpr size_t MaximumLogicListValues = 128;

    // One comparison, in the unit of the step it belongs to.
    struct LogicCondition
    {
        LogicTest Test{ LogicTest::Is };
        uint32_t Value{ 0 };
        uint32_t Lowest{ 0 };
        uint32_t Highest{ 0 };
        std::vector<uint32_t> Values{};
    };

    // Where a message goes that a Branch or a Switch can't look at: one without the part it
    // tests, or a tag or memory that is empty.
    enum class UnreadableWay : int32_t
    {
        EveryWay = 0,
        KeepOut = 1,

        // Yes on a Branch.
        FirstWay = 2,

        // No on a Branch, "Anything else" on a Switch.
        LastWay = 3,
    };

    // What a bypassed Branch or Switch does.
    enum class BypassWay : int32_t
    {
        EveryWay = 0,
        FirstWay = 1,
    };

    // The ways out of a Branch.
    constexpr int32_t BranchYesWay = 0;
    constexpr int32_t BranchNoWay = 1;

    // A Switch's ways: "Anything else" is 0, and each case keeps an id of its own from 1 up, so
    // its connections stay with it when cases are added, taken away or moved.
    constexpr int32_t SwitchOtherwiseWay = 0;
    constexpr size_t MaximumSwitchCases = 64;
    constexpr int32_t MaximumSwitchCaseId = 64;

    struct BranchSettings
    {
        LogicSource Subject{};
        LogicUnit Unit{ LogicUnit::Channel };
        LogicCondition Condition{};

        UnreadableWay Unreadable{ UnreadableWay::EveryWay };
        BypassWay Bypass{ BypassWay::EveryWay };

        // Display only, for a value.
        ValueScale Scale{ ValueScale::SevenBit };
    };

    struct SwitchCase
    {
        int32_t Id{ 1 };
        LogicCondition Condition{};
    };

    // The first case that matches is the way a message goes.
    struct SwitchSettings
    {
        LogicSource Subject{};
        LogicUnit Unit{ LogicUnit::Channel };
        std::vector<SwitchCase> Cases{};

        UnreadableWay Unreadable{ UnreadableWay::EveryWay };
        BypassWay Bypass{ BypassWay::EveryWay };

        ValueScale Scale{ ValueScale::SevenBit };
    };

    // Gives the message a value to carry for the rest of its trip through the patch.
    struct SetTagSettings
    {
        std::wstring Tag{};
        uint32_t TagIndex{ NoLogicIndex };

        LogicSource Value{};

        ValueScale Scale{ ValueScale::SevenBit };
    };

    enum class MemoryAction : int32_t
    {
        Set = 0,

        // Between the two numbers. An empty memory takes the first.
        Toggle = 1,

        StepUp = 2,
        StepDown = 3,

        // Back to empty.
        Clear = 4,
    };

    // Remembers a value for later messages to use. A memory lasts as long as the app runs.
    struct SetMemorySettings
    {
        std::wstring Memory{};
        uint32_t MemoryIndex{ NoLogicIndex };

        // Every message that reaches the step, or only the trigger.
        bool EveryMessage{ true };
        GateTrigger Trigger{ GateTriggerKind::ProgramChange };

        MemoryAction Action{ MemoryAction::Set };
        LogicSource Value{};

        // Toggle: the two numbers. Steps: the range, ends included.
        LogicUnit Unit{ LogicUnit::Number };
        uint32_t First{ 0 };
        uint32_t Second{ 1 };
        uint32_t Lowest{ 0 };
        uint32_t Highest{ 7 };
        bool Wraps{ true };

        // Off keeps out the messages that change the memory.
        bool PassesTriggers{ true };

        ValueScale Scale{ ValueScale::SevenBit };
    };

    // Writes a value into one part of the message.
    struct PutValueSettings
    {
        PartPlace Target{};
        LogicSource Value{};

        // Off sends a message on unchanged when the value is empty.
        bool KeepsOutWhenEmpty{ false };

        ValueScale Scale{ ValueScale::SevenBit };
    };

    // What a MIDI-CI file next to the patch describes: profiles and properties. Read when the patch
    // routes, so it is never part of the patch file itself. In CapabilityInquiry.h.
    struct CiDescription;

    // Answers MIDI-CI for the device it leads to, which is usually a MIDI 1.0 device that knows
    // nothing about it. Answers go back to the endpoint the question came from.
    struct CiResponderSettings
    {
        // MIDI-CI always carries three bytes. A one byte ID such as 0x41 is 41 00 00. 0x7D is the
        // ID for prototypes and private use.
        std::array<uint8_t, 3> Manufacturer{ 0x7D, 0x00, 0x00 };

        // 14 bits each.
        uint16_t Family{ 0 };
        uint16_t Model{ 0 };

        // Four 7-bit numbers, written 1.0.0.0.
        std::array<uint8_t, 4> Version{};

        // What Inquiry: Endpoint is answered with. Printable ASCII, up to 42 characters.
        std::wstring ProductInstanceId{};

        // Reports what has been sent through the step when an app asks for a MIDI Message Report.
        bool ProcessInquiry{ true };

        // MIDI-CI messages go on to the device as well. Off, the device never sees them.
        bool PassMidiCi{ false };

        // A file in the patch folder, by name only.
        std::wstring FileName{};

        // What the file said, attached when the patch routes. Null when there is no file, or it
        // couldn't be read.
        std::shared_ptr<CiDescription const> Description{};
    };

    // Groups of MIDI-CI messages, as a bit each.
    constexpr uint8_t CiCategoryManagement = 0x01;
    constexpr uint8_t CiCategoryProfiles = 0x02;
    constexpr uint8_t CiCategoryPropertyExchange = 0x04;
    constexpr uint8_t CiCategoryProcessInquiry = 0x08;
    constexpr uint8_t CiCategoryAll = 0x0F;

    struct CiFilterSettings
    {
        FilterAction Action{ FilterAction::KeepOut };
        uint8_t Categories{ CiCategoryAll };
    };

    // The name a patch can give a MIDI-CI file: a bare file name, never a path.
    bool IsCiFileName(_In_ std::wstring_view name) noexcept;

    // Printable ASCII only, and no more than MIDI-CI carries.
    std::wstring CiProductInstanceIdFrom(_In_ std::wstring_view text);

    // Text on the canvas, for notes about the patch. Lines break only where the text does.
    constexpr size_t MaximumAnnotationLength = 1000;
    constexpr double MinimumAnnotationFontSize = 8.0;
    constexpr double MaximumAnnotationFontSize = 96.0;
    constexpr double DefaultAnnotationFontSize = 16.0;

    struct AnnotationSettings
    {
        std::wstring Text{};

        // Empty is the app's own font.
        std::wstring FontFamily{};

        double FontSize{ DefaultAnnotationFontSize };
        bool Bold{ false };
        bool Italic{ false };
        bool Underline{ false };

        // "#RRGGBB", or empty for the theme's text color, which reads in both themes.
        std::wstring Color{};

        bool operator==(AnnotationSettings const&) const = default;
    };

    // The text as an annotation keeps it: line breaks as line feeds, no other control characters,
    // no line break at the end, and no longer than the most an annotation holds.
    std::wstring AnnotationTextFrom(_In_ std::wstring_view text);

    // "#RRGGBB" in capitals, or empty when the text is not a color in that form.
    std::wstring AnnotationColorFrom(_In_ std::wstring_view text);

    // Everything any kind of block can hold. Each kind uses only its own part, so one shape
    // serves every kind and a block can be copied, compared and undone as a plain value.
    struct BlockSettings
    {
        // Message type filter and channel filter.
        MessageFilter Filter{};

        // Every transform kind uses the part of this it is named for.
        MessageTransform Transform{};

        // Note filter and control change filter.
        ValueSetFilter Values{};

        VelocityRange Velocities{};

        MessageMask Mask{};

        // Group filter: the groups let through.
        std::array<bool, 16> Groups{};

        GroupMapTable GroupMap{};

        // Throttle: a multiple of MIDI 1.0 wire speed, 0 for no limit.
        uint32_t SendSpeedLimit{ DefaultThrottleSpeed };

        ClockGeneratorSettings Clock{};
        TimeCodeGeneratorSettings TimeCode{};
        LfoGeneratorSettings Lfo{};

        uint32_t ClockDivision{ DefaultClockDivision };

        ParameterFilterSettings ParameterFilter{};
        ParameterTransformSettings ParameterTransform{};
        NoteDistributorSettings Distributor{};
        GateSettings Gate{};

        CiResponderSettings CiResponder{};
        CiFilterSettings CiFilter{};

        BranchSettings Branch{};
        SwitchSettings Switch{};
        SetTagSettings SetTag{};
        SetMemorySettings SetMemory{};
        PutValueSettings PutValue{};

        AnnotationSettings Annotation{};
    };

    // What a block dropped on the canvas starts as. Apart from the throttle, every kind starts
    // out changing nothing, so adding one never surprises a running patch.
    BlockSettings DefaultBlockSettings(_In_ BlockKind kind) noexcept;

    // Runs one block on one message. False means the message is kept out. Transforms rewrite the
    // words in place, so each path through a patch works on its own copy. Called on the service
    // callback thread for every message: no allocation, no locks, nothing that can throw.
    bool ProcessBlock(
        _In_ BlockKind kind,
        _In_ BlockSettings const& settings,
        _Inout_updates_(wordCount) uint32_t* words,
        _In_ uint8_t wordCount) noexcept;

    // True when the block, as set, leaves every message as it is.
    bool BlockChangesNothing(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // Only the part the kind uses is written, so a block in a patch file reads like what it does.
    json::JsonObject BlockSettingsToJson(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // Anything missing or out of range takes the default, the same as everywhere else in the
    // patch file: these files can come from anywhere.
    BlockSettings BlockSettingsFromJson(_In_ BlockKind kind, _In_ json::JsonObject const& object) noexcept;

    // Short and stable, for telling whether a running route needs to change.
    std::wstring BlockSettingsSignature(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // A velocity from a message, in hundredths of a percent. MIDI 1.0 carries 7 bits and MIDI 2.0
    // carries 16, so both land on the same scale.
    int32_t HundredthsFromVelocity16(_In_ uint16_t velocity) noexcept;

    // The clock divider, on one message. Lets one timing clock in every divideBy through and
    // counts the rest. A start resets the count, so the first clock after it always goes through,
    // and a song position is divided too and moves the count to match. Start, stop, continue and
    // everything else go through untouched.
    //
    // The count is per step rather than per path, so it is atomic: two sources can reach one
    // divider on two threads.
    bool DivideClock(
        _In_ uint32_t divideBy,
        _Inout_ std::atomic<uint32_t>& counter,
        _Inout_updates_(wordCount) uint32_t* words,
        _In_ uint8_t wordCount) noexcept;

    // The settings a running generator cannot take on the fly: equal for two settings that only
    // differ in what it can change while it runs, such as the tempo. Starting a generator again
    // is something the receiving device notices, so this is kept to what it has to.
    std::wstring GeneratorRestartSignature(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // Branch, Switch, Set tag, Set memory and Put value.
    bool IsLogicStep(_In_ BlockKind kind) noexcept;

    // A Branch and a Switch have more than one way out. A connection from one names its way in
    // its source group: see BranchYesWay and SwitchOtherwiseWay.
    bool HasWays(_In_ BlockKind kind) noexcept;

    // The ways out, in the order the canvas shows them. Empty for a kind with one way out.
    std::vector<int32_t> WaysOf(_In_ BlockKind kind, _In_ BlockSettings const& settings);

    // The way a bypassed step sends on when its settings say only the first way: Yes, or the
    // first case, or "Anything else" when there are none.
    int32_t FirstWayOf(_In_ BlockKind kind, _In_ BlockSettings const& settings) noexcept;

    // A Switch case id that isn't taken: the smallest from 1 up, or 0 when all of them are.
    int32_t NewSwitchCaseId(_In_ SwitchSettings const& settings) noexcept;

    // The names a step reads or writes, for the editor's lists. Trimmed, never empty.
    void CollectLogicNames(
        _In_ BlockKind kind,
        _In_ BlockSettings const& settings,
        _Inout_ std::vector<std::wstring>& tags,
        _Inout_ std::vector<std::wstring>& memories);

    // A tag or memory name as a patch keeps it: trimmed, no control characters, and no longer
    // than the most a name holds.
    std::wstring LogicNameFrom(_In_ std::wstring_view text);

    // Whether two tag or memory names are the same name.
    bool SameLogicName(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept;
}
