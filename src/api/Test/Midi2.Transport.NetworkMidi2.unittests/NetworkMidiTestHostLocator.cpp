// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <sstream>

namespace NetworkMidiTest
{
    namespace
    {
        std::wstring ToLower(std::wstring const& value)
        {
            std::wstring result{ value };

            std::transform(result.begin(), result.end(), result.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return result;
        }

        std::optional<std::wstring> GetEnvironmentValue(wchar_t const* name)
        {
            wchar_t buffer[512]{ };

            auto length = GetEnvironmentVariableW(name, buffer, ARRAYSIZE(buffer));

            if (length == 0 || length >= ARRAYSIZE(buffer))
            {
                return std::nullopt;
            }

            std::wstring value{ buffer, length };

            if (value.empty())
            {
                return std::nullopt;
            }

            return value;
        }

        // Addresses this machine currently holds, used to prove an advertised host is ours even
        // when its service instance name has been customized.
        std::vector<std::wstring> GetLocalAddresses()
        {
            std::vector<std::wstring> addresses;

            try
            {
                auto hostNames = winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();

                for (auto const& hostName : hostNames)
                {
                    addresses.push_back(ToLower(std::wstring{ hostName.CanonicalName() }));
                }
            }
            catch (...)
            {
            }

            return addresses;
        }

        std::wstring GetStringProperty(
            winrt::Windows::Devices::Enumeration::DeviceInformation const& device,
            winrt::hstring const& key)
        {
            if (!device.Properties().HasKey(key))
            {
                return std::wstring{ };
            }

            auto value = device.Properties().Lookup(key);

            if (!value)
            {
                return std::wstring{ };
            }

            try
            {
                return std::wstring{ winrt::unbox_value<winrt::hstring>(value) };
            }
            catch (...)
            {
                return std::wstring{ };
            }
        }

        std::vector<std::wstring> GetStringArrayProperty(
            winrt::Windows::Devices::Enumeration::DeviceInformation const& device,
            winrt::hstring const& key)
        {
            std::vector<std::wstring> values;

            if (!device.Properties().HasKey(key))
            {
                return values;
            }

            auto value = device.Properties().Lookup(key);

            if (!value)
            {
                return values;
            }

            try
            {
                auto reference = value.as<winrt::Windows::Foundation::IReferenceArray<winrt::hstring>>();

                winrt::com_array<winrt::hstring> array;
                reference.GetStringArray(array);

                for (auto const& entry : array)
                {
                    values.push_back(std::wstring{ entry });
                }
            }
            catch (...)
            {
            }

            return values;
        }

        std::wstring GetTextAttribute(
            winrt::Windows::Devices::Enumeration::DeviceInformation const& device,
            std::wstring const& wantedKey)
        {
            auto attributes = GetStringArrayProperty(device, L"System.Devices.Dnssd.TextAttributes");

            auto lowerWanted = ToLower(wantedKey);

            for (auto const& attribute : attributes)
            {
                auto separator = attribute.find(L'=');

                if (separator == std::wstring::npos)
                {
                    continue;
                }

                if (ToLower(attribute.substr(0, separator)) == lowerWanted)
                {
                    return attribute.substr(separator + 1);
                }
            }

            return std::wstring{ };
        }
    }


    std::wstring GetLocalComputerName()
    {
        wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1]{ };
        DWORD length = ARRAYSIZE(buffer);

        if (!GetComputerNameW(buffer, &length))
        {
            return std::wstring{ };
        }

        return std::wstring{ buffer, length };
    }


    _Use_decl_annotations_
    HostLocatorResult LocateLocalServiceHost(std::chrono::milliseconds const discoveryTimeout)
    {
        HostLocatorResult result{ };
        std::wstringstream diagnostics;

        // 1. Explicit override always wins. This is the CI path.
        auto envHost = GetEnvironmentValue(EnvironmentVariableHost);
        auto envPort = GetEnvironmentValue(EnvironmentVariablePort);

        if (envHost.has_value() && envPort.has_value())
        {
            result.Address.HostNameOrAddress = envHost.value();
            result.Address.Port = static_cast<uint16_t>(_wtoi(envPort.value().c_str()));
            result.Address.DiscoveredVia = L"environment variables";

            if (result.Address.IsValid())
            {
                result.Found = true;

                diagnostics << L"Using " << EnvironmentVariableHost << L"/" << EnvironmentVariablePort
                    << L" override: " << result.Address.HostNameOrAddress << L":" << result.Address.Port;

                result.Diagnostics = diagnostics.str();

                return result;
            }

            diagnostics << L"Environment override was set but invalid. Falling back to discovery. ";
        }

        // 2. mDNS discovery, restricted to hosts which are demonstrably this machine.
        auto computerName = ToLower(GetLocalComputerName());
        auto localAddresses = GetLocalAddresses();

        diagnostics << L"Computer name: '" << computerName << L"'. Local addresses: " << localAddresses.size() << L". ";

        try
        {
            auto hosts = DiscoverMdnsServices("_midi2._udp.local", discoveryTimeout);

            result.TotalAdvertisedHostsSeen = hosts.size();

            diagnostics << L"Advertised _midi2._udp hosts seen: " << hosts.size() << L". ";

            for (auto const& host : hosts)
            {
                auto const& instanceName = host.InstanceName;
                auto const& dnsHostName = host.TargetHostName;
                auto const& addresses = host.IPv4Addresses;
                auto port = host.Port;

                bool isLocal{ false };
                std::wstring matchedBy;

                // strongest signal: it is advertising an address we hold
                for (auto const& advertised : addresses)
                {
                    for (auto const& local : localAddresses)
                    {
                        if (ToLower(advertised) == local)
                        {
                            isLocal = true;
                            matchedBy = L"local IP address " + advertised;

                            break;
                        }
                    }

                    if (isLocal)
                    {
                        break;
                    }
                }

                // the service defaults its instance name to the computer name
                if (!isLocal && !computerName.empty() && ToLower(instanceName).find(computerName) != std::wstring::npos)
                {
                    isLocal = true;
                    matchedBy = L"computer name in service instance name '" + instanceName + L"'";
                }

                if (!isLocal && !computerName.empty() && ToLower(dnsHostName).find(computerName) != std::wstring::npos)
                {
                    isLocal = true;
                    matchedBy = L"computer name in DNS host name '" + dnsHostName + L"'";
                }

                diagnostics << L"[instance='" << instanceName << L"' dns='" << dnsHostName
                    << L"' port=" << port << L" local=" << (isLocal ? L"yes" : L"no") << L"] ";

                if (!isLocal || port == 0)
                {
                    continue;
                }

                // prefer a literal address over a name, since .local resolution is unreliable
                // on networks without proper DNS
                result.Address.HostNameOrAddress = addresses.empty() ? dnsHostName : addresses.front();
                result.Address.Port = port;
                result.Address.DiscoveredVia = L"mDNS, matched by " + matchedBy;
                result.Address.AdvertisedEndpointName = host.TextAttribute(L"UMPEndpointName");
                result.Address.AdvertisedProductInstanceId = host.TextAttribute(L"ProductInstanceId");

                if (result.Address.IsValid())
                {
                    result.Found = true;

                    break;
                }
            }
        }
        catch (...)
        {
            diagnostics << L"Exception during mDNS discovery. ";
        }

        if (!result.Found)
        {
            diagnostics << L"No local Network MIDI 2.0 host found. Start a host in the MIDI service, or set "
                << EnvironmentVariableHost << L" and " << EnvironmentVariablePort << L".";
        }

        result.Diagnostics = diagnostics.str();

        return result;
    }
}
