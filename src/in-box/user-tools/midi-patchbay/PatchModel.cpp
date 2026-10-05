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
    winrt::hstring DescribeGroupIndex(int32_t groupIndex, std::wstring const& groupName, bool isInput) noexcept
    {
        try
        {
            if (groupIndex == AllGroups)
            {
                return resources::GetString(isInput ? L"PortAnyGroup" : L"PortAllGroups");
            }

            auto const groupNumber = groupIndex + 1;

            if (groupName.empty())
            {
                return resources::FormatString(L"PortGroupOnlyFormat", groupNumber);
            }

            return resources::FormatString(L"PortGroupWithNameFormat", groupNumber, groupName);
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

    namespace
    {
        bool SameText(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        std::wstring const& MatchedName(_In_ PatchEndpoint const& endpoint) noexcept
        {
            return endpoint.Match.TransportSuppliedEndpointName.empty()
                ? endpoint.DisplayName
                : endpoint.Match.TransportSuppliedEndpointName;
        }
    }

    _Use_decl_annotations_
    bool IsSameDevice(PatchEndpoint const& left, PatchEndpoint const& right) noexcept
    {
        try
        {
            auto const liveLeft = ResolveEndpoint(left);
            auto const liveRight = ResolveEndpoint(right);

            // Connected, so the device each one finds is the answer, however it was matched.
            if (liveLeft.has_value() && liveRight.has_value())
            {
                return SameText(liveLeft->EndpointDeviceId, liveRight->EndpointDeviceId);
            }

            if (!left.Match.EndpointDeviceId.empty() || !right.Match.EndpointDeviceId.empty())
            {
                return SameText(left.Match.EndpointDeviceId, right.Match.EndpointDeviceId);
            }

            return left.MatchMode == right.MatchMode &&
                !MatchedName(left).empty() &&
                SameText(MatchedName(left), MatchedName(right));
        }
        catch (...)
        {
        }

        return false;
    }

    _Use_decl_annotations_
    bool StandsFor(PatchEndpoint const& endpoint, std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            if (endpointDeviceId.empty())
            {
                return false;
            }

            if (auto const live = ResolveEndpoint(endpoint))
            {
                return SameText(live->EndpointDeviceId, endpointDeviceId);
            }

            return SameText(endpoint.Match.EndpointDeviceId, endpointDeviceId);
        }
        catch (...)
        {
        }

        return false;
    }

    _Use_decl_annotations_
    winrt::hstring BlockDisplayName(PatchBlock const& block) noexcept
    {
        if (!block.Name.empty())
        {
            try
            {
                return winrt::hstring{ block.Name };
            }
            catch (...)
            {
            }
        }

        return BlockKindName(block.Kind);
    }
}
