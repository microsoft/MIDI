// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "color.hpp"

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


int __cdecl main()
{
    winrt::init_apartment();

    std::cout << dye::grey("===================================================================") << std::endl;
    std::cout << dye::aqua(" Enumerating MIDI mdns advertisements currently visible to this PC") << std::endl;
    std::cout << dye::grey("===================================================================") << std::endl;

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

    auto entries = midinet::MidiNetworkEndpointManager::GetAdvertisedHosts();

    if (entries.Size() > 0)
    {
        for (auto const& entry : entries)
        {
            std::cout << winrt::to_string(entry.HostName) << std::endl;
        }
    }
    else
    {
        std::cout << dye::red("No MIDI 2.0 network host MDNS advertisements found.");
    }

    std::cout << std::endl;

    return 0;
}

