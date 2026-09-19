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
    std::wstring SanitizeStoredString(std::wstring value) noexcept
    {
        if (value.size() > MaximumStringLength)
        {
            value.resize(MaximumStringLength);
        }

        // a control character in a name corrupts the display rather than saying anything
        std::erase_if(value, [](wchar_t ch) { return ch < L' '; });

        auto const first = value.find_first_not_of(L' ');

        if (first == std::wstring::npos)
        {
            return {};
        }

        auto const last = value.find_last_not_of(L' ');

        return value.substr(first, last - first + 1);
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

    namespace
    {
        // Key names come from the shipped criteria type rather than being spelled here, so the
        // patch file cannot drift from the service configuration.
        midi2config::MidiServiceConfigEndpointMatchCriteria ToCriteria(_In_ EndpointMatch const& match) noexcept
        {
            midi2config::MidiServiceConfigEndpointMatchCriteria criteria{};

            criteria.EndpointDeviceId(winrt::hstring{ match.EndpointDeviceId });
            criteria.DeviceInstanceId(winrt::hstring{ match.DeviceInstanceId });
            criteria.UsbVendorId(match.UsbVendorId);
            criteria.UsbProductId(match.UsbProductId);
            criteria.UsbSerialNumber(winrt::hstring{ match.UsbSerialNumber });
            criteria.TransportSuppliedEndpointName(winrt::hstring{ match.TransportSuppliedEndpointName });
            criteria.ParentDeviceName(winrt::hstring{ match.ParentDeviceName });

            return criteria;
        }
    }

    _Use_decl_annotations_
    json::JsonObject MatchToJson(EndpointMatch const& match) noexcept
    {
        try
        {
            auto const criteria = ToCriteria(match);

            json::JsonObject parsed{ nullptr };

            if (json::JsonObject::TryParse(criteria.GetConfigJson(), parsed) && parsed != nullptr)
            {
                return parsed;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the endpoint match object.")

        return json::JsonObject{};
    }

    _Use_decl_annotations_
    EndpointMatch MatchFromJson(json::JsonObject const& value) noexcept
    {
        EndpointMatch result{};

        try
        {
            if (value == nullptr)
            {
                return result;
            }

            auto const criteria = midi2config::MidiServiceConfigEndpointMatchCriteria::FromJson(value.Stringify());

            if (criteria == nullptr)
            {
                return result;
            }

            result.EndpointDeviceId = SanitizeStoredString(std::wstring{ criteria.EndpointDeviceId() });
            result.DeviceInstanceId = SanitizeStoredString(std::wstring{ criteria.DeviceInstanceId() });
            result.UsbVendorId = criteria.UsbVendorId();
            result.UsbProductId = criteria.UsbProductId();
            result.UsbSerialNumber = SanitizeStoredString(std::wstring{ criteria.UsbSerialNumber() });
            result.TransportSuppliedEndpointName = SanitizeStoredString(std::wstring{ criteria.TransportSuppliedEndpointName() });
            result.ParentDeviceName = SanitizeStoredString(std::wstring{ criteria.ParentDeviceName() });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read the endpoint match object.")

        return result;
    }
}
