// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A patch as a document: endpoints, processing blocks and the connections between them. Pure,
// like ProcessingBlock.h, so the reader, the conversion from older files and the routing compiler
// are unit tested exactly as they ship.

#include "ProcessingBlock.h"
#include "EndpointMatch.h"
#include "ContentProvenance.h"

namespace midipatchbay
{
    using EndpointMatch = midiapp::EndpointMatch;
    using EndpointMatchMode = midiapp::EndpointMatchMode;

    using midiapp::MatchFromJson;
    using midiapp::MatchToJson;
    using midiapp::SanitizeStoredString;

    constexpr int32_t MaximumGroupCount = midiapp::MaximumGroupCount;
    constexpr size_t MaximumStringLength = midiapp::MaximumStringLength;

    // An endpoint connection point that carries every group untouched. Stored as -1 so a group
    // index and "all groups" can share one field. On an endpoint's In side it reads "Any group":
    // each message keeps the group it already has.
    constexpr int32_t AllGroups = -1;

    // The patch file version this app writes. Version 1 files are converted when they are read.
    constexpr int32_t CurrentPatchFileVersion = 2;

    // Untrusted input guards. These files live in the customer's Documents folder, which other
    // software can write to, so everything read back is bounded before it reaches the UI.
    constexpr size_t MaximumPatchFileBytes = 4 * 1024 * 1024;
    constexpr size_t MaximumEndpointsPerPatch = 64;
    constexpr size_t MaximumBlocksPerPatch = 1024;
    constexpr size_t MaximumConnectionsPerPatch = 2048;
    constexpr size_t MaximumPatchCount = 256;

    // An endpoint placed on the canvas.
    struct PatchEndpoint
    {
        std::wstring Id{};                  // stable within the patch, referenced by connections
        std::wstring DisplayName{};         // last known name, so an absent device still reads right
        std::wstring TransportCode{};
        EndpointMatch Match{};
        EndpointMatchMode MatchMode{ EndpointMatchMode::EndpointDeviceId };

        double CanvasX{ 0 };
        double CanvasY{ 0 };

        // Devices that declare nothing get one group; this opts a node into showing all sixteen.
        bool ShowAllGroups{ false };
    };

    // One processing step on the canvas.
    struct PatchBlock
    {
        std::wstring Id{};
        BlockKind Kind{ BlockKind::MessageTypeFilter };

        // Empty while the block goes by its kind's name.
        std::wstring Name{};

        double CanvasX{ 0 };
        double CanvasY{ 0 };

        // Everything goes through untouched, so a block can be compared with and without.
        bool Bypassed{ false };

        BlockSettings Settings{};
    };

    // One link. Each end is an endpoint or a block, by id. An endpoint end is one group or all
    // of them; a block has one input and one output, so its group is always AllGroups. A Branch
    // or a Switch has several outputs, and the source group of a link from one is the way it
    // leaves by: see BranchYesWay and SwitchOtherwiseWay.
    struct PatchConnection
    {
        std::wstring Id{};
        std::wstring SourceId{};
        int32_t SourceGroupIndex{ AllGroups };
        std::wstring DestinationId{};
        int32_t DestinationGroupIndex{ AllGroups };
        bool Muted{ false };
    };

    enum class ConversionIssueKind : int32_t
    {
        // A note map entry that would land outside 0 to 127 once the connection's transpose is
        // applied after it. The entry is left out, so that note is transposed instead.
        NoteMapEntryOutOfRange = 0,

        // The older file had more processing than a patch can hold as blocks. The connections
        // past that point were left out.
        TooLargeToConvert = 1,
    };

    // Something a conversion could not carry over exactly, for the customer to check.
    struct ConversionIssue
    {
        ConversionIssueKind Kind{ ConversionIssueKind::NoteMapEntryOutOfRange };
        std::wstring BlockId{};
        uint8_t FromNote{ 0 };
        uint8_t ToNote{ 0 };
    };

    // A patch is a file. Nothing in here touches WinRT UI types, so this whole layer is what a
    // future API would be built over.
    struct PatchDocument
    {
        // The app's own name for the patch while it runs, which never changes when the patch is
        // renamed or saved. Never written to the file.
        std::wstring SessionKey{};

        std::wstring Name{};
        std::wstring Description{};

        // Who made it, with what, and from what. The same block MIDI Glass writes.
        std::optional<midiapp::ContentProvenance> Provenance{};

        // Empty while the patch has never been written, which is also what makes it temporary.
        std::wstring FilePath{};

        bool IsTemporary{ false };
        bool ActivateAtStartup{ true };

        // Every send waits until the service has taken it, the way WinMM sends do. It is how
        // each device connection is opened, so it covers the whole patch.
        bool WaitForSendComplete{ false };

        // Seconds since 1970. Kept small enough to survive a JSON number exactly.
        int64_t CreatedTimestamp{ 0 };
        int64_t ModifiedTimestamp{ 0 };

        std::vector<PatchEndpoint> Endpoints{};
        std::vector<PatchBlock> Blocks{};
        std::vector<PatchConnection> Connections{};

        // The version the file was written in. Lower than the current one means the patch was
        // converted on the way in.
        int32_t LoadedFileVersion{ CurrentPatchFileVersion };
        std::vector<ConversionIssue> ConversionIssues{};

        // Written by a newer version of the app, or holding a step this version doesn't know.
        // What this version can't read would be lost if it saved the file, so it never does.
        bool IsFromNewerVersion{ false };

        // Where the app kept the file as the earlier version wrote it. Empty when it didn't
        // need to.
        std::wstring EarlierVersionPath{};

        PatchEndpoint* FindEndpoint(_In_ std::wstring const& id) noexcept;
        PatchEndpoint const* FindEndpoint(_In_ std::wstring const& id) const noexcept;

        PatchBlock* FindBlock(_In_ std::wstring const& id) noexcept;
        PatchBlock const* FindBlock(_In_ std::wstring const& id) const noexcept;

        PatchConnection* FindConnection(_In_ std::wstring const& id) noexcept;
        PatchConnection const* FindConnection(_In_ std::wstring const& id) const noexcept;

        bool HasNode(_In_ std::wstring const& id) const noexcept;
        bool IsBlock(_In_ std::wstring const& id) const noexcept;

        // The steps messages go through. Annotations are text on the canvas, not steps.
        size_t StepCount() const noexcept;

        // True when the same source point is already wired to the same destination point.
        bool HasConnection(
            _In_ std::wstring const& sourceId,
            _In_ int32_t sourceGroupIndex,
            _In_ std::wstring const& destinationId,
            _In_ int32_t destinationGroupIndex) const noexcept;

        // Removing a node removes every connection to or from it.
        void RemoveEndpoint(_In_ std::wstring const& id) noexcept;
        void RemoveBlock(_In_ std::wstring const& id) noexcept;
        void RemoveConnection(_In_ std::wstring const& id) noexcept;

        // Where a node sits, for endpoints and blocks alike. Null when there is no such node.
        double* NodeX(_In_ std::wstring const& id) noexcept;
        double* NodeY(_In_ std::wstring const& id) noexcept;

        static std::wstring NewId() noexcept;
    };

    // The source group a new link from a block starts with: Yes on a Branch, "Anything else" on a
    // Switch, and AllGroups on every other kind.
    int32_t DefaultWayOf(_In_ BlockKind kind) noexcept;

    // Whether a link from this block leaves by a way it has.
    bool IsWayOf(_In_ PatchBlock const& block, _In_ int32_t way) noexcept;
}
