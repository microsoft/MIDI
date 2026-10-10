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

        // Pattern and generator settings, kept exactly as written until those kinds are built.
        winrt::Windows::Data::Json::JsonObject Settings{ nullptr };

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

    struct TrackSource
    {
        EndpointRef Endpoint{};
        int8_t Group{ -1 };                     // -1 is any group
        uint16_t Channels{ AllChannels };       // one bit per channel
        bool SystemExclusive{ false };

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

    struct Sequence
    {
        uint32_t FileVersion{ CurrentFileVersion };

        // The name exports carry. Starts out as the file name.
        std::wstring Name{};

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

    // MIDI files carry UTF-8. Anything that isn't well formed comes back as U+FFFD, not a guess.
    std::string ToUtf8(_In_ std::wstring_view text);
    std::wstring FromUtf8(_In_ std::string_view bytes);
}
