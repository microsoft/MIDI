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
}
