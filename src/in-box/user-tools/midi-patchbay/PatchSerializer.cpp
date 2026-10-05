// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "PatchSerializer.h"
#include "PatchLayout.h"

#include <midi_send_pacer.h>

#include <algorithm>
#include <cmath>

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t KeyComment[] = L"_comment";
        constexpr wchar_t KeyFileVersion[] = L"fileVersion";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyDescription[] = L"description";
        constexpr wchar_t KeyCreated[] = L"created";
        constexpr wchar_t KeyModified[] = L"modified";
        constexpr wchar_t KeyActivateAtStartup[] = L"activateAtStartup";
        constexpr wchar_t KeyWaitForSendComplete[] = L"waitForSendComplete";
        constexpr wchar_t KeyEndpoints[] = L"endpoints";
        constexpr wchar_t KeyBlocks[] = L"blocks";
        constexpr wchar_t KeyConnections[] = L"connections";

        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyDisplayName[] = L"displayName";
        constexpr wchar_t KeyTransportCode[] = L"transportCode";
        constexpr wchar_t KeyMatch[] = L"match";
        constexpr wchar_t KeyMatchMode[] = L"matchMode";
        constexpr wchar_t KeyCanvasX[] = L"x";
        constexpr wchar_t KeyCanvasY[] = L"y";
        constexpr wchar_t KeyShowAllGroups[] = L"showAllGroups";

        constexpr wchar_t KeyType[] = L"type";
        constexpr wchar_t KeyBypassed[] = L"bypassed";
        constexpr wchar_t KeySettings[] = L"settings";

        constexpr wchar_t KeySource[] = L"source";
        constexpr wchar_t KeySourceGroup[] = L"sourceGroup";
        constexpr wchar_t KeyDestination[] = L"destination";
        constexpr wchar_t KeyDestinationGroup[] = L"destinationGroup";
        constexpr wchar_t KeyMuted[] = L"muted";

        // Version 1 connections.
        constexpr wchar_t KeySourceEndpoint[] = L"sourceEndpointId";
        constexpr wchar_t KeyDestinationEndpoint[] = L"destinationEndpointId";
        constexpr wchar_t KeyFilter[] = L"filter";
        constexpr wchar_t KeyTransform[] = L"transform";
        constexpr wchar_t KeySendSpeedLimit[] = L"sendSpeedLimit";

        constexpr wchar_t CommentText[] =
            L"Windows MIDI Patchbay. Written by the MIDI Patchbay app. The MIDI service does not "
            L"read this file.";

        constexpr wchar_t MatchModeDeviceId[] = L"endpointDeviceId";
        constexpr wchar_t MatchModeUsb[] = L"usbVendorAndProduct";
        constexpr wchar_t MatchModeName[] = L"endpointName";

        // Canvas coordinates are clamped rather than rejected: a nonsense value should move a
        // node back into view, not throw the whole patch away.
        constexpr double MaximumCanvasCoordinate = 100000.0;

        json::JsonValue GetValue(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (parent != nullptr && parent.HasKey(key))
                {
                    return parent.GetNamedValue(key);
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        std::wstring ReadString(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(parent, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::String)
                {
                    return SanitizeStoredString(std::wstring{ value.GetString() });
                }
            }
            catch (...)
            {
            }

            return {};
        }

        double ReadNumber(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view key,
            _In_ double defaultValue) noexcept
        {
            try
            {
                auto const value = GetValue(parent, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Number)
                {
                    auto const number = value.GetNumber();

                    if (std::isfinite(number))
                    {
                        return number;
                    }
                }
            }
            catch (...)
            {
            }

            return defaultValue;
        }

        bool ReadBool(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view key,
            _In_ bool defaultValue) noexcept
        {
            try
            {
                auto const value = GetValue(parent, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Boolean)
                {
                    return value.GetBoolean();
                }
            }
            catch (...)
            {
            }

            return defaultValue;
        }

        json::JsonObject ReadObject(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(parent, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                {
                    return value.GetObject();
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        json::JsonArray ReadArray(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            try
            {
                auto const value = GetValue(parent, key);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
                {
                    return value.GetArray();
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        int32_t ReadGroupIndex(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            auto const raw = ReadNumber(parent, key, AllGroups);

            if (raw < 0 || raw >= MaximumGroupCount)
            {
                return AllGroups;
            }

            return static_cast<int32_t>(raw);
        }

        // A multiple of MIDI 1.0 wire speed from 1 through 32. Anything else is no limit, as it is
        // for the network transports.
        uint32_t ReadSendSpeedLimit(_In_ json::JsonObject const& parent) noexcept
        {
            auto const raw = ReadNumber(parent, KeySendSpeedLimit, 0.0);

            if (raw < 1.0 || raw > static_cast<double>(::WindowsMidiServicesInternal::MidiSendSpeedMaxMultiple))
            {
                return 0;
            }

            return static_cast<uint32_t>(raw);
        }

        double ClampCoordinate(_In_ double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }

            return std::clamp(value, -MaximumCanvasCoordinate, MaximumCanvasCoordinate);
        }

        EndpointMatchMode MatchModeFromString(_In_ std::wstring const& value) noexcept
        {
            if (value == MatchModeUsb)
            {
                return EndpointMatchMode::UsbVendorAndProduct;
            }

            if (value == MatchModeName)
            {
                return EndpointMatchMode::EndpointName;
            }

            return EndpointMatchMode::EndpointDeviceId;
        }

        std::wstring MatchModeToString(_In_ EndpointMatchMode mode) noexcept
        {
            switch (mode)
            {
            case EndpointMatchMode::UsbVendorAndProduct:    return MatchModeUsb;
            case EndpointMatchMode::EndpointName:           return MatchModeName;
            default:                                        return MatchModeDeviceId;
            }
        }

        void ReadEndpoints(_In_ json::JsonObject const& root, _Inout_ PatchDocument& patch)
        {
            auto const entries = ReadArray(root, KeyEndpoints);

            if (entries == nullptr)
            {
                return;
            }

            for (auto const& entry : entries)
            {
                if (patch.Endpoints.size() >= MaximumEndpointsPerPatch)
                {
                    break;
                }

                if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const item = entry.GetObject();

                PatchEndpoint endpoint{};

                endpoint.Id = ReadString(item, KeyId);
                endpoint.DisplayName = ReadString(item, KeyDisplayName);
                endpoint.TransportCode = ReadString(item, KeyTransportCode);
                endpoint.Match = MatchFromJson(ReadObject(item, KeyMatch));
                endpoint.MatchMode = MatchModeFromString(ReadString(item, KeyMatchMode));
                endpoint.CanvasX = ClampCoordinate(ReadNumber(item, KeyCanvasX, 0.0));
                endpoint.CanvasY = ClampCoordinate(ReadNumber(item, KeyCanvasY, 0.0));
                endpoint.ShowAllGroups = ReadBool(item, KeyShowAllGroups, false);

                if (endpoint.Id.empty() || patch.FindEndpoint(endpoint.Id) != nullptr)
                {
                    continue;
                }

                patch.Endpoints.push_back(std::move(endpoint));
            }
        }

        void ReadBlocks(_In_ json::JsonObject const& root, _Inout_ PatchDocument& patch)
        {
            auto const entries = ReadArray(root, KeyBlocks);

            if (entries == nullptr)
            {
                return;
            }

            for (auto const& entry : entries)
            {
                if (patch.Blocks.size() >= MaximumBlocksPerPatch)
                {
                    break;
                }

                if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const item = entry.GetObject();

                // A kind this version does not know is left out, along with its links, rather
                // than guessed at.
                auto const kind = BlockKindFromKey(ReadString(item, KeyType));

                if (!kind.has_value())
                {
                    continue;
                }

                PatchBlock block{};

                block.Id = ReadString(item, KeyId);
                block.Kind = kind.value();
                block.Name = ReadString(item, KeyName);
                block.CanvasX = ClampCoordinate(ReadNumber(item, KeyCanvasX, 0.0));
                block.CanvasY = ClampCoordinate(ReadNumber(item, KeyCanvasY, 0.0));
                block.Bypassed = ReadBool(item, KeyBypassed, false);
                block.Settings = BlockSettingsFromJson(block.Kind, ReadObject(item, KeySettings));

                // Ids are how links find their ends, so one id has to mean one node.
                if (block.Id.empty() || patch.HasNode(block.Id))
                {
                    continue;
                }

                patch.Blocks.push_back(std::move(block));
            }
        }

        void ReadConnections(_In_ json::JsonObject const& root, _Inout_ PatchDocument& patch)
        {
            auto const entries = ReadArray(root, KeyConnections);

            if (entries == nullptr)
            {
                return;
            }

            for (auto const& entry : entries)
            {
                if (patch.Connections.size() >= MaximumConnectionsPerPatch)
                {
                    break;
                }

                if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const item = entry.GetObject();

                PatchConnection connection{};

                connection.Id = ReadString(item, KeyId);
                connection.SourceId = ReadString(item, KeySource);
                connection.DestinationId = ReadString(item, KeyDestination);
                connection.Muted = ReadBool(item, KeyMuted, false);

                // a link to something the file does not contain would draw from nowhere, so it
                // is left out rather than half drawn
                if (connection.SourceId == connection.DestinationId ||
                    !patch.HasNode(connection.SourceId) ||
                    !patch.HasNode(connection.DestinationId))
                {
                    continue;
                }

                // MIDI clock, MIDI Time Code and an annotation have no way in to draw it to.
                if (auto const* destination = patch.FindBlock(connection.DestinationId);
                    destination != nullptr && !HasInput(destination->Kind))
                {
                    continue;
                }

                // An annotation has no way out.
                if (auto const* source = patch.FindBlock(connection.SourceId);
                    source != nullptr && !HasOutput(source->Kind))
                {
                    continue;
                }

                // A block has one way in and one way out; only an endpoint end has a group.
                connection.SourceGroupIndex = patch.IsBlock(connection.SourceId)
                    ? AllGroups
                    : ReadGroupIndex(item, KeySourceGroup);

                connection.DestinationGroupIndex = patch.IsBlock(connection.DestinationId)
                    ? AllGroups
                    : ReadGroupIndex(item, KeyDestinationGroup);

                if (patch.HasConnection(
                    connection.SourceId, connection.SourceGroupIndex,
                    connection.DestinationId, connection.DestinationGroupIndex))
                {
                    continue;
                }

                if (connection.Id.empty() || patch.FindConnection(connection.Id) != nullptr)
                {
                    connection.Id = PatchDocument::NewId();
                }

                patch.Connections.push_back(std::move(connection));
            }
        }

        void ReadVersion1Connections(_In_ json::JsonObject const& root, _Inout_ PatchDocument& patch)
        {
            auto const entries = ReadArray(root, KeyConnections);

            if (entries == nullptr)
            {
                return;
            }

            struct Seen
            {
                std::wstring Source;
                int32_t SourceGroup;
                std::wstring Destination;
                int32_t DestinationGroup;
            };

            std::vector<Seen> seen{};
            size_t read{ 0 };

            for (auto const& entry : entries)
            {
                // the cap version 1 had, so a file it wrote always reads in full
                if (read >= 512)
                {
                    break;
                }

                if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const item = entry.GetObject();

                LegacyConnection legacy{};

                legacy.Id = ReadString(item, KeyId);
                legacy.SourceEndpointId = ReadString(item, KeySourceEndpoint);
                legacy.SourceGroupIndex = ReadGroupIndex(item, KeySourceGroup);
                legacy.DestinationEndpointId = ReadString(item, KeyDestinationEndpoint);
                legacy.DestinationGroupIndex = ReadGroupIndex(item, KeyDestinationGroup);
                legacy.Muted = ReadBool(item, KeyMuted, false);
                legacy.Filter = FilterFromJson(ReadObject(item, KeyFilter));
                legacy.Transform = TransformFromJson(ReadObject(item, KeyTransform));
                legacy.SendSpeedLimit = ReadSendSpeedLimit(item);

                if (patch.FindEndpoint(legacy.SourceEndpointId) == nullptr ||
                    patch.FindEndpoint(legacy.DestinationEndpointId) == nullptr)
                {
                    continue;
                }

                // Version 1 kept one connection per pair of connection points. The first one
                // read is the one it showed.
                auto const duplicate = std::any_of(seen.begin(), seen.end(), [&legacy](Seen const& s)
                {
                    return s.Source == legacy.SourceEndpointId &&
                        s.SourceGroup == legacy.SourceGroupIndex &&
                        s.Destination == legacy.DestinationEndpointId &&
                        s.DestinationGroup == legacy.DestinationGroupIndex;
                });

                if (duplicate)
                {
                    continue;
                }

                seen.push_back({ legacy.SourceEndpointId, legacy.SourceGroupIndex,
                    legacy.DestinationEndpointId, legacy.DestinationGroupIndex });

                if (legacy.Id.empty() || patch.FindConnection(legacy.Id) != nullptr)
                {
                    legacy.Id = PatchDocument::NewId();
                }

                read++;

                if (!ConvertLegacyConnection(patch, legacy))
                {
                    ConversionIssue issue{};
                    issue.Kind = ConversionIssueKind::TooLargeToConvert;
                    patch.ConversionIssues.push_back(std::move(issue));
                    break;
                }
            }
        }

        // Version 1 kept a transpose and a note map together: a listed note went where the map
        // said and every other note was transposed. As two blocks, one has to run first.
        void AddNoteBlocks(
            _In_ MessageTransform const& transform,
            _In_ BlockSettings const& start,
            _Inout_ std::vector<PatchBlock>& chain,
            _Inout_ std::vector<ConversionIssue>& issues)
        {
            auto const transpose = transform.TransposeSemitones;
            auto const hasNoteMap = CountMapEntries(transform.NoteMap.data(), transform.NoteMap.size()) > 0;

            auto addBlock = [&chain, &start, &transform](BlockKind kind) -> PatchBlock&
            {
                PatchBlock block{};
                block.Id = PatchDocument::NewId();
                block.Kind = kind;
                block.Settings = start;
                block.Settings.Transform.IgnoreExactPitchNotes = transform.IgnoreExactPitchNotes;

                chain.push_back(std::move(block));
                return chain.back();
            };

            auto addTranspose = [&addBlock, transpose]()
            {
                addBlock(BlockKind::Transpose).Settings.Transform.TransposeSemitones = transpose;
            };

            if (!hasNoteMap)
            {
                addTranspose();
                return;
            }

            auto target = [&transform](size_t note) -> int32_t
            {
                return std::clamp<int32_t>(transform.NoteMap[note], 0, 127);
            };

            if (transpose == 0)
            {
                auto& block = addBlock(BlockKind::NoteMap);

                for (size_t note = 0; note < transform.NoteMap.size(); note++)
                {
                    block.Settings.Transform.NoteMap[note] = transform.NoteMap[note] < 0
                        ? static_cast<int16_t>(-1)
                        : static_cast<int16_t>(target(note));
                }

                return;
            }

            // The map first, with each target moved back by the transpose, so the transpose that
            // follows puts it where version 1 did. That works unless a target lands off the end.
            size_t mapFirstProblems{ 0 };

            for (size_t note = 0; note < transform.NoteMap.size(); note++)
            {
                if (transform.NoteMap[note] >= 0)
                {
                    auto const moved = target(note) - transpose;

                    if (moved < 0 || moved > 127)
                    {
                        mapFirstProblems++;
                    }
                }
            }

            // Otherwise the transpose first, with each listed note moved by it. That works unless
            // a listed note moves off the end, or onto the end where unlisted notes pile up.
            size_t transposeFirstProblems{ 0 };
            bool unlistedPastTop{ false };
            bool unlistedPastBottom{ false };

            for (size_t note = 0; note < transform.NoteMap.size(); note++)
            {
                if (transform.NoteMap[note] < 0)
                {
                    auto const moved = static_cast<int32_t>(note) + transpose;

                    unlistedPastTop = unlistedPastTop || moved > 127;
                    unlistedPastBottom = unlistedPastBottom || moved < 0;
                }
            }

            for (size_t note = 0; note < transform.NoteMap.size(); note++)
            {
                if (transform.NoteMap[note] >= 0)
                {
                    auto const moved = static_cast<int32_t>(note) + transpose;

                    if (moved < 0 || moved > 127 ||
                        (moved == 127 && unlistedPastTop) ||
                        (moved == 0 && unlistedPastBottom))
                    {
                        transposeFirstProblems++;
                    }
                }
            }

            if (mapFirstProblems > 0 && transposeFirstProblems == 0)
            {
                addTranspose();

                auto& block = addBlock(BlockKind::NoteMap);

                for (size_t note = 0; note < transform.NoteMap.size(); note++)
                {
                    if (transform.NoteMap[note] >= 0)
                    {
                        block.Settings.Transform.NoteMap[static_cast<size_t>(static_cast<int32_t>(note) + transpose)] =
                            static_cast<int16_t>(target(note));
                    }
                }

                return;
            }

            auto& block = addBlock(BlockKind::NoteMap);

            for (size_t note = 0; note < transform.NoteMap.size(); note++)
            {
                if (transform.NoteMap[note] < 0)
                {
                    continue;
                }

                auto const moved = target(note) - transpose;

                if (moved < 0 || moved > 127)
                {
                    // Neither order can keep this one. Left out, so the note is transposed.
                    ConversionIssue issue{};
                    issue.Kind = ConversionIssueKind::NoteMapEntryOutOfRange;
                    issue.BlockId = block.Id;
                    issue.FromNote = static_cast<uint8_t>(note);
                    issue.ToNote = static_cast<uint8_t>(target(note));

                    issues.push_back(std::move(issue));
                    continue;
                }

                block.Settings.Transform.NoteMap[note] = static_cast<int16_t>(moved);
            }

            addTranspose();
        }
    }

    _Use_decl_annotations_
    bool ConvertLegacyConnection(PatchDocument& patch, LegacyConnection const& legacy) noexcept
    {
        try
        {
            std::vector<PatchBlock> chain{};
            std::vector<ConversionIssue> issues{};

            auto add = [&chain](BlockKind kind, BlockSettings const& settings)
            {
                PatchBlock block{};
                block.Id = PatchDocument::NewId();
                block.Kind = kind;
                block.Settings = settings;

                chain.push_back(std::move(block));
            };

            auto const& filter = legacy.Filter;

            if (filter.IsActive && !filter.PassesEverything())
            {
                auto const typesNarrowed =
                    !AllTrue(filter.MessageTypes.data(), filter.MessageTypes.size()) ||
                    !AllTrue(filter.ChannelVoiceStatuses.data(), filter.ChannelVoiceStatuses.size()) ||
                    !AllTrue(filter.SystemMessages.data(), filter.SystemMessages.size());

                if (typesNarrowed)
                {
                    auto settings = DefaultBlockSettings(BlockKind::MessageTypeFilter);
                    settings.Filter.MessageTypes = filter.MessageTypes;
                    settings.Filter.ChannelVoiceStatuses = filter.ChannelVoiceStatuses;
                    settings.Filter.SystemMessages = filter.SystemMessages;

                    add(BlockKind::MessageTypeFilter, settings);
                }

                if (!AllTrue(filter.Channels.data(), filter.Channels.size()))
                {
                    auto settings = DefaultBlockSettings(BlockKind::ChannelFilter);
                    settings.Filter.Channels = filter.Channels;

                    add(BlockKind::ChannelFilter, settings);
                }

                auto const lowest = filter.LowestAllowedNote;
                auto const highest = filter.HighestAllowedNote;

                if (filter.LimitNoteRange && (lowest > 0 || highest < 127))
                {
                    auto settings = DefaultBlockSettings(BlockKind::NoteFilter);

                    if (lowest > highest)
                    {
                        // Version 1 let no note through a range that ran backward.
                        settings.Values.Mode = ValueSetMode::List;
                        settings.Values.List.fill(false);
                    }
                    else
                    {
                        settings.Values.Mode = ValueSetMode::Range;
                        settings.Values.Lowest = lowest;
                        settings.Values.Highest = highest;
                    }

                    add(BlockKind::NoteFilter, settings);
                }
            }

            auto const& transform = legacy.Transform;

            if (transform.IsActive && !transform.ChangesNothing())
            {
                auto start = [&transform](BlockKind kind)
                {
                    auto settings = DefaultBlockSettings(kind);

                    // How values are shown, for the kinds that show one.
                    if (kind == BlockKind::Velocity || kind == BlockKind::Aftertouch || kind == BlockKind::ControlChangeValue)
                    {
                        settings.Transform.Scale = transform.Scale;
                    }

                    return settings;
                };

                // The order version 1 ran them in: the channel first, then what is addressed
                // within it.
                if (CountMapEntries(transform.ChannelMap.data(), transform.ChannelMap.size()) > 0)
                {
                    auto settings = start(BlockKind::ChannelMap);
                    settings.Transform.ChannelMap = transform.ChannelMap;

                    add(BlockKind::ChannelMap, settings);
                }

                if (transform.TransposeSemitones != 0 ||
                    CountMapEntries(transform.NoteMap.data(), transform.NoteMap.size()) > 0)
                {
                    AddNoteBlocks(transform, start(BlockKind::NoteMap), chain, issues);
                }

                if (transform.Curve != VelocityCurve::Unchanged || transform.RescaleVelocity)
                {
                    auto settings = start(BlockKind::Velocity);
                    settings.Transform.Curve = transform.Curve;
                    settings.Transform.FixedVelocityHundredths = transform.FixedVelocityHundredths;
                    settings.Transform.RescaleVelocity = transform.RescaleVelocity;
                    settings.Transform.MinimumVelocityHundredths = transform.MinimumVelocityHundredths;
                    settings.Transform.MaximumVelocityHundredths = transform.MaximumVelocityHundredths;

                    add(BlockKind::Velocity, settings);
                }

                if (!transform.AftertouchShape.ChangesNothing())
                {
                    auto settings = start(BlockKind::Aftertouch);
                    settings.Transform.AftertouchShape = transform.AftertouchShape;

                    add(BlockKind::Aftertouch, settings);
                }

                // A controller is renumbered before its bank or its value is looked at.
                if (CountMapEntries(transform.ControlMap.data(), transform.ControlMap.size()) > 0)
                {
                    auto settings = start(BlockKind::ControlChangeMap);
                    settings.Transform.ControlMap = transform.ControlMap;

                    add(BlockKind::ControlChangeMap, settings);
                }

                if (CountMapEntries(transform.ProgramMap.data(), transform.ProgramMap.size()) > 0 ||
                    CountMapEntries(transform.BankMsbMap.data(), transform.BankMsbMap.size()) > 0 ||
                    CountMapEntries(transform.BankLsbMap.data(), transform.BankLsbMap.size()) > 0)
                {
                    auto settings = start(BlockKind::ProgramMap);
                    settings.Transform.ProgramMap = transform.ProgramMap;
                    settings.Transform.BankMsbMap = transform.BankMsbMap;
                    settings.Transform.BankLsbMap = transform.BankLsbMap;

                    add(BlockKind::ProgramMap, settings);
                }

                if (std::any_of(transform.ControlValueShapes.begin(), transform.ControlValueShapes.end(),
                    [](ValueShape const& shape) { return !shape.ChangesNothing(); }))
                {
                    auto settings = start(BlockKind::ControlChangeValue);
                    settings.Transform.ControlValueShapes = transform.ControlValueShapes;

                    add(BlockKind::ControlChangeValue, settings);
                }
            }

            // Last, as in version 1: it paces what is actually sent.
            if (legacy.SendSpeedLimit != 0)
            {
                auto settings = DefaultBlockSettings(BlockKind::Throttle);
                settings.SendSpeedLimit = legacy.SendSpeedLimit;

                add(BlockKind::Throttle, settings);
            }

            if (patch.Blocks.size() + chain.size() > MaximumBlocksPerPatch ||
                patch.Connections.size() + chain.size() + 1 > MaximumConnectionsPerPatch)
            {
                return false;
            }

            std::vector<PatchConnection> links{};
            links.reserve(chain.size() + 1);

            auto sourceId = legacy.SourceEndpointId;
            auto sourceGroup = legacy.SourceGroupIndex;

            for (auto const& block : chain)
            {
                PatchConnection link{};
                link.Id = links.empty() ? legacy.Id : PatchDocument::NewId();
                link.SourceId = sourceId;
                link.SourceGroupIndex = sourceGroup;
                link.DestinationId = block.Id;
                link.DestinationGroupIndex = AllGroups;

                // Nothing goes past a muted first link, which is what a muted connection did.
                link.Muted = links.empty() && legacy.Muted;

                links.push_back(std::move(link));

                sourceId = block.Id;
                sourceGroup = AllGroups;
            }

            PatchConnection last{};
            last.Id = links.empty() ? legacy.Id : PatchDocument::NewId();
            last.SourceId = sourceId;
            last.SourceGroupIndex = sourceGroup;
            last.DestinationId = legacy.DestinationEndpointId;
            last.DestinationGroupIndex = legacy.DestinationGroupIndex;
            last.Muted = links.empty() && legacy.Muted;

            links.push_back(std::move(last));

            // Room first, so the patch is changed all at once or not at all.
            patch.Blocks.reserve(patch.Blocks.size() + chain.size());
            patch.Connections.reserve(patch.Connections.size() + links.size());
            patch.ConversionIssues.reserve(patch.ConversionIssues.size() + issues.size());

            for (auto& block : chain)
            {
                patch.Blocks.push_back(std::move(block));
            }

            for (auto& link : links)
            {
                patch.Connections.push_back(std::move(link));
            }

            for (auto& issue : issues)
            {
                patch.ConversionIssues.push_back(std::move(issue));
            }

            return true;
        }
        catch (...)
        {
        }

        return false;
    }

    _Use_decl_annotations_
    std::optional<PatchDocument> ReadPatchJson(std::wstring_view text, std::wstring const& fallbackName) noexcept
    {
        try
        {
            if (text.empty())
            {
                return std::nullopt;
            }

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ text }, root) || root == nullptr)
            {
                return std::nullopt;
            }

            PatchDocument patch{};

            patch.Name = ReadString(root, KeyName);
            patch.Description = ReadString(root, KeyDescription);
            patch.ActivateAtStartup = ReadBool(root, KeyActivateAtStartup, true);
            patch.WaitForSendComplete = ReadBool(root, KeyWaitForSendComplete, false);
            patch.CreatedTimestamp = static_cast<int64_t>(std::clamp(ReadNumber(root, KeyCreated, 0.0), 0.0, 1.0e12));
            patch.ModifiedTimestamp = static_cast<int64_t>(std::clamp(ReadNumber(root, KeyModified, 0.0), 0.0, 1.0e12));

            if (patch.Name.empty())
            {
                patch.Name = SanitizeStoredString(fallbackName);
            }

            auto const version = ReadNumber(root, KeyFileVersion, 1.0);

            patch.LoadedFileVersion = version < 2.0 ? 1 : static_cast<int32_t>((std::min)(version, 1000000.0));

            ReadEndpoints(root, patch);

            if (patch.LoadedFileVersion == 1)
            {
                ReadVersion1Connections(root, patch);

                // Blocks need room between the endpoints, so a converted patch is laid out again.
                // One without blocks keeps the layout it had.
                if (!patch.Blocks.empty())
                {
                    ArrangeInColumns(patch, nullptr);
                }
            }
            else
            {
                ReadBlocks(root, patch);
                ReadConnections(root, patch);
            }

            return patch;
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::wstring WritePatchJson(PatchDocument const& patch) noexcept
    {
        try
        {
            json::JsonObject root{};

            root.SetNamedValue(KeyComment, json::JsonValue::CreateStringValue(CommentText));
            root.SetNamedValue(KeyFileVersion, json::JsonValue::CreateNumberValue(CurrentPatchFileVersion));
            root.SetNamedValue(KeyName, json::JsonValue::CreateStringValue(patch.Name));
            root.SetNamedValue(KeyDescription, json::JsonValue::CreateStringValue(patch.Description));
            root.SetNamedValue(KeyCreated, json::JsonValue::CreateNumberValue(static_cast<double>(patch.CreatedTimestamp)));
            root.SetNamedValue(KeyModified, json::JsonValue::CreateNumberValue(static_cast<double>(patch.ModifiedTimestamp)));
            root.SetNamedValue(KeyActivateAtStartup, json::JsonValue::CreateBooleanValue(patch.ActivateAtStartup));
            root.SetNamedValue(KeyWaitForSendComplete, json::JsonValue::CreateBooleanValue(patch.WaitForSendComplete));

            json::JsonArray endpoints{};

            for (auto const& endpoint : patch.Endpoints)
            {
                json::JsonObject item{};

                item.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(endpoint.Id));
                item.SetNamedValue(KeyDisplayName, json::JsonValue::CreateStringValue(endpoint.DisplayName));
                item.SetNamedValue(KeyTransportCode, json::JsonValue::CreateStringValue(endpoint.TransportCode));
                item.SetNamedValue(KeyMatch, MatchToJson(endpoint.Match));
                item.SetNamedValue(KeyMatchMode, json::JsonValue::CreateStringValue(MatchModeToString(endpoint.MatchMode)));
                item.SetNamedValue(KeyCanvasX, json::JsonValue::CreateNumberValue(endpoint.CanvasX));
                item.SetNamedValue(KeyCanvasY, json::JsonValue::CreateNumberValue(endpoint.CanvasY));
                item.SetNamedValue(KeyShowAllGroups, json::JsonValue::CreateBooleanValue(endpoint.ShowAllGroups));

                endpoints.Append(item);
            }

            root.SetNamedValue(KeyEndpoints, endpoints);

            json::JsonArray blocks{};

            for (auto const& block : patch.Blocks)
            {
                json::JsonObject item{};

                item.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(block.Id));
                item.SetNamedValue(KeyType, json::JsonValue::CreateStringValue(BlockKindKey(block.Kind)));

                // Only a name the customer gave it. Otherwise it goes by its kind's name, in the
                // language of whoever opens the file.
                if (!block.Name.empty())
                {
                    item.SetNamedValue(KeyName, json::JsonValue::CreateStringValue(block.Name));
                }

                item.SetNamedValue(KeyCanvasX, json::JsonValue::CreateNumberValue(block.CanvasX));
                item.SetNamedValue(KeyCanvasY, json::JsonValue::CreateNumberValue(block.CanvasY));
                item.SetNamedValue(KeyBypassed, json::JsonValue::CreateBooleanValue(block.Bypassed));
                item.SetNamedValue(KeySettings, BlockSettingsToJson(block.Kind, block.Settings));

                blocks.Append(item);
            }

            root.SetNamedValue(KeyBlocks, blocks);

            json::JsonArray connections{};

            for (auto const& connection : patch.Connections)
            {
                json::JsonObject item{};

                item.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(connection.Id));
                item.SetNamedValue(KeySource, json::JsonValue::CreateStringValue(connection.SourceId));

                if (!patch.IsBlock(connection.SourceId))
                {
                    item.SetNamedValue(KeySourceGroup, json::JsonValue::CreateNumberValue(connection.SourceGroupIndex));
                }

                item.SetNamedValue(KeyDestination, json::JsonValue::CreateStringValue(connection.DestinationId));

                if (!patch.IsBlock(connection.DestinationId))
                {
                    item.SetNamedValue(KeyDestinationGroup, json::JsonValue::CreateNumberValue(connection.DestinationGroupIndex));
                }

                item.SetNamedValue(KeyMuted, json::JsonValue::CreateBooleanValue(connection.Muted));

                connections.Append(item);
            }

            root.SetNamedValue(KeyConnections, connections);

            return std::wstring{ root.Stringify() };
        }
        catch (...)
        {
        }

        return {};
    }
}
