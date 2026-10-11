// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The sequence a window edits and plays: tracks, folders, scenes, clips, tags, tempo and meter.
//
// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged. JSON objects appear
// only to carry keys a newer build wrote, so a file opened here and saved again loses nothing.

#include <sal.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>

#include "ContentProvenance.h"

namespace midisequencer
{
    // Every position is in ticks at this resolution. 960 divides evenly into halves, thirds,
    // quarters, fifths, sixths and eighths of a beat, which covers every grid the editors offer.
    inline constexpr int64_t TicksPerQuarterNote = 960;

    inline constexpr uint32_t CurrentFileVersion = 1;

    // Ceilings for a file that arrives from anywhere. Far above anything a person builds.
    struct SequenceLimits
    {
        size_t MaximumTracks{ 4096 };
        size_t MaximumFolderDepth{ 16 };
        size_t MaximumScenes{ 1024 };
        size_t MaximumClips{ 65536 };
        size_t MaximumNotesPerClip{ 2000000 };
        size_t MaximumEventsPerClip{ 2000000 };
        size_t MaximumNotesInSequence{ 8000000 };
        size_t MaximumPlacementsPerTrack{ 100000 };
        size_t MaximumTags{ 100000 };
        size_t MaximumTempoPoints{ 100000 };
        size_t MaximumTextLength{ 1024 };
        size_t MaximumLanesPerOwner{ 512 };
        size_t MaximumPointsPerLane{ 1000000 };
        size_t MaximumLanePointsInSequence{ 8000000 };
        size_t MaximumLyrics{ 100000 };
        int64_t MaximumTick{ 0x7FFFFFFF };
    };

    // A note at MIDI 2.0 resolution. Per-note pitch bend and per-note controllers are ordinary
    // events in the clip, because that is what they are on the wire: messages that name a note
    // number, which apply to whichever note with that number is sounding.
    struct Note
    {
        int64_t Tick{ 0 };              // from the start of the clip
        int64_t Length{ 0 };
        uint8_t Channel{ 0 };           // 0 to 15
        uint8_t Number{ 0 };            // 0 to 127
        uint16_t Velocity{ 0 };         // 0 to 65535
        uint16_t ReleaseVelocity{ 0 };
        uint8_t AttributeType{ 0 };     // 0 when the note has no attribute
        uint16_t AttributeData{ 0 };
        uint8_t Chance{ 100 };          // percent

        bool operator==(Note const&) const = default;
    };

    // Any other message, as the Universal MIDI Packet it was recorded or drawn as. The group in
    // the first word is ignored: the destination decides the group.
    struct ClipEvent
    {
        int64_t Tick{ 0 };
        std::array<uint32_t, 4> Words{};
        uint8_t WordCount{ 0 };

        bool operator==(ClipEvent const&) const = default;
    };

    enum class ClipKind : uint8_t
    {
        Notes = 0,
        Pattern = 1,
        Generator = 2,
    };

    // How a clip came to be, which is what lets the app suggest how a whole sequence was made.
    enum class ClipOrigin : uint8_t
    {
        Unknown = 0,
        Recorded = 1,
        Drawn = 2,
        Generated = 3,
        Imported = 4,
        Assistant = 5,
    };

    // What an automation lane changes.
    enum class LaneTarget : uint8_t
    {
        Controller = 0,         // Number is the controller, 0 to 127
        PitchBend = 1,
        ChannelPressure = 2,
        Program = 3,
        Registered = 4,         // RPN. Number is bank * 128 + index
        Assignable = 5,         // NRPN. Number is bank * 128 + index
    };

    // A lane's value at a moment. Every target keeps 32 bits; a program lane's value is scaled to
    // 7 bits when it's sent, and always holds until the next point.
    struct LanePoint
    {
        int64_t Tick{ 0 };
        uint32_t Value{ 0 };

        // Stay at this value until the next point, rather than a straight line to it.
        bool Hold{ false };

