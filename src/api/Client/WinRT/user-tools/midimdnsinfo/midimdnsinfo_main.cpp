// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "console_tools_shared.h"


void WriteField(_In_ std::wstring const& label, _In_ std::wstring const& value, _In_ fmt::text_style const& valueStyle)
{
    fmt::println(L"{:<22}{}",
        Styled(label, darkLabelTextStyle),
        Styled(value, valueStyle));
}


std::wstring GetNetworkTypesDescription(_In_ netconn::NetworkTypes const types)
{
    std::wstring description{ };

    if (WI_AreAllFlagsSet(types, netconn::NetworkTypes::Internet))
    {
        description += internal::ResourceGetWString(IDS_NETWORK_TYPE_INTERNET);
    }

    if (WI_AreAllFlagsSet(types, netconn::NetworkTypes::PrivateNetwork))
    {
        if (!description.empty())
        {
            description += L", ";
        }

        description += internal::ResourceGetWString(IDS_NETWORK_TYPE_PRIVATE);
    }

    if (description.empty())
    {
        description = internal::ResourceGetWString(IDS_NETWORK_TYPE_NONE);
    }

    return L"(" + description + L")";
}


void DisplayLocalHostName(_In_ net::HostName const& hostName)
{
    if (hostName.Type() == net::HostNameType::DomainName)
    {
        fmt::println(L"  {}",
            Styled(std::wstring{ hostName.DisplayName() }, highlight2TextStyle));

        return;
    }

    if (hostName.Type() != net::HostNameType::Ipv4 && hostName.Type() != net::HostNameType::Ipv6)
    {
        return;
    }

    // 15 characters covers a PC name or an IPv4 address, plus 6 for the ".local" suffix
    fmt::print(L"  {:<21}",
        Styled(std::wstring{ hostName.DisplayName() }, entityIdentifierFieldValueTextStyle));

    auto ipInformation = hostName.IPInformation();

    if (ipInformation != nullptr)
    {
        fmt::print(L"/{:<5}",
            Styled(ipInformation.PrefixLength().Value(), highlightTextStyle));

        auto networkItem = ipInformation.NetworkAdapter().NetworkItem();

        if (networkItem != nullptr)
        {
            fmt::print(L"{:<20}",
                Styled(GetNetworkTypesDescription(networkItem.GetNetworkTypes()), promptTextStyle));
        }

        fmt::print(L"{} {}",
            Styled(internal::ResourceGetWString(IDS_LABEL_ADAPTER), darkLabelTextStyle),
            Styled(internal::GuidToString(ipInformation.NetworkAdapter().NetworkAdapterId()), fieldValueTextStyle));
    }

    WriteBlankLine();
}


void DisplayLocalHostNames()
{
    auto hostNames = netconn::NetworkInformation::GetHostNames();

    if (hostNames.Size() == 0)
    {
        return;
    }

    WriteNormalLine(internal::ResourceGetWString(IDS_LABEL_LOCAL_HOST_NAMES));

    for (auto const& hostName : hostNames)
    {
        DisplayLocalHostName(hostName);
    }

    WriteBlankLine();
    WriteDoubleSeparatorLine();
    WriteBlankLine();
}


void DisplayAdvertisedHost(_In_ midinet::MidiNetworkAdvertisedHost const& host)
{
    WriteField(internal::ResourceGetWString(IDS_LABEL_DEVICE_ID), std::wstring{ host.DeviceId() }, entityIdentifierFieldValueTextStyle);
    WriteField(internal::ResourceGetWString(IDS_LABEL_DEVICE_NAME), std::wstring{ host.DeviceName() }, entityNameFieldValueTextStyle);

    WriteField(internal::ResourceGetWString(IDS_LABEL_FULL_NAME), std::wstring{ host.FullName() }, fieldValueTextStyle);
    WriteField(internal::ResourceGetWString(IDS_LABEL_SERVICE_INSTANCE_NAME), std::wstring{ host.ServiceInstanceName() }, fieldValueTextStyle);
    WriteField(internal::ResourceGetWString(IDS_LABEL_SERVICE_TYPE), std::wstring{ host.ServiceType() }, fieldValueTextStyle);
    WriteField(internal::ResourceGetWString(IDS_LABEL_DOMAIN), std::wstring{ host.Domain() }, fieldValueTextStyle);
    WriteField(internal::ResourceGetWString(IDS_LABEL_HOST_NAME), std::wstring{ host.HostName() }, fieldValueTextStyle);

    for (auto const& address : host.IPAddresses())
    {
        WriteField(internal::ResourceGetWString(IDS_LABEL_IP_ADDRESS), std::wstring{ address }, entityIdentifierFieldValueTextStyle);
    }

    if (host.Port() != 0)
    {
        fmt::println(L"{:<22}{}",
            Styled(internal::ResourceGetWString(IDS_LABEL_PORT), darkLabelTextStyle),
            Styled(host.Port(), portNumberFieldValueTextStyle));
    }
    else
    {
        WriteField(internal::ResourceGetWString(IDS_LABEL_PORT), internal::ResourceGetWString(IDS_ERROR_INVALID_PORT), errorTextStyle);
    }

    WriteField(internal::ResourceGetWString(IDS_LABEL_UMP_ENDPOINT_NAME), std::wstring{ host.UmpEndpointName() }, entityNameFieldValueTextStyle);
    WriteField(internal::ResourceGetWString(IDS_LABEL_PRODUCT_INSTANCE_ID), std::wstring{ host.ProductInstanceId() }, entityNameFieldValueTextStyle);

    WriteSingleSeparatorLine();
}


int __cdecl main()
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    winrt::init_apartment();

    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    WriteDoubleSeparatorLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_BANNER_DESCRIPTION));
    WriteBlankLine();

    DisplayLocalHostNames();

    auto watcher = midinet::MidiNetworkAdvertisedHostWatcher::Create();

    if (watcher == nullptr)
    {
        WriteErrorLine(internal::ResourceGetWString(IDS_ERROR_UNABLE_TO_CREATE_WATCHER));

        return RETURN_GENERAL_FAILURE;
    }

    auto addedEventToken = watcher.Added([](auto const&, midinet::MidiNetworkAdvertisedHostAddedEventArgs const& args)
        {
            DisplayAdvertisedHost(args.AddedHost());
        });

    auto enumerationCompleteToken = watcher.EnumerationCompleted([](auto const&, foundation::IInspectable const&)
        {
            WriteBlankLine();
            WriteHighlightLine(internal::ResourceGetWString(IDS_STATUS_ENUMERATION_COMPLETE));
            WriteBlankLine();
        });

    WriteInfoLine(internal::ResourceGetWString(IDS_STATUS_SEARCHING));
    WriteBlankLine();

    watcher.Start();

    while (_getch() != KEY_ESCAPE)
    {
        // keep waiting for the user to press escape
    }

    WriteBlankLine();
    WriteInfoLine(internal::ResourceGetWString(IDS_STATUS_CLOSING));

    watcher.Stop();
    watcher.Added(addedEventToken);
    watcher.EnumerationCompleted(enumerationCompleteToken);

    return RETURN_SUCCESS;
}

