// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>
#include <objbase.h>

#include "PatchDocument.h"

#include <algorithm>

namespace midipatchbay
{
    namespace
    {
        template <typename TItem>
        TItem* FindById(_In_ std::vector<TItem>& items, _In_ std::wstring const& id) noexcept
        {
            auto it = std::find_if(items.begin(), items.end(), [&id](TItem const& item) { return item.Id == id; });
            return it == items.end() ? nullptr : &(*it);
        }

        template <typename TItem>
        TItem const* FindById(_In_ std::vector<TItem> const& items, _In_ std::wstring const& id) noexcept
        {
            auto it = std::find_if(items.begin(), items.end(), [&id](TItem const& item) { return item.Id == id; });
            return it == items.end() ? nullptr : &(*it);
        }
    }

    _Use_decl_annotations_
    PatchEndpoint* PatchDocument::FindEndpoint(std::wstring const& id) noexcept
    {
        return FindById(Endpoints, id);
    }

    _Use_decl_annotations_
    PatchEndpoint const* PatchDocument::FindEndpoint(std::wstring const& id) const noexcept
    {
        return FindById(Endpoints, id);
    }

    _Use_decl_annotations_
    PatchBlock* PatchDocument::FindBlock(std::wstring const& id) noexcept
    {
        return FindById(Blocks, id);
    }

    _Use_decl_annotations_
    PatchBlock const* PatchDocument::FindBlock(std::wstring const& id) const noexcept
    {
        return FindById(Blocks, id);
    }

    _Use_decl_annotations_
    PatchConnection* PatchDocument::FindConnection(std::wstring const& id) noexcept
    {
        return FindById(Connections, id);
    }

    _Use_decl_annotations_
    PatchConnection const* PatchDocument::FindConnection(std::wstring const& id) const noexcept
    {
        return FindById(Connections, id);
    }

    _Use_decl_annotations_
    bool PatchDocument::HasNode(std::wstring const& id) const noexcept
    {
        return FindEndpoint(id) != nullptr || FindBlock(id) != nullptr;
    }

    _Use_decl_annotations_
    bool PatchDocument::IsBlock(std::wstring const& id) const noexcept
    {
        return FindBlock(id) != nullptr;
    }

    size_t PatchDocument::StepCount() const noexcept
    {
        return static_cast<size_t>(std::count_if(Blocks.begin(), Blocks.end(),
            [](PatchBlock const& block) { return !IsAnnotation(block.Kind); }));
    }

    bool PatchDocument::HasOscConnections() const noexcept
    {
        return OscConnectionCount() > 0;
    }

    size_t PatchDocument::OscConnectionCount() const noexcept
    {
        return static_cast<size_t>(std::count_if(Endpoints.begin(), Endpoints.end(),
            [](PatchEndpoint const& endpoint) { return endpoint.IsOsc(); }));
    }

