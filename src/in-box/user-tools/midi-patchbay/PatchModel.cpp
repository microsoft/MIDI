// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchModel.h"
#include "StringResources.h"

namespace midipatchbay
{
    _Use_decl_annotations_
    PatchEndpoint* PatchDocument::FindEndpoint(std::wstring const& id) noexcept
    {
        auto it = std::find_if(Endpoints.begin(), Endpoints.end(),
            [&id](PatchEndpoint const& e) { return e.Id == id; });

        return it == Endpoints.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    PatchEndpoint const* PatchDocument::FindEndpoint(std::wstring const& id) const noexcept
    {
        auto it = std::find_if(Endpoints.begin(), Endpoints.end(),
            [&id](PatchEndpoint const& e) { return e.Id == id; });

        return it == Endpoints.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    PatchConnection* PatchDocument::FindConnection(std::wstring const& id) noexcept
    {
        auto it = std::find_if(Connections.begin(), Connections.end(),
            [&id](PatchConnection const& c) { return c.Id == id; });

        return it == Connections.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    PatchConnection const* PatchDocument::FindConnection(std::wstring const& id) const noexcept
    {
        auto it = std::find_if(Connections.begin(), Connections.end(),
            [&id](PatchConnection const& c) { return c.Id == id; });

        return it == Connections.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    bool PatchDocument::HasConnection(
        std::wstring const& sourceEndpointId,
        int32_t sourceGroupIndex,
        std::wstring const& destinationEndpointId,
        int32_t destinationGroupIndex) const noexcept
    {
        return std::any_of(Connections.begin(), Connections.end(),
            [&](PatchConnection const& c)
            {
                return c.SourceEndpointId == sourceEndpointId &&
                    c.SourceGroupIndex == sourceGroupIndex &&
                    c.DestinationEndpointId == destinationEndpointId &&
                    c.DestinationGroupIndex == destinationGroupIndex;
            });
    }

    _Use_decl_annotations_
    void PatchDocument::RemoveEndpoint(std::wstring const& id) noexcept
    {
        std::erase_if(Connections, [&id](PatchConnection const& c)
            { return c.SourceEndpointId == id || c.DestinationEndpointId == id; });

        std::erase_if(Endpoints, [&id](PatchEndpoint const& e) { return e.Id == id; });
    }

    _Use_decl_annotations_
    void PatchDocument::RemoveConnection(std::wstring const& id) noexcept
    {
        std::erase_if(Connections, [&id](PatchConnection const& c) { return c.Id == id; });
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
    winrt::hstring DescribeGroupIndex(int32_t groupIndex, std::wstring const& portName) noexcept
    {
        try
        {
            if (groupIndex == AllGroups)
            {
                return resources::GetString(L"PortAllGroups");
            }

            auto const groupNumber = groupIndex + 1;

            if (portName.empty())
            {
                return resources::FormatString(L"PortGroupOnlyFormat", groupNumber);
            }

            return resources::FormatString(L"PortGroupWithNameFormat", groupNumber, portName);
        }
        catch (...)
        {
            return winrt::hstring{ std::to_wstring(groupIndex + 1) };
        }
    }

    _Use_decl_annotations_
    std::optional<LiveEndpoint> ResolveEndpoint(PatchEndpoint const& endpoint) noexcept
    {
        return EndpointCatalog::Current().Resolve(endpoint.Match, endpoint.MatchMode, endpoint.DisplayName);
    }

    _Use_decl_annotations_
    std::optional<LiveEndpoint> SuggestReplacementFor(PatchEndpoint const& endpoint) noexcept
    {
        return EndpointCatalog::Current().SuggestReplacement(endpoint.Match, endpoint.MatchMode, endpoint.DisplayName);
    }
}
