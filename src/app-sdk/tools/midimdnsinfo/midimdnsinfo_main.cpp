// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#include "color.hpp"
#pragma warning(pop)

void WriteBrightLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::white(fullLabel);
}

void WriteLabel(std::string label)
{
    auto fullLabel = label + ":";
    std::cout << std::left << std::setw(25) << std::setfill(' ') << dye::grey(fullLabel);
}

init::MidiDesktopAppSdkInitializer initializer{ };

#define LINE_LENGTH 96

int __cdecl main()
{
    winrt::init_apartment();

    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" This tool is part of the Windows MIDI Services SDK and tools") << std::endl;
    std::cout << dye::aqua(" Copyright 2026- Microsoft Corporation.") << std::endl;
    std::cout << dye::aqua(" Information, license, and source available at https://aka.ms/midi") << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << dye::aqua(" Watching for new MIDI mDNS advertisements visible to this PC, on the local network. Press") << std::endl;
    std::cout << dye::aqua(" escape to exit.") << std::endl;
    std::cout << std::endl;
    std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
    std::cout << std::endl;


    auto cleanup = wil::scope_exit([&]
        {
            initializer.ShutdownSdkRuntime();
        });

    bool initialized = initializer.InitializeSdkRuntime();

    if (!initialized)
    {
        std::cout << dye::red("Unable to initialize MIDI SDK runtime.");
        return 1;
    }

    if (!initializer.EnsureServiceAvailable())
    {
        std::cout << dye::red("Unable to start MIDI service.");
        return 1;
    }


    // list host names for this PC, just as a helper

    auto hostNames = winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();

    if (hostNames.Size() > 0)
    {
        std::cout << dye::grey("Host names for this PC:") << std::endl;

        for (auto const& hostName : hostNames)
        {
            if (hostName.Type() == winrt::Windows::Networking::HostNameType::DomainName)
            {
                std::cout
                    << "  "
                    << std::left
                    << std::setw(21)                                            // max 15 for PC name or IP, and then 6 for ".local" when used
                    << dye::yellow(winrt::to_string(hostName.DisplayName()));
            }
            else if (hostName.Type() == winrt::Windows::Networking::HostNameType::Ipv4 || hostName.Type() == winrt::Windows::Networking::HostNameType::Ipv6)
            {
                std::cout
                    << "  "
                    << std::left
                    << std::setw(21)                                            // max 15 for PC name or IP, and then 6 for ".local" when used
                    << dye::purple(winrt::to_string(hostName.DisplayName()));

                if (hostName.IPInformation() != nullptr)
                {
                    uint16_t prefixLength = hostName.IPInformation().PrefixLength().Value();

                    std::cout << "/" << dye::aqua(prefixLength);

                    auto item = hostName.IPInformation().NetworkAdapter().NetworkItem();

                    if (item != nullptr)
                    {
                        auto types = item.GetNetworkTypes();

                        std::string typesText{ };

                        if (WI_AreAllFlagsSet(types, winrt::Windows::Networking::Connectivity::NetworkTypes::Internet))
                        {
                            typesText += "Internet";
                        }

                        if (WI_AreAllFlagsSet(types, winrt::Windows::Networking::Connectivity::NetworkTypes::PrivateNetwork))
                        {
                            if (!typesText.empty())
                            {
                                typesText += ", ";
                            }

                            typesText += "Private";
                        }

                        if (typesText.empty())
                        {
                            typesText = "None";
                        }

                        typesText = "(" + typesText + ")";

                        std::cout << " " << std::setw(20) << dye::green(typesText);

                    }

                    std::cout << dye::grey(" Adapter: ");

                    std::wcout
                        << internal::GuidToString(hostName.IPInformation().NetworkAdapter().NetworkAdapterId());
                }

            }

            std::wcout
                << std::endl;
        }

        std::cout << std::endl;
        std::cout << dye::grey(std::string(LINE_LENGTH, '=')) << std::endl;
        std::cout << std::endl;
    }


    auto subnet = winrt::Windows::Networking::Connectivity::NetworkInformation::GetConnectionProfiles();


    // create the watcher and start spitting out info

    auto watcher = midinet::MidiNetworkAdvertisedHostWatcher::Create();

    auto addedEventToken = watcher.Added([](auto const&, midinet::MidiNetworkAdvertisedHostAddedEventArgs const& args)
        {
            auto entry = args.AddedHost();

            std::cout << dye::grey("Id:                   ") << dye::yellow(winrt::to_string(entry.DeviceId())) << std::endl;
            std::cout << dye::grey("Name:                 ") << dye::yellow(winrt::to_string(entry.DeviceName())) << std::endl;

            //  std::wcout<< L"Protocol Id:  " << internal::GuidToString(protocolId) << std::endl;

            std::cout << dye::grey("FullName:             ") << dye::aqua(winrt::to_string(entry.FullName())) << std::endl;
            std::cout << dye::grey("InstanceName:         ") << dye::aqua(winrt::to_string(entry.ServiceInstanceName())) << std::endl;
            std::cout << dye::grey("ServiceType:          ") << dye::aqua(winrt::to_string(entry.ServiceType())) << std::endl;
            std::cout << dye::grey("Domain:               ") << dye::aqua(winrt::to_string(entry.Domain())) << std::endl;
            std::cout << dye::grey("HostName:             ") << dye::aqua(winrt::to_string(entry.HostName())) << std::endl;

            for (auto const& address : entry.IPAddresses())
            {
                std::cout << dye::grey("IP Address:           ") << dye::purple(winrt::to_string(address)) << std::endl;
            }

            if (entry.Port() != 0)
            {
                std::cout << dye::grey("Port:                 ") << dye::purple(entry.Port()) << std::endl;
            }
            else
            {
                std::cout << dye::grey("Port:                 ") << dye::red("Invalid port (0)") << std::endl;
            }

            std::cout << dye::grey("UMP Endpoint Name:    ") << dye::light_aqua(winrt::to_string(entry.UmpEndpointName())) << std::endl;
            std::cout << dye::grey("Product Instance Id:  ") << dye::light_aqua(winrt::to_string(entry.ProductInstanceId())) << std::endl;

            std::cout << dye::grey(std::string(LINE_LENGTH, '-')) << std::endl;

        });

    auto enumerationCompleteToken = watcher.EnumerationCompleted([](auto const&, foundation::IInspectable const&)
        {
            std::cout << std::endl << dye::aqua("Initial enumeration complete. Continuing to watch for new hosts. Press escape to cancel.") << std::endl << std::endl;
        });


    std::cout << std::endl << dye::aqua("Searching for Network MIDI 2.0 Hosts. Press escape to cancel.") << std::endl << std::endl;
    watcher.Start();

    // wait for escape
    while (true)
    {
        auto ch = _getch();

        if (ch == KEY_ESCAPE)
        {
            std::cout << std::endl << "Closing..." << std::endl;
            break;
        }
    }

    watcher.Stop();
    watcher.Added(addedEventToken);
    watcher.EnumerationCompleted(enumerationCompleteToken);

    return 0;
}

