// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML so this compiles into a unit test project unchanged.

#include "EndpointMatch.h"

#include <mutex>

#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>

namespace midiapp
{
    namespace
    {
        namespace mdm2config = winrt::Windows::Devices::Midi2::ServiceConfig;
        namespace mjson = winrt::Windows::Data::Json;

        std::mutex g_errorHandlerLock{};
        std::function<void(std::wstring_view)> g_errorHandler{};

        // Key names come from the shipped criteria type rather than being spelled here, so a
        // stored match cannot drift from the service configuration.
        mdm2config::MidiServiceConfigEndpointMatchCriteria ToCriteria(_In_ EndpointMatch const& match) noexcept
        {
            mdm2config::MidiServiceConfigEndpointMatchCriteria criteria{};

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
    void SetEndpointErrorHandler(std::function<void(std::wstring_view)> handler) noexcept
    {
        std::scoped_lock guard{ g_errorHandlerLock };
        g_errorHandler = std::move(handler);
    }

    _Use_decl_annotations_
    void ReportEndpointError(std::wstring_view message) noexcept
    {
        std::function<void(std::wstring_view)> handler{};

        {
            std::scoped_lock guard{ g_errorHandlerLock };
            handler = g_errorHandler;
        }

        if (handler)
        {
            try
            {
                handler(message);
            }
            catch (...)
            {
            }
        }
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
    mjson::JsonObject MatchToJson(EndpointMatch const& match) noexcept
    {
        try
        {
            auto const criteria = ToCriteria(match);

            mjson::JsonObject parsed{ nullptr };

            if (mjson::JsonObject::TryParse(criteria.GetConfigJson(), parsed) && parsed != nullptr)
            {
                return parsed;
            }
        }
        catch (...)
        {
            ReportEndpointError(L"Unable to build the endpoint match object.");
        }

        return mjson::JsonObject{};
    }

    _Use_decl_annotations_
    EndpointMatch MatchFromJson(mjson::JsonObject const& value) noexcept
    {
        EndpointMatch result{};

        try
        {
            if (value == nullptr)
            {
                return result;
            }

            auto const criteria = mdm2config::MidiServiceConfigEndpointMatchCriteria::FromJson(value.Stringify());

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
        catch (...)
        {
            ReportEndpointError(L"Unable to read the endpoint match object.");
        }

        return result;
    }
}