    _Use_decl_annotations_
    std::wstring OscDeviceId(std::wstring const& patchKey, std::wstring const& nodeId)
    {
        std::wstring id{ OscDeviceIdPrefix };
        id += patchKey;
        id += L'|';
        id += nodeId;

        // The engine keys its connections by lowercased device id.
        std::transform(id.begin(), id.end(), id.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

        return id;
    }

    _Use_decl_annotations_
    bool IsOscDeviceId(std::wstring_view deviceId) noexcept
    {
        return deviceId.starts_with(OscDeviceIdPrefix);
    }

    _Use_decl_annotations_
    bool PatchDocument::HasConnection(
        std::wstring const& sourceId,
        int32_t sourceGroupIndex,
        std::wstring const& destinationId,
        int32_t destinationGroupIndex) const noexcept
    {
        return std::any_of(Connections.begin(), Connections.end(),
            [&](PatchConnection const& c)
            {
                return c.SourceId == sourceId &&
                    c.SourceGroupIndex == sourceGroupIndex &&
                    c.DestinationId == destinationId &&
                    c.DestinationGroupIndex == destinationGroupIndex;
            });
    }

    _Use_decl_annotations_
    void PatchDocument::RemoveEndpoint(std::wstring const& id) noexcept
    {
        std::erase_if(Connections, [&id](PatchConnection const& c)
            { return c.SourceId == id || c.DestinationId == id; });

        std::erase_if(Endpoints, [&id](PatchEndpoint const& e) { return e.Id == id; });
    }

    _Use_decl_annotations_
    void PatchDocument::RemoveBlock(std::wstring const& id) noexcept
    {
        std::erase_if(Connections, [&id](PatchConnection const& c)
            { return c.SourceId == id || c.DestinationId == id; });

        std::erase_if(Blocks, [&id](PatchBlock const& b) { return b.Id == id; });
    }

    _Use_decl_annotations_
    void PatchDocument::RemoveConnection(std::wstring const& id) noexcept
    {
        std::erase_if(Connections, [&id](PatchConnection const& c) { return c.Id == id; });
    }

    _Use_decl_annotations_
    double* PatchDocument::NodeX(std::wstring const& id) noexcept
    {
        if (auto* endpoint = FindEndpoint(id))
        {
            return &endpoint->CanvasX;
        }

        if (auto* block = FindBlock(id))
        {
            return &block->CanvasX;
        }

        return nullptr;
    }

    _Use_decl_annotations_
    double* PatchDocument::NodeY(std::wstring const& id) noexcept
    {
        if (auto* endpoint = FindEndpoint(id))
        {
            return &endpoint->CanvasY;
        }

        if (auto* block = FindBlock(id))
        {
            return &block->CanvasY;
        }

        return nullptr;
    }

    std::wstring PatchDocument::NewId() noexcept
    {
        GUID value{};

        if (FAILED(::CoCreateGuid(&value)))
        {
            return {};
        }

        wchar_t buffer[40]{};

        if (::StringFromGUID2(value, buffer, ARRAYSIZE(buffer)) == 0)
        {
            return {};
        }

        std::wstring result{ buffer };

        // braces only add noise inside a file the app owns end to end
        std::erase(result, L'{');
        std::erase(result, L'}');

        return result;
    }

    _Use_decl_annotations_
    int32_t DefaultWayOf(BlockKind kind) noexcept
    {
        return HasWays(kind) ? (kind == BlockKind::Branch ? BranchYesWay : SwitchOtherwiseWay) : AllGroups;
    }

    _Use_decl_annotations_
    bool IsWayOf(PatchBlock const& block, int32_t way) noexcept
    {
        if (!HasWays(block.Kind))
        {
            return way == AllGroups;
        }

        try
        {
            auto const ways = WaysOf(block.Kind, block.Settings);

            return std::find(ways.begin(), ways.end(), way) != ways.end();
        }
        catch (...)
        {
        }

        return false;
    }

    _Use_decl_annotations_
    std::vector<LogicNameUse> FindLogicNameUses(std::vector<PatchBlock> const& blocks)
    {
        std::vector<LogicNameUse> uses{};

        auto const note = [&uses](std::wstring const& rawName, bool isMemory, bool sets, std::wstring const& blockId)
            {
                auto const name = LogicNameFrom(rawName);

                if (name.empty())
                {
                    return;
                }

                auto found = std::find_if(uses.begin(), uses.end(), [&](LogicNameUse const& use)
                    {
                        return use.IsMemory == isMemory && SameLogicName(use.Name, name);
                    });

                if (found == uses.end())
                {
                    LogicNameUse use{};
                    use.Name = name;
                    use.IsMemory = isMemory;

                    uses.push_back(std::move(use));
                    found = uses.end() - 1;
                }

                auto& steps = sets ? found->SetBy : found->ReadBy;

                if (std::find(steps.begin(), steps.end(), blockId) == steps.end())
                {
                    steps.push_back(blockId);
                }
            };

        auto const read = [&note](LogicSource const& source, std::wstring const& blockId)
            {
                if (source.Kind == LogicSourceKind::Tag || source.Kind == LogicSourceKind::Memory)
                {
                    note(source.Name, source.Kind == LogicSourceKind::Memory, false, blockId);
                }
            };

        for (auto const& block : blocks)
        {
            auto const& settings = block.Settings;

            switch (block.Kind)
            {
            case BlockKind::Branch:
                read(settings.Branch.Subject, block.Id);
                break;

            case BlockKind::Switch:
                read(settings.Switch.Subject, block.Id);
                break;

            case BlockKind::SetTag:
                note(settings.SetTag.Tag, false, true, block.Id);
                read(settings.SetTag.Value, block.Id);
                break;

            case BlockKind::SetMemory:
                note(settings.SetMemory.Memory, true, true, block.Id);

                if (settings.SetMemory.Action == MemoryAction::Set)
                {
                    read(settings.SetMemory.Value, block.Id);
                }
                break;

            case BlockKind::PutValue:
                read(settings.PutValue.Value, block.Id);
                break;

            default:
                break;
            }
        }

        std::stable_sort(uses.begin(), uses.end(), [](LogicNameUse const& left, LogicNameUse const& right)
            {
                if (left.IsMemory != right.IsMemory)
                {
                    return left.IsMemory;
                }

                return ::CompareStringOrdinal(
                    left.Name.c_str(), static_cast<int>(left.Name.size()),
                    right.Name.c_str(), static_cast<int>(right.Name.size()),
                    TRUE) == CSTR_LESS_THAN;
            });

        return uses;
    }
}
