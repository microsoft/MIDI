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

int __cdecl main()
{
    winrt::init_apartment();

    std::cout << dye::grey("===================================================================") << std::endl;
    std::cout << dye::aqua(" Enumerating MIDI mdns advertisements currently visible to this PC") << std::endl;
    std::cout << dye::grey("===================================================================") << std::endl;


    std::cout << std::endl;

    return 0;
}

