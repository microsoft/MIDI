// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Creating virtual devices through the service, for the tests which need one. Key names are
// spelled out rather than taken from the transport's headers, so these stay an independent check
// of the wire shape.

namespace VirtualDeviceTest
{
    // {8FEAAD91-70E1-4A19-997A-377720A719C1}
    inline constexpr GUID VirtualMidiTransportId
    {
        0x8FEAAD91, 0x70E1, 0x4A19, { 0x99, 0x7A, 0x37, 0x77, 0x20, 0xA7, 0x19, 0xC1 }
    };

    inline constexpr wchar_t VirtualMidiTransportIdString[]{ L"{8FEAAD91-70E1-4A19-997A-377720A719C1}" };


    inline TransportConfigTest::ServiceConfigResult SendVirtualConfig(_In_ std::wstring const& json)
    {
        return TransportConfigTest::SendTransportConfig(VirtualMidiTransportId, VirtualMidiTransportIdString, json);
    }


    inline bool VirtualAvailable()
    {
        // an empty create array is a no-op the transport still answers
        return SendVirtualConfig(L"{\"create\":[]}").CallSucceeded;
    }


    // Both endpoints are named from the same unique id and differ only in this prefix. The SDK
    // finds the client side by association id instead, which needs device enumeration.
    inline std::wstring ClientEndpointIdFor(_In_ std::wstring const& deviceEndpointId)
    {
        constexpr std::wstring_view devicePrefix{ L"midiu_appdev_" };
        constexpr std::wstring_view clientPrefix{ L"midiu_apppub_" };

        auto const position = deviceEndpointId.find(devicePrefix);

        if (position == std::wstring::npos)
        {
            return {};
        }

        std::wstring clientEndpointId{ deviceEndpointId };

        return clientEndpointId.replace(position, devicePrefix.size(), clientPrefix);
    }


    struct CreatedDevice
    {
        std::wstring DeviceEndpointId{ };   // empty when the service did not create the device
        std::wstring ClientEndpointId{ };   // the service creates it only once the device side opens
        std::wstring Detail{ };
    };

    // There is no command to remove a virtual device. Opening its device side and closing it again
    // is what removes it, so a device which never gets that far stays until the service restarts.
    inline CreatedDevice CreateDevice(_In_ std::wstring const& name)
    {
        CreatedDevice created{ };

        auto const result = SendVirtualConfig(
            L"{\"create\":[{"
            L"\"associationIdentifier\":\"" + TransportConfigTest::MakeGuidString() + L"\","
            L"\"name\":\"" + TransportConfigTest::EscapeJsonString(name) + L"\","
            L"\"description\":\"Service test virtual device\","
            L"\"uniqueIdentifier\":\"" + TransportConfigTest::MakeUniqueIdString() + L"\""
            L"}]}");

        created.Detail = result.Message + L" " + result.ResponseJson;

        if (!result.IsSuccess())
        {
            return created;
        }

        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(result.ResponseJson, response))
        {
            return created;
        }

        auto const createdDevices = response.GetNamedArray(L"createdDevices", nullptr);

        if (createdDevices == nullptr || createdDevices.Size() == 0)
        {
            return created;
        }

        created.DeviceEndpointId = std::wstring{ createdDevices.GetObjectAt(0).GetNamedString(L"id", L"") };
        created.ClientEndpointId = ClientEndpointIdFor(created.DeviceEndpointId);

        return created;
    }
}