        bool operator==(LanePoint const&) const = default;
    };

    // Automation: a controller, pitch bend, pressure or program drawn over time. On a track, ticks
    // are on the timeline. In a clip they're from the clip's start, and loop with it.
    struct AutomationLane
    {
        std::wstring Id{};

        // Empty uses the target's own name, such as "Controller 74".
        std::wstring Name{};

        LaneTarget Target{ LaneTarget::Controller };
        uint16_t Number{ 0 };

        // 0 to 15. A destination that sets a channel overrides it, as it does for notes.
        uint8_t Channel{ 0 };

        // Sorted by tick. Two points at one tick make a jump: the line arrives at the first and
        // leaves from the second.
        std::vector<LanePoint> Points{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    // ---- pattern clips ----

    enum class PatternMode : uint8_t
    {
        Drums = 0,      // a lane for each drum, each playing its own note
        Melodic = 1,    // one line of notes: each step has its own note, accent and slide
    };

    // Which passes a step plays on, counted through its own lane.
    enum class StepCondition : uint8_t
    {
        Always = 0,
        Every = 1,          // pass Of of every Every passes: 1 of 2 is the first of each two
        FirstPass = 2,
        NotFirstPass = 3,
    };

    enum class RatchetShape : uint8_t
    {
        Even = 0,
        Softer = 1,         // each repeat a little softer than the one before
        Louder = 2,         // each repeat a little louder
    };

    // A step that plays. Steps that don't play aren't stored.
    struct PatternStep
    {
        uint16_t Index{ 0 };                // which step of its lane, from 0
        uint16_t Velocity{ 51400 };         // 100 of 127, scaled up
        uint8_t Chance{ 100 };              // percent
        uint8_t Ratchet{ 1 };               // how many hits inside the step, 1 to 8
        RatchetShape Shape{ RatchetShape::Even };
        StepCondition Condition{ StepCondition::Always };
        uint8_t Every{ 2 };                 // 2 to 16
        uint8_t Of{ 1 };                    // 1 to Every
        int16_t Nudge{ 0 };                 // ticks early (negative) or late, at most half a step
        uint16_t Length{ 50 };              // percent of a step, 1 to 1600
        uint8_t Note{ 48 };                 // melodic mode. A drum lane plays the lane's note.
        bool Accent{ false };               // melodic mode: plays at full velocity
        bool Slide{ false };                // melodic mode: runs on into the next step

        bool operator==(PatternStep const&) const = default;
    };

    struct PatternLane
    {
        std::wstring Id{};

        // Empty uses the drum's name, or the note's.
        std::wstring Name{};

        uint8_t Note{ 36 };

        // How many steps before the lane starts again, when lanes keep their own length.
        uint16_t StepCount{ 16 };

        bool Muted{ false };
        bool Soloed{ false };

        // Sorted by index, one per step.
        std::vector<PatternStep> Steps{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    struct PatternSettings
    {
        PatternMode Mode{ PatternMode::Drums };

        // Steps in one pass of the clip, 1 to 64. The clip's length is this many steps.
        uint16_t StepCount{ 16 };
        int64_t StepTicks{ TicksPerQuarterNote / 4 };

        // Where every second step starts, as a percent of two steps: 50 is straight, up to 75.
        uint8_t Swing{ 50 };

        // Each lane runs its own number of steps, so a 12-step lane drifts against a 16-step one.
        // Off, every lane runs the pattern's StepCount.
        bool LanesKeepLength{ true };

        // 0 to 15. A destination that sets a channel overrides it, as it does for notes.
        uint8_t Channel{ 9 };

        std::vector<PatternLane> Lanes{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    // ---- generator clips ----

    enum class GeneratorType : uint8_t
    {
        Euclidean = 0,
        ChanceMelody = 1,
        Arpeggiator = 2,    // not built yet: plays nothing, and its settings are kept as written
    };

    // A ring of a Euclidean rhythm: Pulses spread as evenly as they go over Steps, turned by
    // Rotate. Each ring runs its own number of steps, like a pattern lane.
    struct EuclidRing
    {
        uint8_t Note{ 36 };
        uint8_t Pulses{ 4 };
        uint8_t Steps{ 16 };                // 1 to 64
        uint8_t Rotate{ 0 };                // 0 to Steps - 1
        uint16_t Velocity{ 51400 };
        uint8_t Chance{ 100 };              // percent

        bool operator==(EuclidRing const&) const = default;
    };

    enum class ScaleKind : uint8_t
    {
        Major = 0,
        NaturalMinor = 1,
        Dorian = 2,
        Phrygian = 3,
        Lydian = 4,
        Mixolydian = 5,
        HarmonicMinor = 6,
        MajorPentatonic = 7,
        MinorPentatonic = 8,
        Blues = 9,
        Chromatic = 10,
    };

    struct GeneratorSettings
    {
        GeneratorType Type{ GeneratorType::Euclidean };
        int64_t StepTicks{ TicksPerQuarterNote / 4 };

        // Off, every pass plays what the first one did. On, chance and choices are made again on
        // each pass, from the seed and the pass number.
        bool ChangesEachPass{ false };

        // 0 to 15. A destination that sets a channel overrides it, as it does for notes.
        uint8_t Channel{ 0 };

        // How long each note lasts, as a percent of a step.
        uint16_t NoteLength{ 50 };

        // Euclidean rhythm
        std::vector<EuclidRing> Rings{};

        // Chance melody: notes from a scale inside a range, on about Busy percent of the steps. When
        // it changes each pass, Keep percent of the steps play what they played the pass before.
        uint8_t ScaleRoot{ 4 };             // 0 is C, 4 is E
        ScaleKind Scale{ ScaleKind::MinorPentatonic };
        uint8_t LowNote{ 52 };
        uint8_t HighNote{ 64 };
        uint8_t Busy{ 50 };
        uint8_t Keep{ 75 };
        uint16_t VelocityLow{ 32768 };
        uint16_t VelocityHigh{ 62258 };

        // Keys this version doesn't know, which hold an arpeggiator's settings until it's built.
        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    struct Clip
    {
        std::wstring Id{};
        ClipKind Kind{ ClipKind::Notes };
        std::wstring Name{};
        std::optional<uint32_t> Color{};        // 0xRRGGBB, or the track's color
        int64_t Length{ TicksPerQuarterNote * 4 };
        bool Loop{ true };

        // Sorted by tick, then by note number, so playback and drawing never search.
        std::vector<Note> Notes{};

        // Sorted by tick. Messages at one tick keep the order they were written in, because that
        // order is often the difference between a bank select landing before or after its
        // program change.
        std::vector<ClipEvent> Events{};

        // The seed for chance, patterns and generators. Saved, so playback is the same every time.
        uint32_t Seed{ 0 };

        ClipOrigin Origin{ ClipOrigin::Unknown };

        // The file it was imported from, or the device it was recorded from.
        std::wstring OriginDetail{};

        // Settings of a clip kind this version doesn't know, kept exactly as written.
        winrt::Windows::Data::Json::JsonObject Settings{ nullptr };

        // A pattern clip's steps. Its notes are worked out from them (see RefreshPatternPreview),
        // so drawing a pattern is drawing notes, and they aren't saved.
        std::optional<PatternSettings> Pattern{};

        // A generator clip's settings. Like a pattern, its notes are worked out, not saved.
        std::optional<GeneratorSettings> Generator{};

        // Inside a placement of this clip, and while it's launched, a lane here wins over the
        // track's lane for the same target.
        std::vector<AutomationLane> Lanes{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    // Text at a moment. Never sent to a device.
    struct Tag
    {
        int64_t Tick{ 0 };
        std::wstring Text{};
        std::optional<uint32_t> Color{};

        bool operator==(Tag const&) const = default;
    };

    // Matched by name, the way MIDI Glass and MIDI Patchbay do it, so a sequence moves between
    // PCs. The id is a hint for the PC that saved it.
    struct EndpointRef
    {
        std::wstring Name{};
        std::wstring Id{};

        bool IsEmpty() const noexcept { return Name.empty() && Id.empty(); }

        bool operator==(EndpointRef const&) const = default;
    };

    enum class ProtocolChoice : uint8_t
    {
        // What the destination's function block, or group terminal block, says it speaks.
        Automatic = 0,
        Midi1 = 1,
        Midi2 = 2,
    };

    inline constexpr uint16_t AllChannels = 0xFFFF;

    // What a track records, one bit each. System exclusive has its own switch because it's off
    // unless asked for.
    inline constexpr uint8_t RecordNotes = 0x01;
    inline constexpr uint8_t RecordControllers = 0x02;
    inline constexpr uint8_t RecordPitchBend = 0x04;
    inline constexpr uint8_t RecordPressure = 0x08;
    inline constexpr uint8_t RecordProgram = 0x10;
    inline constexpr uint8_t RecordEverything = 0x1F;

    struct TrackSource
    {
        EndpointRef Endpoint{};
        int8_t Group{ -1 };                     // -1 is any group
        uint16_t Channels{ AllChannels };       // one bit per channel
        bool SystemExclusive{ false };
        uint8_t Record{ RecordEverything };

        // What's played on the source goes straight to the destination while the track is armed.
        bool Echo{ true };

        bool operator==(TrackSource const&) const = default;
    };

    struct TrackDestination
    {
        EndpointRef Endpoint{};
        uint8_t Group{ 0 };
        int8_t Channel{ 0 };                    // -1 keeps each message's own channel
        ProtocolChoice Protocol{ ProtocolChoice::Automatic };

        bool operator==(TrackDestination const&) const = default;
    };

    // A clip placed on the timeline. The same clip can be placed many times: it's one clip, so an
    // edit reaches every placement.
    struct Placement
    {
        std::wstring ClipId{};
        int64_t Tick{ 0 };

        // How long it plays, which can be longer than the clip when the clip loops. 0 is the
        // clip's own length.
        int64_t Length{ 0 };

        bool operator==(Placement const&) const = default;
    };

    struct Track
    {
        std::wstring Id{};
        bool IsFolder{ false };
        std::wstring Name{};
        uint32_t Color{ 0x9E9E9E };
        bool Pinned{ false };
        bool Muted{ false };
        bool Soloed{ false };

        // Folders only. Kept in the file so a sequence opens the way it was left.
        bool Open{ true };

        TrackSource Source{};
        TrackDestination Destination{};

        // Sent before the first note. Their ticks are ignored.
        std::vector<ClipEvent> Startup{};

        std::vector<Placement> Timeline{};

        // One per scene. Empty is a slot with no clip.
        std::vector<std::wstring> Slots{};

        std::vector<Tag> Tags{};

        // Automation on the timeline, drawn under the track while LanesOpen.
        std::vector<AutomationLane> Lanes{};
        bool LanesOpen{ true };

        // Folders only.
        std::vector<Track> Children{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    struct Scene
    {
        std::wstring Id{};
        std::wstring Name{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    struct TempoPoint
    {
        int64_t Tick{ 0 };
        double BeatsPerMinute{ 120.0 };

        // A straight line in beats per minute from here to the next point.
        bool RampToNext{ false };

        bool operator==(TempoPoint const&) const = default;
    };

    inline constexpr double MinimumBeatsPerMinute = 10.0;
    inline constexpr double MaximumBeatsPerMinute = 999.0;

    struct MeterChange
    {
        int64_t Tick{ 0 };                      // always at the start of a bar
        uint8_t Numerator{ 4 };                 // 1 to 32
        uint8_t Denominator{ 4 };               // 1, 2, 4, 8, 16 or 32

        bool operator==(MeterChange const&) const = default;
    };

    // What follows a syllable, as RP-017 and the MIDI 2.0 Lyric Data message mark it: a space, a
    // carriage return, or a carriage return and line feed.
    enum class LyricEnd : uint8_t
    {
        None = 0,           // the word goes on in the next syllable
        Word = 1,
        Line = 2,
        Paragraph = 3,
    };

    // One sung syllable, at the tick its note starts. No text is a melisma: the syllable before
    // keeps being sung.
    struct Lyric
    {
        int64_t Tick{ 0 };
        std::wstring Text{};
        LyricEnd End{ LyricEnd::None };

        bool operator==(Lyric const&) const = default;
    };

    // The optional lyrics track (design section 23).
    struct LyricsTrack
    {
        // BCP 47, such as "en" or "ja". Empty when no one said.
        std::wstring Language{};

        // The track that sings the words: its notes place them, and it sends them as it plays.
        std::wstring TrackId{};

        // Sorted by tick.
        std::vector<Lyric> Syllables{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    struct Sequence
    {
        uint32_t FileVersion{ CurrentFileVersion };

        // The name exports carry. Starts out as the file name.
        std::wstring Name{};

        // The copyright notice exports carry, as the person wrote it.
        std::wstring Copyright{};

        std::optional<midiapp::ContentProvenance> Provenance{};

        // Sorted by tick, and always starting at tick 0.
        std::vector<TempoPoint> Tempo{ TempoPoint{} };
        std::vector<MeterChange> Meter{ MeterChange{} };

        // Tags on the Tempo and meter track: about the whole sequence.
        std::vector<Tag> Tags{};

        std::vector<Scene> Scenes{};

        // The top level, in order. Folders hold the rest.
        std::vector<Track> Tracks{};

        std::vector<Clip> Clips{};

        std::optional<LyricsTrack> Lyrics{};

        winrt::Windows::Data::Json::JsonObject Unknown{ nullptr };
    };

    // ---- finding things ----

    Track* FindTrack(_In_ Sequence& sequence, _In_ std::wstring_view id) noexcept;
    Track const* FindTrack(_In_ Sequence const& sequence, _In_ std::wstring_view id) noexcept;
    Clip* FindClip(_In_ Sequence& sequence, _In_ std::wstring_view id) noexcept;
    Clip const* FindClip(_In_ Sequence const& sequence, _In_ std::wstring_view id) noexcept;

    // Depth first, in the order the rows are drawn. Return false to stop.
    void ForEachTrack(
        _In_ Sequence const& sequence,
        _In_ std::function<bool(Track const& track, size_t depth)> const& visit);

    size_t CountTracks(_In_ Sequence const& sequence) noexcept;

    // How many timeline placements and launcher slots use this clip.
    size_t CountClipUses(_In_ Sequence const& sequence, _In_ std::wstring_view clipId) noexcept;

    // Short, readable and unique enough for a file: "t-3f2b8c1e".
    std::wstring NewId(_In_ std::wstring_view prefix);

    // Puts a sequence into the shape the rest of the app relies on: sorted notes, events, tags,
    // tempo and meter, a tempo and meter at tick 0, values in range, one slot per scene, and no
    // placement or slot naming a clip that doesn't exist. Run after reading and after importing.
    void NormalizeSequence(_Inout_ Sequence& sequence);

    void SortClip(_Inout_ Clip& clip);

    // ---- tempo and meter ----

    // Turns ticks into time and back with the tempo map, ramps included. Built once, then asked
    // many times, so the playback engine never walks the map from the start.
    class TempoMap
    {
    public:
        TempoMap() = default;
        explicit TempoMap(_In_ std::vector<TempoPoint> const& points);

        double SecondsAtTick(_In_ int64_t tick) const noexcept;
        int64_t TickAtSeconds(_In_ double seconds) const noexcept;
        double BeatsPerMinuteAtTick(_In_ int64_t tick) const noexcept;

    private:
        struct Segment
        {
            int64_t StartTick{ 0 };
            int64_t EndTick{ 0 };              // the next segment's start, or no end for the last
            double StartSeconds{ 0 };
            double StartBeatsPerMinute{ 120 };
            double EndBeatsPerMinute{ 120 };   // the same as the start when the segment holds
        };

        size_t SegmentIndexAtTick(_In_ int64_t tick) const noexcept;
        size_t SegmentIndexAtSeconds(_In_ double seconds) const noexcept;

        std::vector<Segment> m_segments{ Segment{ 0, INT64_MAX, 0, 120, 120 } };
    };

    struct BarPosition
    {
        int64_t Bar{ 1 };                       // counted from 1, the way people count bars
        int64_t Beat{ 1 };                      // counted from 1
        int64_t TicksIntoBeat{ 0 };
    };

    int64_t TicksPerBeat(_In_ MeterChange const& meter) noexcept;
    int64_t TicksPerBar(_In_ MeterChange const& meter) noexcept;
    MeterChange const& MeterAtTick(_In_ std::vector<MeterChange> const& meter, _In_ int64_t tick) noexcept;
    BarPosition BarPositionAtTick(_In_ std::vector<MeterChange> const& meter, _In_ int64_t tick) noexcept;

    // The tick where a bar starts. Bar 1 is tick 0.
    int64_t TickAtBar(_In_ std::vector<MeterChange> const& meter, _In_ int64_t bar) noexcept;

    // ---- values ----

    // MIDI 2.0 bit scaling (M2-115), min-center-max. Scaling down a value that was scaled up
    // gives back the original, so a part played on a MIDI 1.0 keyboard plays back to a MIDI 1.0
    // synth exactly as played.
    uint32_t ScaleUp(_In_ uint32_t value, _In_ uint8_t fromBits, _In_ uint8_t toBits) noexcept;
    uint32_t ScaleDown(_In_ uint32_t value, _In_ uint8_t fromBits, _In_ uint8_t toBits) noexcept;

    // Whether a note with less than 100% chance plays on this pass. Depends only on the clip's
    // seed, the pass and the note, so playing from the start always gives the same notes and an
    // export writes what you heard.
    bool NotePlaysOnPass(
        _In_ Note const& note,
        _In_ uint32_t clipSeed,
        _In_ uint32_t noteIndex,
        _In_ uint64_t pass) noexcept;

    // The number of words in a Universal MIDI Packet, from its message type.
    uint8_t UmpWordCount(_In_ uint32_t firstWord) noexcept;

    // ---- automation ----

    // A lane's value at a tick. Before its first point the lane holds the first point's value, and
    // after its last point the last one's. Nothing for a lane with no points.
    std::optional<uint32_t> LaneValueAt(_In_ AutomationLane const& lane, _In_ int64_t tick) noexcept;

    // Whether the line from a point ramps to the next point rather than holding. A program lane
    // always holds, and two points at one tick are a jump, not a ramp.
    bool LaneRampsAfter(_In_ AutomationLane const& lane, _In_ size_t pointIndex) noexcept;

    // Two lanes that change the same thing, so a clip's lane stands in for the track's.
    bool SameLaneTarget(_In_ AutomationLane const& a, _In_ AutomationLane const& b) noexcept;

    // How many bits of a lane's value a destination gets: all 32 over MIDI 2.0, and 7 or 14 over
    // MIDI 1.0. A program is 7 bits either way.
    uint8_t LaneResolutionBits(_In_ LaneTarget target, _In_ bool midi2) noexcept;

    // Lane numbers and channels in range, points in order, and an id for each lane.
    void NormalizeLanes(_Inout_ std::vector<AutomationLane>& lanes);

    // MIDI files carry UTF-8. Anything that isn't well formed comes back as U+FFFD, not a guess.
    std::string ToUtf8(_In_ std::wstring_view text);
    std::wstring FromUtf8(_In_ std::string_view bytes);
}
